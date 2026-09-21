"""DecoDXLog Cloud — l'API.

Poche rotte, tutte sotto /v1, con OpenAPI generata: e' il contratto che il
client Qt di DecoDXLog implementa.

    POST /v1/auth/signup   nominativo + password -> token
    POST /v1/auth/token    nominativo + password -> token
    POST /v1/sync/push     i QSO in coda, con la revisione che il client conosceva
    GET  /v1/sync/pull     quello che e' cambiato dopo un cursore
    GET  /v1/sync/status   quanti QSO ci sono e a che punto e' il contatore
    GET  /v1/health        per il monitoraggio
"""

from __future__ import annotations

import datetime as dt
import logging
from contextlib import asynccontextmanager

from fastapi import Depends, FastAPI, HTTPException, Query, Request, status
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field
from sqlalchemy import delete, func, select
from sqlalchemy.orm import Session

from pathlib import Path

from fastapi.staticfiles import StaticFiles

from . import approval, auth, sync, web
from .models import Account, Counter, Doc, Qso, QsoHistory, create_all
from .settings import settings

@asynccontextmanager
async def lifespan(_: FastAPI):
    # Le tabelle si creano all'avvio: il servizio parte anche su un database vuoto.
    create_all()
    # uvicorn configura i propri registri e lascia stare gli altri: senza questo,
    # quello che scrive decolog — un avviso non partito, un account approvato —
    # non arriverebbe da nessuna parte, e il journal resterebbe muto.
    decolog_log = logging.getLogger("decolog")
    if not decolog_log.handlers and not logging.getLogger().handlers:
        handler = logging.StreamHandler()
        handler.setFormatter(logging.Formatter("%(levelname)s:     %(name)s %(message)s"))
        decolog_log.addHandler(handler)
    decolog_log.setLevel(logging.INFO)
    yield


# La pagina dell'API resta, ma nginx la lascia vedere solo da dentro la macchina.
app = FastAPI(
    title="DecoDXLog Cloud",
    version="1.0",
    summary="Sync del log fra i dispositivi di una stazione radioamatoriale",
    lifespan=lifespan,
)


# Come si chiamano i campi, per chi li legge invece di scriverli.
_FIELD_NAMES = {
    "callsign": "il nominativo",
    "password": "la password",
    "device": "il nome del dispositivo",
}


def _readable(error: dict) -> str:
    """Un errore di validazione detto a chi sta davanti allo schermo.

    FastAPI risponde con un elenco di oggetti: giusto per un programma, illeggibile
    per una persona — e un client che si aspetta una frase si ritrova con
    "status code 422" e nessuna idea di cosa fare.
    """
    field = next((str(p) for p in reversed(error.get("loc", [])) if p != "body"), "")
    name = _FIELD_NAMES.get(field, field or "il dato")
    context = error.get("ctx") or {}
    kind = error.get("type", "")

    if kind == "missing":
        return f"manca {name}"
    if kind == "string_too_short":
        return f"{name} deve avere almeno {context.get('min_length', '?')} caratteri"
    if kind == "string_too_long":
        return f"{name} non puo' superare i {context.get('max_length', '?')} caratteri"
    return f"{name}: {error.get('msg', 'valore non valido')}"


@app.exception_handler(RequestValidationError)
async def say_it_in_words(_: Request, exc: RequestValidationError) -> JSONResponse:
    """Il 422 con una frase dentro `detail`, dove i client la cercano."""
    reasons = [_readable(error) for error in exc.errors()]
    # 422: il numero resta quello di sempre, cambia solo che dentro c'e' una frase.
    return JSONResponse(status_code=422,
                        content={"detail": "; ".join(reasons) or "richiesta non valida"})


# ── Modelli ───────────────────────────────────────────────────────────────────


class Credentials(BaseModel):
    # Il nominativo e' quello che e': 9H1SR, VY2XT, 9H1SR/M, un indicativo
    # speciale di due lettere. Non c'e' una lunghezza minima che valga per tutti
    # i paesi del mondo, quindi non se ne mette una: basta che ci sia.
    callsign: str = Field(min_length=1, max_length=32)
    # La password invece e' una scelta, e otto caratteri sono il minimo serio.
    password: str = Field(min_length=8, max_length=200)
    device: str = Field(default="", max_length=120)


class TokenOut(BaseModel):
    token: str
    callsign: str


class QsoIn(BaseModel):
    uuid: str
    revision: int = 1
    deleted: bool = False
    manual: bool = False
    call: str = ""
    band: str = ""
    mode: str = ""
    submode: str = ""
    startedAt: str | None = None
    fields: dict = Field(default_factory=dict)


class DocIn(BaseModel):
    # Un documento del log che non e' un QSO: profilo stazione, impostazione,
    # filtro salvato, regola d'avviso.
    kind: str
    key: str
    revision: int = 1
    deleted: bool = False
    data: dict = Field(default_factory=dict)


