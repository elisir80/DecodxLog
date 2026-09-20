"""DecoDXLog Cloud — il log dal browser.

Non una pagina web che parla dello stesso log: **la stessa finestra**. Barra
superiore a blocchi, tre colonne di pannelli (scheda del QSO, log, scheda del
nominativo con FT2 Award e mappa), le schede in basso — Diplomi, Statistiche,
Invio QSL, Registro attivita', DX Cluster — e la barra di stato. Anche i colori
sono quelli della stazione: il tema arriva con le impostazioni sincronizzate
(vedi theme.py).

Cambia una cosa sola, ed e' voluta: da qui si guarda e si scarica, si scrive dal
programma.

Pagine servite dal server, senza un secondo progetto davanti: Jinja per il
contenuto e HTMX per le poche cose vive (la ricerca mentre si scrive, le pagine
che si allungano).

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

from . import analytics, auth, solar as solar_source, sync, theme as theming
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
    """Un QSO come lo vuole la tabella: gia' leggibile, niente logica nel template.

    Le colonne sono quelle del log di DecoDXLog, nello stesso ordine — fino alle
    QSL per servizio (L Q C E) e alle etichette.
    """
    date = _field(row, "QSO_DATE")
    time = _field(row, "TIME_ON")
    when = f"{date[:4]}-{date[4:6]}-{date[6:8]}" if len(date) >= 8 else ""

    def yes(name: str) -> bool:
        return _field(row, name).upper().startswith("Y")

    freq = _field(row, "FREQ")
    return {
        "uuid": row.uuid,
        "call": row.call or _field(row, "CALL"),
        "date": when,
        "time": f"{time[:2]}:{time[2:4]}" if len(time) >= 4 else "",
        "band": row.band or _field(row, "BAND"),
        # In MHz con tre decimali, come nella colonna del programma.
        "freq": f"{float(freq):.3f}" if freq.replace(".", "", 1).isdigit() else freq,
        # Il sottomodo dice FT2 dove il modo direbbe solo MFSK.
        "mode": _field(row, "SUBMODE", "MODE"),
        "rst_sent": _field(row, "RST_SENT"),
        "rst_rcvd": _field(row, "RST_RCVD"),
        "grid": _field(row, "GRIDSQUARE"),
        "name": _field(row, "NAME"),
        "country": _field(row, "COUNTRY"),
        "qsl": {"lotw": yes("LOTW_QSL_RCVD"), "qrz": yes("QRZCOM_QSO_DOWNLOAD_STATUS"),
                "card": yes("QSL_RCVD"), "eqsl": yes("EQSL_QSL_RCVD")},
        "tags": _field(row, "APP_DECOLOG_TAGS"),
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
def log(request: Request, db: Session = Depends(auth.session)):
    """Il log: la finestra con la scheda in basso predefinita, come all'avvio."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    rows = _all_rows(db, account)
    data = analytics.statistics(rows)
    return _page(request, db, account, "statistiche",
                 s=data, mode="", year=0, years=data["all_years"], groups=_MODE_GROUPS)


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


# Le schede in basso sono quelle del programma, nello stesso ordine.
TABS = [
    ("diplomi", "Diplomi", "/awards"),
    ("statistiche", "Statistiche", "/stats"),
    ("qsl", "Invio QSL", "/qsl"),
    ("attivita", "Registro attività", "/activity"),
    ("contest", "Contest", "/contest"),
    ("cluster", "DX Cluster", "/cluster"),
    ("propagazione", "Propagazione", "/propagation"),
]


def _settings_doc(db: Session, account: Account) -> dict:
    row = db.scalar(
        select(Doc).where(Doc.account_id == account.id, Doc.kind == "setting", Doc.key == "station")
    )
    return (row.data or {}) if row else {}


