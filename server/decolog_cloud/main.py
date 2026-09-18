"""DecoLog Cloud — l'API.

Poche rotte, tutte sotto /v1, con OpenAPI generata: e' il contratto che il
client Qt di DecoLog implementa.

    POST /v1/auth/signup   nominativo + password -> token
    POST /v1/auth/token    nominativo + password -> token
    POST /v1/sync/push     i QSO in coda, con la revisione che il client conosceva
    GET  /v1/sync/pull     quello che e' cambiato dopo un cursore
    GET  /v1/sync/status   quanti QSO ci sono e a che punto e' il contatore
    GET  /v1/health        per il monitoraggio
"""

from __future__ import annotations

from contextlib import asynccontextmanager

from fastapi import Depends, FastAPI, HTTPException, Query, status
from pydantic import BaseModel, Field
from sqlalchemy import func, select
from sqlalchemy.orm import Session

from pathlib import Path

from fastapi.staticfiles import StaticFiles

from . import auth, sync, web
from .models import Account, Counter, Qso, create_all
from .settings import settings

@asynccontextmanager
async def lifespan(_: FastAPI):
    # Le tabelle si creano all'avvio: il servizio parte anche su un database vuoto.
    create_all()
    yield


# La pagina dell'API resta, ma nginx la lascia vedere solo da dentro la macchina.
app = FastAPI(
    title="DecoLog Cloud",
    version="1.0",
    summary="Sync del log fra i dispositivi di una stazione radioamatoriale",
    lifespan=lifespan,
)


# ── Modelli ───────────────────────────────────────────────────────────────────


class Credentials(BaseModel):
    callsign: str = Field(min_length=3, max_length=32)
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
def signup(body: Credentials, db: Session = Depends(auth.session)) -> TokenOut:
    if not settings.allow_signup:
        raise HTTPException(status.HTTP_403_FORBIDDEN, "registrazione chiusa su questo server")
    callsign = body.callsign.strip().upper()
    if db.scalar(select(Account).where(Account.callsign == callsign)):
        raise HTTPException(status.HTTP_409_CONFLICT, "nominativo gia' registrato")
    account = Account(callsign=callsign, password_hash=auth.hash_password(body.password))
    db.add(account)
    db.commit()
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


@app.get("/v1/health")
def health() -> dict:
    # `features` dice cosa sa fare *questo* servizio, non cosa dovrebbe saper
    # fare: un aggiornamento a meta' si vede da qui, e update.sh se ne accorge
    # invece di lasciare in piedi una versione vecchia che risponde "ok".
    #   qso   — push e pull dei collegamenti
    #   docs  — profili stazione, impostazioni, credenziali sigillate
    #   web   — il log dal browser
    #   stats — statistiche, diplomi, QSL e mappa calcolati dal log
    return {"status": "ok", "service": "decolog-cloud", "version": app.version,
            "features": ["qso", "docs", "web", "stats"]}


# ── Il log dal browser ────────────────────────────────────────────────────────
# Le pagine stanno in coda alle rotte /v1, cosi' l'API resta il contratto e la
# web UI e' quello che ci si appoggia sopra.
app.mount("/static", StaticFiles(directory=str(Path(__file__).parent / "static")), name="static")
app.include_router(web.router)