class PushIn(BaseModel):
    device: str = ""
    qsos: list[QsoIn] = Field(default_factory=list)
    docs: list[DocIn] = Field(default_factory=list)


class PushOut(BaseModel):
    results: list[dict]
    docResults: list[dict] = Field(default_factory=list)
    cursor: int


class PullOut(BaseModel):
    qsos: list[dict]
    docs: list[dict] = Field(default_factory=list)
    cursor: int
    more: bool


class StatusOut(BaseModel):
    callsign: str
    qsos: int
    deleted: int
    cursor: int


# ── Autenticazione ────────────────────────────────────────────────────────────


@app.post("/v1/auth/signup", response_model=TokenOut)
def signup(body: Credentials, request: Request, db: Session = Depends(auth.session)) -> TokenOut:
    if not settings.allow_signup:
        raise HTTPException(status.HTTP_403_FORBIDDEN, "registrazione chiusa su questo server")
    callsign = body.callsign.strip().upper()
    if db.scalar(select(Account).where(Account.callsign == callsign)):
        # Chi esiste gia' non fa partire nessun avviso: se no bastava provare
        # cinquanta nominativi noti per riempire una casella.
        raise HTTPException(status.HTTP_409_CONFLICT, "nominativo gia' registrato")
    ip = approval.client_ip(request)
    # Con l'approvazione accesa l'account nasce in attesa: il token glielo diamo
    # lo stesso, cosi' il programma non resta a meta', ma il sync dice di no
    # finche' qualcuno non ha detto di si'.
    account = Account(
        callsign=callsign,
        password_hash=auth.hash_password(body.password),
        approved=not settings.approval_required,
        approval_token=approval.new_token() if settings.approval_required else "",
        signup_ip=ip,
    )
    db.add(account)
    db.commit()
    if settings.approval_required:
        approval.notify(account, ip)
    return TokenOut(token=auth.issue_token(db, account, body.device), callsign=callsign)


@app.post("/v1/auth/token", response_model=TokenOut)
def token(body: Credentials, db: Session = Depends(auth.session)) -> TokenOut:
    callsign = body.callsign.strip().upper()
    account = db.scalar(select(Account).where(Account.callsign == callsign))
    # Stessa risposta per nominativo sconosciuto e password sbagliata: da fuori
    # non si deve capire quali nominativi esistono.
    if account is None or not auth.verify_password(account.password_hash, body.password):
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "nominativo o password non validi")
    return TokenOut(token=auth.issue_token(db, account, body.device), callsign=callsign)


# ── Sync ──────────────────────────────────────────────────────────────────────


@app.post("/v1/sync/push", response_model=PushOut)
def push(
    body: PushIn,
    account: Account = Depends(auth.current_account),
    db: Session = Depends(auth.session),
) -> PushOut:
    if len(body.qsos) + len(body.docs) > settings.max_batch:
        raise HTTPException(status.HTTP_413_REQUEST_ENTITY_TOO_LARGE, "troppa roba in una volta")
    results = [sync.apply_push(db, account, record.model_dump(), body.device) for record in body.qsos]
    docs = [sync.apply_doc(db, account, record.model_dump(), body.device) for record in body.docs]
    db.commit()
    counter = db.get(Counter, account.id)
    return PushOut(results=results, docResults=docs, cursor=counter.value if counter else 0)


@app.get("/v1/sync/pull", response_model=PullOut)
def pull(
    since: int = Query(default=0, ge=0),
    limit: int = Query(default=0, ge=0),
    account: Account = Depends(auth.current_account),
    db: Session = Depends(auth.session),
) -> PullOut:
    size = min(limit or settings.page_size, settings.page_size)
    records, cursor, more = sync.pull(db, account, since, size)
    # I documenti sono pochi e cambiano di rado: stanno nella stessa pagina,
    # fino al punto dove sono arrivati i QSO.
    docs = sync.pull_docs(db, account, since, size + 1)
    docs_truncated = len(docs) > size
    docs = docs[:size]
    if more:
        # La pagina dei QSO si e' fermata prima: i documenti oltre quel punto
        # arrivano col giro dopo, altrimenti il cursore li salterebbe.
        docs = [d for d in docs if d["seq"] <= cursor]
    elif docs:
        # Niente altro da leggere: il cursore va dove sono arrivati tutti e due.
        cursor = max(cursor, max(d["seq"] for d in docs))
        more = docs_truncated
    return PullOut(qsos=records, docs=docs, cursor=cursor, more=more)