def _window(request: Request, db: Session, account: Account, tab: str,
            data_rows: list[analytics.Row] | None = None, **extra) -> dict:
    """Quello che c'e' in ogni schermata: testata, colonne laterali, barra di stato.

    Le colonne di destra e di sinistra non cambiano da una scheda all'altra,
    esattamente come nella finestra del programma.
    """
    rows = _all_rows(db, account) if data_rows is None else data_rows

    settings = _settings_doc(db, account)
    # Il QSO scelto: quello indicato nell'indirizzo, altrimenti l'ultimo fatto.
    wanted = request.query_params.get("sel", "")
    chosen = None
    if wanted:
        chosen = db.scalar(select(Qso).where(Qso.account_id == account.id, Qso.uuid == wanted))
    if chosen is None:
        chosen = db.scalar(
            select(Qso)
            .where(Qso.account_id == account.id, Qso.deleted.is_(False))
            .order_by(Qso.started_at.desc().nullslast(), Qso.id.desc())
            .limit(1)
        )

    # FT2 Award, come il riquadro nella colonna di destra del programma.
    ft2 = analytics.awards(rows, mode_group="FT2")
    ft2_dxcc = next(a for a in ft2 if a.id == "dxcc")
    ft2_grids = next(a for a in ft2 if a.id == "grids")
    ft2_lotw = sum(1 for r in rows if r.submode == "FT2" and r.confirmed_lotw)

    stats = analytics.statistics(rows)
    points = analytics.grid_points(rows)

    return {
        "request": request,
        "callsign": account.callsign,
        "theme": theming.theme_of(settings),
        "tabs": TABS,
        "tab": tab,
        "total": len(rows),
        "entities": stats["entities"],
        "grids": stats["grids"],
        "last_sync": _last_change(db, account),
        "air": _on_air(db, account),
        "selected": _detail_view(chosen) if chosen is not None else None,
        "worked": _worked_before(rows, chosen.call if chosen is not None else ""),
        "ft2": {"dxcc": ft2_dxcc.worked, "grids": ft2_grids.worked, "confirmed": ft2_lotw},
        "points": points,
        "map_points": points[:400],
        **extra,
    }


# Dopo quanto una stazione che non parla si considera spenta.
PRESENCE_FRESH = dt.timedelta(minutes=3)


