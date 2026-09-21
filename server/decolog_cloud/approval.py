"""DecoDXLog Cloud — chi entra lo decide una persona.

La porta resta aperta: chiunque si registra e il programma gli da' il suo token
come sempre. Ma l'account nasce **in attesa**, e finche' qualcuno non dice di si'
il sync risponde di no, con una frase che si capisce.

A dire di si' si fa dall'email che arriva a ogni registrazione: dentro c'e' un
collegamento con una chiave che vale una volta sola. Si apre dal telefono, si
legge chi ha chiesto di entrare, e si decide. Niente SSH, niente pannelli.

Il collegamento porta a una pagina con due pulsanti, non decide da solo: i
programmi che scansionano la posta seguono i collegamenti, e una scansione non
deve approvare nessuno.
"""

from __future__ import annotations

import datetime as dt
import logging
import secrets
import time

from fastapi import APIRouter, Depends, Form, Request
from fastapi.responses import HTMLResponse
from fastapi.templating import Jinja2Templates
from pathlib import Path
from sqlalchemy import select
from sqlalchemy.orm import Session

from . import auth, mailer
from .models import Account
from .settings import settings

log = logging.getLogger("decolog.approval")
router = APIRouter()
templates = Jinja2Templates(directory=str(Path(__file__).parent / "templates"))


def new_token() -> str:
    return secrets.token_urlsafe(24)


def client_ip(request: Request) -> str:
    """L'indirizzo di chi ha chiamato, per quel che vale.

    Dietro nginx uvicorn gira con --proxy-headers, quindi request.client e' gia'
    l'indirizzo vero e non quello del proxy.
    """
    return request.client.host if request.client else ""


# Da quale indirizzo si e' gia' mandato un avviso, e quando. Sta in memoria: il
# servizio e' un processo solo, e se riparte il peggio che succede e' un avviso
# in piu'.
_last_notice: dict[str, float] = {}


def may_notify(ip: str) -> bool:
    """Un avviso per indirizzo ogni tot minuti: la casella non si riempie."""
    if settings.notify_minutes <= 0:
        return True
    now = time.monotonic()
    window = settings.notify_minutes * 60
    # Le voci vecchie si buttano qui, che e' l'unico posto dove si passa.
    for address, when in list(_last_notice.items()):
        if now - when > window:
            del _last_notice[address]
    if ip and ip in _last_notice:
        return False
    _last_notice[ip or "?"] = now
    return True


def notify(account: Account, ip: str) -> bool:
    """L'email che chiede se questo nominativo puo' entrare."""
    if not may_notify(ip):
        log.info("avviso non mandato per %s: troppi tentativi da %s", account.callsign, ip)
        return False
    link = f"{settings.public_url}/admin/signup/{account.id}/{account.approval_token}"
    when = account.created_at or dt.datetime.now(dt.UTC)
    body = (
        f"Qualcuno si e' registrato su DecoDXLog Cloud e aspetta il tuo via libera.\n"
        f"\n"
        f"  Nominativo: {account.callsign}\n"
        f"  Quando:     {when:%Y-%m-%d %H:%M} UTC\n"
        f"  Da:         {ip or 'indirizzo sconosciuto'}\n"
        f"\n"
        f"Per decidere, apri questa pagina:\n"
        f"\n"
        f"  {link}\n"
        f"\n"
        f"Ci sono due pulsanti, accetta e rifiuta. Il collegamento vale una volta\n"
        f"sola: dopo la decisione non funziona piu'.\n"
        f"\n"
        f"Finche' non decidi, quel nominativo puo' registrarsi e avere il token, ma\n"
        f"il sync gli risponde che la registrazione e' in attesa di approvazione.\n"
        f"\n"
        f"-- \n"
        f"DecoDXLog Cloud, {settings.public_url}\n"
    )
    return mailer.send(f"DecoDXLog Cloud: {account.callsign} chiede di entrare", body)


def _page(request: Request, account: Account | None, message: str, status_code: int = 200) -> HTMLResponse:
    return templates.TemplateResponse(
        request,
        "approval.html",
        {"account": account, "message": message, "public_url": settings.public_url},
        status_code=status_code,
    )


@router.get("/admin/signup/{account_id}/{token}", response_class=HTMLResponse)
def review(account_id: int, token: str, request: Request, db: Session = Depends(auth.session)):
    account = db.get(Account, account_id)
    if account is None or not account.approval_token or not secrets.compare_digest(account.approval_token, token):
        # Gia' deciso, oppure un collegamento inventato: stessa risposta, che da
        # fuori non si deve capire quale dei due.
        return _page(request, None, "Questo collegamento non vale piu'.", status_code=404)
    return _page(request, account, "")


@router.post("/admin/signup/{account_id}/{token}", response_class=HTMLResponse)
def decide(
    account_id: int,
    token: str,
    request: Request,
    answer: str = Form(...),
    db: Session = Depends(auth.session),
):
    account = db.get(Account, account_id)
    if account is None or not account.approval_token or not secrets.compare_digest(account.approval_token, token):
        return _page(request, None, "Questo collegamento non vale piu'.", status_code=404)

    callsign = account.callsign
    if answer == "si":
        account.approved = True
        account.approved_at = dt.datetime.now(dt.UTC)
        account.approval_token = ""
        db.commit()
        log.info("account approvato: %s", callsign)
        return _page(request, None, f"{callsign} adesso puo' usare il Cloud.")

    # Rifiutato: l'account si toglie di mezzo del tutto, con i suoi token. Chi e'
    # stato rifiutato per sbaglio si registra di nuovo, e arriva un altro avviso.
    db.delete(account)
    db.commit()
    log.info("account rifiutato e cancellato: %s", callsign)
    return _page(request, None, f"{callsign} e' stato rifiutato, e non c'e' piu'.")


def pending(db: Session) -> list[Account]:
    """Chi aspetta ancora una risposta: per chi guarda da riga di comando."""
    return list(db.scalars(select(Account).where(Account.approved.is_(False)).order_by(Account.created_at)))
