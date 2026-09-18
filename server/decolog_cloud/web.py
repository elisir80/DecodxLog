"""DecoLog Cloud — il log dal browser.

Pagine servite dal server, senza un secondo progetto davanti: Jinja per il
contenuto e HTMX per le poche cose vive (la ricerca mentre si scrive, le pagine
che si allungano). Chi entra qui vede il proprio log e basta: e' la stessa
sostanza del programma, in sola lettura.

La sessione del browser e' un token come quello dei dispositivi — il server ne
tiene solo l'impronta — dentro un cookie HttpOnly.
"""

from __future__ import annotations

import datetime as dt
from pathlib import Path

from fastapi import APIRouter, Depends, Form, Request
from fastapi.responses import HTMLResponse, PlainTextResponse, RedirectResponse, Response
from fastapi.templating import Jinja2Templates
from sqlalchemy import func, select
from sqlalchemy.orm import Session

from . import auth
from .models import Account, Doc, Qso

HERE = Path(__file__).parent
templates = Jinja2Templates(directory=str(HERE / "templates"))
router = APIRouter()

COOKIE = "decolog_session"
PAGE = 50

# Quanto dura la sessione del browser: meno di un token di dispositivo, perche'
# un browser lo si usa da posti piu' distratti.
SESSION_DAYS = 30


def _account_from_cookie(request: Request, db: Session) -> Account | None:
    raw = request.cookies.get(COOKIE)
    if not raw:
        return None
    from .models import Token

    token = db.scalar(select(Token).where(Token.token_hash == auth.token_fingerprint(raw)))
    if token is None:
        return None
    if token.expires_at is not None:
        expires = token.expires_at
        if expires.tzinfo is None:
            expires = expires.replace(tzinfo=dt.UTC)
        if expires < dt.datetime.now(dt.UTC):
            return None
    return db.get(Account, token.account_id)


def _field(row: Qso, *names: str, default: str = "") -> str:
    for name in names:
        value = (row.fields or {}).get(name)
        if value:
            return str(value)
    return default


def _row_view(row: Qso) -> dict:
    """Un QSO come lo vuole la tabella: gia' leggibile, niente logica nel template."""
    date = _field(row, "QSO_DATE")
    time = _field(row, "TIME_ON")
    when = f"{date[:4]}-{date[4:6]}-{date[6:8]}" if len(date) >= 8 else ""
    return {
        "uuid": row.uuid,
        "call": row.call or _field(row, "CALL"),
        "date": when,
        "time": f"{time[:2]}:{time[2:4]}" if len(time) >= 4 else "",
        "band": row.band or _field(row, "BAND"),
        # Il sottomodo dice FT2 dove il modo direbbe solo MFSK.
        "mode": _field(row, "SUBMODE", "MODE"),
        "rst_sent": _field(row, "RST_SENT"),
        "rst_rcvd": _field(row, "RST_RCVD"),
        "grid": _field(row, "GRIDSQUARE"),
        "name": _field(row, "NAME"),
        "country": _field(row, "COUNTRY"),
        "revision": row.revision,
    }


def _filtered(db: Session, account: Account, search: str, band: str, mode: str):
    query = select(Qso).where(Qso.account_id == account.id, Qso.deleted.is_(False))
    if search:
        query = query.where(Qso.call.like(f"%{search.strip().upper()}%"))
    if band:
        query = query.where(Qso.band == band.strip().lower())
    if mode:
        query = query.where(Qso.mode_group == mode.strip().upper())
    return query.order_by(Qso.started_at.desc().nullslast(), Qso.id.desc())


# ── Accesso ───────────────────────────────────────────────────────────────────


@router.get("/", response_class=HTMLResponse)
def home(request: Request, db: Session = Depends(auth.session)):
    if _account_from_cookie(request, db) is not None:
        return RedirectResponse("/log", status_code=303)
    return templates.TemplateResponse(request, "login.html", {"error": ""})


@router.post("/login", response_class=HTMLResponse)
def login(
    request: Request,
    callsign: str = Form(...),
    password: str = Form(...),
    db: Session = Depends(auth.session),
):
    account = db.scalar(select(Account).where(Account.callsign == callsign.strip().upper()))
    if account is None or not auth.verify_password(account.password_hash, password):
        # Un solo messaggio per tutti i casi: da fuori non si deve capire quali
        # nominativi esistono.
        return templates.TemplateResponse(
            request, "login.html", {"error": "Nominativo o password non validi."}, status_code=401
        )

    raw = auth.new_token()
    from .models import Token

    db.add(
        Token(
            account_id=account.id,
            token_hash=auth.token_fingerprint(raw),
            device="browser",
            expires_at=dt.datetime.now(dt.UTC) + dt.timedelta(days=SESSION_DAYS),
        )
    )
    db.commit()

    reply = RedirectResponse("/log", status_code=303)
    reply.set_cookie(
        COOKIE,
        raw,
        max_age=SESSION_DAYS * 86400,
        httponly=True,
        samesite="lax",
        # Dietro nginx il traffico e' HTTPS; in locale il cookie deve funzionare
        # lo stesso, quindi il flag segue lo schema della richiesta.
        secure=request.url.scheme == "https",
        path="/",
    )
    return reply


@router.post("/logout")
def logout(request: Request, db: Session = Depends(auth.session)):
    raw = request.cookies.get(COOKIE)
    if raw:
        from .models import Token

        token = db.scalar(select(Token).where(Token.token_hash == auth.token_fingerprint(raw)))
        if token is not None:
            db.delete(token)
            db.commit()
    reply = RedirectResponse("/", status_code=303)
    reply.delete_cookie(COOKIE, path="/")
    return reply