def _on_air(db: Session, account: Account) -> dict:
    """Dov'e' la stazione adesso, se l'ha detto di recente.

    La pillola in alto a sinistra e' quella del programma: li' c'e' la frequenza
    che si sta ascoltando. Qui la si mostra se e' fresca; se il computer di casa
    tace da qualche minuto, torna a essere trattini.
    """
    from .models import Presence

    row = db.scalar(
        select(Presence)
        .where(Presence.account_id == account.id)
        .order_by(Presence.updated_at.desc())
        .limit(1)
    )
    if row is None or not row.updated_at:
        return {}

    when = row.updated_at if row.updated_at.tzinfo else row.updated_at.replace(tzinfo=dt.UTC)
    age = dt.datetime.now(dt.UTC) - when
    mhz = row.frequency_hz / 1_000_000 if row.frequency_hz else 0.0
    return {
        "live": age < PRESENCE_FRESH,
        "frequency": f"{mhz:10.6f}".strip() if mhz else "",
        "band": row.band,
        "mode": row.mode,
        "dxCall": row.dx_call,
        "transmitting": row.transmitting,
        "client": row.client,
        "device": row.device,
        "when": when.strftime("%Y-%m-%d %H:%M"),
        "minutes": int(age.total_seconds() // 60),
    }


def _last_change(db: Session, account: Account) -> str:
    when = db.scalar(
        select(func.max(Qso.updated_at)).where(Qso.account_id == account.id)
    )
    return when.strftime("%Y-%m-%d %H:%M") if when else ""


def _detail_view(row: Qso) -> dict:
    """Il QSO come lo mostra la colonna di sinistra: i campi che si guardano."""
    view = _row_view(row)
    view["fields"] = sorted((row.fields or {}).items())
    view["country"] = _field(row, "COUNTRY")
    view["comment"] = _field(row, "COMMENT", "NOTES")
    view["qth"] = _field(row, "QTH")
    view["freq"] = _field(row, "FREQ")
    view["tx_pwr"] = _field(row, "TX_PWR")
    view["operator"] = _field(row, "STATION_CALLSIGN", "OPERATOR")
    return view


def _worked_before(rows: list[analytics.Row], call: str) -> dict:
    """La scheda del nominativo: quante volte, su cosa, e quando."""
    call = (call or "").upper()
    mine = [r for r in rows if r.call == call]
    if not mine:
        return {}
    bands = sorted({r.band for r in mine if r.band},
                   key=lambda b: analytics.BAND_ORDER.index(b) if b in analytics.BAND_ORDER else 99)
    times = [r.when for r in mine if r.when]
    return {
        "call": call,
        "qsos": len(mine),
        "bands": bands,
        "modes": sorted({r.label_mode for r in mine if r.label_mode}),
        "country": next((r.country for r in mine if r.country), ""),
        "continent": next((r.continent for r in mine if r.continent), ""),
        "grid": next((r.grid for r in mine if r.grid), ""),
        "first": min(times).strftime("%Y-%m-%d") if times else "",
        "last": max(times).strftime("%Y-%m-%d") if times else "",
        "confirmed": any(r.confirmed_lotw or r.confirmed_card or r.confirmed_eqsl for r in mine),
    }


def _all_rows(db: Session, account: Account) -> list[analytics.Row]:
    """Tutto il log, ridotto a quello che serve ai conti.

    Si legge per intero perche' i diplomi e le statistiche guardano ogni QSO:
    e' la stessa cosa che fa DecoDXLog sul computer, e un log di stazione sta in
    memoria senza fatica.
    """
    rows = db.scalars(
        select(Qso)
        .where(Qso.account_id == account.id, Qso.deleted.is_(False))
        .order_by(Qso.started_at, Qso.id)
    ).all()
    return [analytics.row_of(r) for r in rows]


# I gruppi di modi come li offre DecoDXLog nei filtri dei diplomi.
_MODE_GROUPS = [("", "tutti i modi"), ("FT2", "FT2"), ("FT8", "FT8"),
                ("DIGITAL", "digitali"), ("CW", "CW"), ("PHONE", "fonia")]


def _log_center(db: Session, account: Account, q: str, band: str, mode: str) -> dict:
    """Il pannello di mezzo: la tabella del log con i suoi filtri."""
    rows = db.scalars(_filtered(db, account, q, band, mode).limit(PAGE)).all()
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
    return {
        "rows": [_row_view(r) for r in rows],
        "bands": bands,
        "modes": modes,
        "q": q, "band": band, "mode_filter": mode,
        "offset": len(rows),
        "more": len(rows) == PAGE,
    }


def _page(request: Request, db: Session, account: Account, tab: str, **extra):
    """Una schermata: la finestra di sempre, con la scheda in basso che cambia."""
    rows = _all_rows(db, account)
    q = request.query_params.get("q", "")
    band = request.query_params.get("band", "")
    mode_filter = request.query_params.get("fmode", "")
    context = _window(request, db, account, tab, data_rows=rows,
                      **_log_center(db, account, q, band, mode_filter), **extra)
    return templates.TemplateResponse(request, "window.html", context)


@router.get("/stats", response_class=HTMLResponse)
def stats(request: Request, mode: str = "", year: int = 0, db: Session = Depends(auth.session)):
    """Statistiche: la stessa scheda in basso della finestra del programma."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    rows = _all_rows(db, account)
    data = analytics.statistics(rows, mode_group=mode, year=year)
    all_years = analytics.statistics(rows, mode_group=mode)["all_years"]
    return _page(request, db, account, "statistiche",
                 s=data, mode=mode, year=year, years=all_years, groups=_MODE_GROUPS)


@router.get("/contest", response_class=HTMLResponse)
def contest(request: Request, hours: int = 24, mult: str = "dxcc", mode: str = "",
            points: int = 1, db: Session = Depends(auth.session)):
    """Il punteggio della gara, contato sul Cloud.

    Qui non si registra niente: i QSO sono quelli che il programma ha gia'
    mandato su, e il conto si rifa' a ogni giro di pagina. Serve a guardare
    come sta andando da un altro computer, o dal telefono in macchina mentre
    l'operatore in shack macina.
    """
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    rows = _all_rows(db, account)
    hours = max(1, min(hours, 24 * 14))
    since = dt.datetime.now(dt.UTC) - dt.timedelta(hours=hours)
    score = analytics.contest_score(rows, since=since, mode_group=mode,
                                    points_per_qso=max(1, min(points, 100)), multiplier=mult)
    return _page(request, db, account, "contest",
                 score=score, hours=hours, mult=mult, mode=mode, points=points,
                 mults=analytics.CONTEST_MULTIPLIERS, groups=_MODE_GROUPS)


@router.get("/awards", response_class=HTMLResponse)
def awards_page(
    request: Request,
    band: str = "",
    mode: str = "",
    lotw: int = 1,
    card: int = 1,
    eqsl: int = 0,
    db: Session = Depends(auth.session),
):
    """Diplomi, con le conferme che si scelgono."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    rows = _all_rows(db, account)
    results = analytics.awards(rows, band=band, mode_group=mode,
                               confirm_lotw=bool(lotw), confirm_card=bool(card),
                               confirm_eqsl=bool(eqsl))
    used = [b for b in analytics.BAND_ORDER if any(b in i.worked for a in results for i in a.items)]
    was = next((a for a in results if a.id == "was"), None)
    waz = next((a for a in results if a.id == "waz"), None)

    return _page(request, db, account, "diplomi",
                 awards=[a for a in results if a.worked or a.target],
                 award_bands=used, award_band=band, mode=mode, groups=_MODE_GROUPS,
                 lotw=bool(lotw), card=bool(card), eqsl=bool(eqsl),
                 missing_states=analytics.missing_states(was) if was else [],
                 missing_zones=analytics.missing_zones(waz) if waz else [])


@router.get("/qsl", response_class=HTMLResponse)
def qsl_page(request: Request, db: Session = Depends(auth.session)):
    """Invio QSL: quante ne sono partite e quante ne sono tornate."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    rows = _all_rows(db, account)
    confirmed = [r for r in rows if r.confirmed_lotw or r.confirmed_card or r.confirmed_eqsl]
    latest = sorted(confirmed, key=lambda r: r.when or dt.datetime.min.replace(tzinfo=dt.UTC),
                    reverse=True)[:40]
    return _page(request, db, account, "qsl",
                 summary=analytics.qsl_summary(rows),
                 paper=analytics.paper_queue(rows),
                 confirmed=len(confirmed),
                 latest=[{"call": r.call, "band": r.band, "mode": r.label_mode,
                          "when": r.when.strftime("%Y-%m-%d") if r.when else "",
                          "lotw": r.confirmed_lotw, "card": r.confirmed_card,
                          "eqsl": r.confirmed_eqsl}
                         for r in latest])


@router.get("/activity", response_class=HTMLResponse)
def activity_page(request: Request, db: Session = Depends(auth.session)):
    """Registro attivita': cosa e' cambiato sul Cloud, e da quale dispositivo."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    recent = db.scalars(
        select(Qso)
        .where(Qso.account_id == account.id)
        .order_by(Qso.updated_at.desc().nullslast(), Qso.id.desc())
        .limit(40)
    ).all()
    docs = db.scalars(
        select(Doc).where(Doc.account_id == account.id).order_by(Doc.updated_at.desc().nullslast())
    ).all()
    return _page(request, db, account, "attivita",
                 changes=[{"uuid": r.uuid, "call": r.call, "band": r.band,
                           "revision": r.revision, "deleted": r.deleted, "device": r.device,
                           "when": r.updated_at.strftime("%Y-%m-%d %H:%M") if r.updated_at else ""}
                          for r in recent],
                 docs=[{"kind": d.kind, "key": d.key, "revision": d.revision, "device": d.device,
                        "when": d.updated_at.strftime("%Y-%m-%d %H:%M") if d.updated_at else ""}
                       for d in docs])