@app.get("/v1/sync/status", response_model=StatusOut)
def sync_status(
    account: Account = Depends(auth.current_account),
    db: Session = Depends(auth.session),
) -> StatusOut:
    total = db.scalar(
        select(func.count()).select_from(Qso).where(Qso.account_id == account.id, Qso.deleted.is_(False))
    )
    gone = db.scalar(
        select(func.count()).select_from(Qso).where(Qso.account_id == account.id, Qso.deleted.is_(True))
    )
    counter = db.get(Counter, account.id)
    return StatusOut(
        callsign=account.callsign,
        qsos=int(total or 0),
        deleted=int(gone or 0),
        cursor=counter.value if counter else 0,
    )


class PurgeIn(BaseModel):
    """Per cancellare tutto bisogna scriverlo: la parola e' DELETE."""

    confirm: str = Field(default="", max_length=32)


@app.post("/v1/account/purge")
def purge(
    body: PurgeIn,
    account: Account = Depends(auth.current_account),
    db: Session = Depends(auth.session),
) -> dict:
    """Svuota il Cloud di questo nominativo: QSO, storico, documenti, presenze.

    L'account resta — nominativo, password e dispositivi collegati non si
    toccano —: quello che sparisce e' il log. Non e' una cancellazione morbida:
    qui le righe se ne vanno davvero, ed e' per questo che si deve scrivere
    DELETE. Il cursore torna a zero, cosi' chi sincronizza dopo riparte da capo.
    """
    from .models import DocHistory, Presence

    if body.confirm.strip() != "DELETE":
        raise HTTPException(status_code=400, detail="Per cancellare tutto scrivi DELETE.")

    counts = {}
    for name, model, column in (("qsos", Qso, Qso.account_id),
                                ("history", QsoHistory, QsoHistory.account_id),
                                ("docs", Doc, Doc.account_id),
                                ("docHistory", DocHistory, DocHistory.account_id),
                                ("presence", Presence, Presence.account_id)):
        counts[name] = int(db.scalar(select(func.count()).select_from(model).where(column == account.id)) or 0)
        db.execute(delete(model).where(column == account.id))

    counter = db.get(Counter, account.id)
    if counter is not None:
        counter.value = 0
    db.commit()
    return {"ok": True, "deleted": counts}


class PresenceIn(BaseModel):
    """Dov'e' la stazione adesso. Tutto facoltativo: quello che si sa, si dice."""

    device: str = Field(default="", max_length=120)
    frequencyHz: int = Field(default=0, ge=0, le=300_000_000_000)
    band: str = Field(default="", max_length=16)
    mode: str = Field(default="", max_length=32)
    dxCall: str = Field(default="", max_length=32)
    transmitting: bool = False
    client: str = Field(default="", max_length=64)


@app.post("/v1/presence")
def presence(
    body: PresenceIn,
    account: Account = Depends(auth.current_account),
    db: Session = Depends(auth.session),
) -> dict:
    """La frequenza di adesso, non un pezzo di log.

    Si riscrive sopra alla riga di quel dispositivo: niente storia, niente
    revisioni, e soprattutto **niente cursore** — un giro di VFO non deve
    svegliare gli altri dispositivi come fa un QSO.
    """
    from .models import Presence

    row = db.scalar(
        select(Presence).where(Presence.account_id == account.id, Presence.device == body.device)
    )
    if row is None:
        row = Presence(account_id=account.id, device=body.device)
        db.add(row)

    row.frequency_hz = body.frequencyHz
    row.band = body.band.strip().lower()
    row.mode = body.mode.strip().upper()
    row.dx_call = body.dxCall.strip().upper()
    row.transmitting = body.transmitting
    row.client = body.client.strip()
    row.updated_at = dt.datetime.now(dt.UTC)
    db.commit()
    return {"ok": True}


@app.get("/v1/health")
def health() -> dict:
    # `features` dice cosa sa fare *questo* servizio, non cosa dovrebbe saper
    # fare: un aggiornamento a meta' si vede da qui, e update.sh se ne accorge
    # invece di lasciare in piedi una versione vecchia che risponde "ok".
    #   qso     — push e pull dei collegamenti
    #   docs    — profili stazione, impostazioni, credenziali sigillate
    #   web     — il log dal browser
    #   stats   — statistiche, diplomi, QSL e mappa calcolati dal log
    #   contest — punteggio del contest, nel log dal browser
    #   purge   — svuotare il Cloud di un nominativo (/v1/account/purge)
    return {"status": "ok", "service": "decolog-cloud", "version": app.version,
            "features": ["qso", "docs", "web", "stats", "contest", "purge"]}


# ── Il log dal browser ────────────────────────────────────────────────────────
# Le pagine stanno in coda alle rotte /v1, cosi' l'API resta il contratto e la
# web UI e' quello che ci si appoggia sopra.
app.mount("/static", StaticFiles(directory=str(Path(__file__).parent / "static")), name="static")
# La pagina che decide chi entra sta prima del sito: e' un collegamento che
# arriva per email, non una cosa che si naviga.
app.include_router(approval.router)
app.include_router(web.router)