# ── Il log ────────────────────────────────────────────────────────────────────


@router.get("/log", response_class=HTMLResponse)
def log(
    request: Request,
    q: str = "",
    band: str = "",
    mode: str = "",
    db: Session = Depends(auth.session),
):
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    rows = db.scalars(_filtered(db, account, q, band, mode).limit(PAGE)).all()
    total = db.scalar(
        select(func.count()).select_from(Qso).where(Qso.account_id == account.id, Qso.deleted.is_(False))
    )
    bands = db.scalars(
        select(Qso.band)
        .where(Qso.account_id == account.id, Qso.deleted.is_(False), Qso.band != "")
        .group_by(Qso.band)
        .order_by(func.count().desc())
    ).all()
    modes = db.scalars(
        select(Qso.mode_group)
        .where(Qso.account_id == account.id, Qso.deleted.is_(False), Qso.mode_group != "")
        .group_by(Qso.mode_group)
        .order_by(func.count().desc())
    ).all()

    return templates.TemplateResponse(
        request,
        "log.html",
        {
            "callsign": account.callsign,
            "rows": [_row_view(r) for r in rows],
            "total": total or 0,
            "shown": len(rows),
            "bands": bands,
            "modes": modes,
            "q": q,
            "band": band,
            "mode": mode,
            "offset": len(rows),
            "more": len(rows) == PAGE,
        },
    )


@router.get("/log/rows", response_class=HTMLResponse)
def log_rows(
    request: Request,
    q: str = "",
    band: str = "",
    mode: str = "",
    offset: int = 0,
    db: Session = Depends(auth.session),
):
    """Le sole righe: le chiede HTMX quando si cerca o si scende in fondo."""
    account = _account_from_cookie(request, db)
    if account is None:
        return HTMLResponse("", status_code=401)

    rows = db.scalars(_filtered(db, account, q, band, mode).offset(offset).limit(PAGE)).all()
    return templates.TemplateResponse(
        request,
        "_rows.html",
        {
            "rows": [_row_view(r) for r in rows],
            "q": q,
            "band": band,
            "mode": mode,
            "offset": offset + len(rows),
            "more": len(rows) == PAGE,
        },
    )


@router.get("/qso/{uuid}", response_class=HTMLResponse)
def qso(request: Request, uuid: str, db: Session = Depends(auth.session)):
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)
    row = db.scalar(select(Qso).where(Qso.account_id == account.id, Qso.uuid == uuid))
    if row is None:
        return HTMLResponse("QSO non trovato", status_code=404)

    fields = sorted((row.fields or {}).items())
    return templates.TemplateResponse(
        request,
        "qso.html",
        {
            "callsign": account.callsign,
            "qso": _row_view(row),
            "fields": fields,
            "updated": row.updated_at.strftime("%Y-%m-%d %H:%M") if row.updated_at else "",
            "device": row.device,
            "deleted": row.deleted,
        },
    )


@router.get("/station", response_class=HTMLResponse)
def station(request: Request, db: Session = Depends(auth.session)):
    """Profili stazione e impostazioni: il resto del log, quello che non e' un QSO."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    profiles = db.scalars(
        select(Doc)
        .where(Doc.account_id == account.id, Doc.kind == "profile", Doc.deleted.is_(False))
        .order_by(Doc.key)
    ).all()
    settings_doc = db.scalar(
        select(Doc).where(Doc.account_id == account.id, Doc.kind == "setting", Doc.key == "station")
    )
    values = sorted((settings_doc.data or {}).items()) if settings_doc else []

    return templates.TemplateResponse(
        request,
        "station.html",
        {
            "callsign": account.callsign,
            "profiles": [
                {
                    "key": row.key,
                    "revision": row.revision,
                    "updated": row.updated_at.strftime("%Y-%m-%d %H:%M") if row.updated_at else "",
                    "device": row.device,
                    "data": row.data or {},
                }
                for row in profiles
            ],
            "settings": values,
            "settings_revision": settings_doc.revision if settings_doc else 0,
            "settings_updated": settings_doc.updated_at.strftime("%Y-%m-%d %H:%M")
            if settings_doc and settings_doc.updated_at
            else "",
        },
    )


@router.get("/export.adi", response_class=PlainTextResponse)
def export_adif(request: Request, db: Session = Depends(auth.session)) -> Response:
    """Tutto il log in ADIF: il Cloud non e' una gabbia, i QSO si riprendono."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    rows = db.scalars(
        select(Qso)
        .where(Qso.account_id == account.id, Qso.deleted.is_(False))
        .order_by(Qso.started_at, Qso.id)
    ).all()

    out = [
        f"DecoLog Cloud — {account.callsign}",
        "<ADIF_VER:5>3.1.4",
        "<PROGRAMID:12>DecoLog Cloud",
        f"<CREATED_TIMESTAMP:15>{dt.datetime.now(dt.UTC).strftime('%Y%m%d %H%M%S')}",
        "<EOH>",
    ]
    for row in rows:
        record = []
        for name, value in (row.fields or {}).items():
            text = str(value)
            if text:
                record.append(f"<{name.upper()}:{len(text.encode('utf-8'))}>{text}")
        record.append("<EOR>")
        out.append("".join(record))

    return PlainTextResponse(
        "\n".join(out) + "\n",
        headers={"Content-Disposition": f'attachment; filename="{account.callsign}-cloud.adi"'},
        media_type="text/plain; charset=utf-8",
    )