@router.get("/cluster", response_class=HTMLResponse)
def cluster_page(request: Request, db: Session = Depends(auth.session)):
    """DX Cluster: il collegamento vive nel programma, qui ci sono le sue fonti."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    settings = _settings_doc(db, account)
    sources = _json_setting(settings.get("cluster/sources"))
    rules = _json_setting(settings.get("cluster/alertRules") or settings.get("cluster/rules"))
    return _page(request, db, account, "cluster", sources=sources, rules=rules)


def _json_setting(raw) -> list:
    """Una impostazione che il programma salva come testo JSON."""
    import json

    if isinstance(raw, list):
        return raw
    if not raw:
        return []
    try:
        parsed = json.loads(raw)
    except (TypeError, ValueError):
        return []
    return parsed if isinstance(parsed, list) else []


@router.get("/propagation", response_class=HTMLResponse)
def propagation_page(request: Request, db: Session = Depends(auth.session)):
    """Propagazione: gli stessi numeri del pannello del programma, stessa fonte."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    data = solar_source.current()
    # Le condizioni HF, una riga per banda con giorno e notte affiancati.
    table = []
    for band in dict.fromkeys(c["band"] for c in data.get("hf", [])):
        day = next((c for c in data["hf"] if c["band"] == band and c["when"] == "day"), None)
        night = next((c for c in data["hf"] if c["band"] == band and c["when"] == "night"), None)
        table.append({
            "band": band,
            "day": day["condition"] if day else "",
            "day_class": day["class"] if day else "unknown",
            "night": night["condition"] if night else "",
            "night_class": night["class"] if night else "unknown",
        })

    return _page(request, db, account, "propagazione", solar=data, hf_table=table)


@router.get("/map", response_class=HTMLResponse)
def map_page(request: Request, db: Session = Depends(auth.session)):
    """La mappa grande: gli stessi locatori del riquadro nella colonna."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)
    return _page(request, db, account, "mappa", big_map=True)


@router.get("/qso/{uuid}", response_class=HTMLResponse)
def qso(request: Request, uuid: str, db: Session = Depends(auth.session)):
    """Un QSO: la finestra con quel collegamento scelto nella colonna."""
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)
    row = db.scalar(select(Qso).where(Qso.account_id == account.id, Qso.uuid == uuid))
    if row is None:
        return HTMLResponse("QSO non trovato", status_code=404)

    rows = _all_rows(db, account)
    data = analytics.statistics(rows)
    return _page(request, db, account, "statistiche", all_fields=True,
                 s=data, mode="", year=0, years=data["all_years"], groups=_MODE_GROUPS)


@router.get("/station", response_class=HTMLResponse)
def station(request: Request, db: Session = Depends(auth.session)):
    """Stazione: profili e impostazioni, il resto del log che non e' un QSO."""
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
    secrets = db.scalar(
        select(Doc).where(Doc.account_id == account.id, Doc.kind == "secret", Doc.key == "vault")
    )

    return _page(
        request, db, account, "stazione",
        profiles=[{"key": row.key, "revision": row.revision,
                   "updated": row.updated_at.strftime("%Y-%m-%d %H:%M") if row.updated_at else "",
                   "device": row.device, "data": row.data or {}}
                  for row in profiles],
        settings=values,
        settings_revision=settings_doc.revision if settings_doc else 0,
        settings_updated=settings_doc.updated_at.strftime("%Y-%m-%d %H:%M")
        if settings_doc and settings_doc.updated_at else "",
        vault={"revision": secrets.revision,
               "updated": secrets.updated_at.strftime("%Y-%m-%d %H:%M") if secrets.updated_at else "",
               "device": secrets.device} if secrets else None,
        editable=[{"key": key, "field": key.replace("/", "."), "options": options,
                   "value": _as_text(dict(settings_doc.data or {}).get(key) if settings_doc else None)}
                  for key, options in EDITABLE.items()],
    )


def _as_text(value) -> str:
    """Il valore di un'impostazione come lo mostra un menu a tendina."""
    if isinstance(value, bool):
        return "true" if value else "false"
    if value is None or isinstance(value, dict):
        return ""
    return str(value)


# Quello che si puo' cambiare dal browser, e i valori ammessi.
#
# Non tutte le impostazioni: porte, percorsi e filtri salvati si toccano dal
# programma, dove c'e' il contesto per capirli. Da qui si cambia quello che ha
# senso cambiare stando altrove — come si vede e come si sincronizza — e non si
# puo' scrivere niente che il programma non sappia rileggere.
EDITABLE = {
    "theme/current": ["Ocean Blue", "Stellar Light", "Darkcodium"],
    "theme/accentVariant": ["phosphor", "cyan", "amber", "red"],
    "theme/density": ["compact", "regular", "comfortable"],
    "ui/language": ["auto", "it", "en"],
    "cloud/auto": ["qso", "timer", "manual"],
    "cluster/followDecodiumBand": ["true", "false"],
    "cluster/voice/enabled": ["true", "false"],
    "rotor/followDx": ["true", "false"],
    "udp/followDxCall": ["true", "false"],
    "backup/enabled": ["true", "false"],
}


@router.post("/station/settings")
async def save_settings(request: Request, db: Session = Depends(auth.session)):
    """Cambia le impostazioni della stazione da qui, e le manda al programma.

    Si scrive lo stesso documento che sincronizza DecoDXLog, con una revisione in
    piu': al giro dopo il programma se lo riprende e si adegua — il tema, per
    esempio, si ridipinge da solo senza riavviare.
    """
    account = _account_from_cookie(request, db)
    if account is None:
        return RedirectResponse("/", status_code=303)

    form = await request.form()
    row = db.scalar(
        select(Doc).where(Doc.account_id == account.id, Doc.kind == "setting", Doc.key == "station")
    )
    values = dict(row.data or {}) if row else {}

    changed = 0
    for key, allowed in EDITABLE.items():
        # I campi arrivano con i punti al posto delle barre: una barra in un
        # nome di campo HTML fa solo confusione.
        sent = form.get(key.replace("/", "."))
        if sent is None:
            continue
        sent = str(sent)
        if sent not in allowed:
            continue
        # Vero e falso restano booleani, come li scrive il programma.
        value: object = sent
        if sent in ("true", "false"):
            value = sent == "true"
        if values.get(key) != value:
            values[key] = value
            changed += 1

    if changed:
        sync.apply_doc(db, account, {
            "kind": "setting", "key": "station",
            "revision": (row.revision if row else 0) + 1,
            "data": values,
        }, device="browser")
        db.commit()

    return RedirectResponse("/station", status_code=303)


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
        f"DecoDXLog Cloud — {account.callsign}",
        "<ADIF_VER:5>3.1.4",
        "<PROGRAMID:12>DecoDXLog Cloud",
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
