"""DecoDXLog Cloud — le QSL che passano di qui.

Il programma sul computer dell'operatore non ha la password di nessuna casella:
manda la cartolina a questa rotta, con il suo token, e la casella la mette il
server. Cosi' il segreto sta su una macchina sola, e chi tiene il servizio puo'
chiudere il rubinetto a chi ne abusa senza dover aggiornare nessuno.

L'email parte come «IU8LMC via DecoDXLog», e chi risponde arriva all'operatore,
non qui: una QSL che non si sa da chi viene non serve a niente.

Il freno e' la parte che conta. Una casella condivisa che manda a sconosciuti
in giro per il mondo e' esattamente quello che i filtri antispam guardano: se
una persona sola potesse mandarne diecimila, il dominio finirebbe in lista nera
per tutti. Quindi un tetto al giorno per nominativo e un tetto al giorno in
tutto, e quando si toccano si dice quanto manca al giorno dopo.
"""

from __future__ import annotations

import base64
import binascii
import datetime as dt
import logging

from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, Field
from sqlalchemy.orm import Session

from . import auth, mailer
from .models import Account
from .settings import settings

log = logging.getLogger("decolog.qsl")
router = APIRouter()

# Una cartolina e' un PNG: oltre questo non e' una cartolina, e' un problema.
MAX_ATTACHMENT_BYTES = 4 * 1024 * 1024
# Il PNG comincia sempre cosi'. Si controlla per non fare da corriere a qualunque
# file: di qui passano solo cartoline.
PNG_MAGIC = b"\x89PNG\r\n\x1a\n"


class CardIn(BaseModel):
    to: str = Field(min_length=3, max_length=254)
    subject: str = Field(default="", max_length=200)
    body: str = Field(default="", max_length=20_000)
    replyTo: str = Field(default="", max_length=254)
    attachmentName: str = Field(default="qsl.png", max_length=120)
    attachment: str = Field(default="", max_length=8 * 1024 * 1024)


class CardOut(BaseModel):
    sent: bool
    remaining: int


# Quante ne ha mandate oggi ognuno, e quante in tutto. Sta in memoria: il
# servizio e' un processo solo, e se riparte il peggio che succede e' che i
# contatori ripartono — non si perde niente e non si manda niente di piu' di
# quello che l'operatore ha chiesto.
_sent_today: dict[str, int] = {}
_day: dt.date | None = None


def _roll_over() -> None:
    global _day
    today = dt.datetime.now(dt.UTC).date()
    if _day != today:
        _day = today
        _sent_today.clear()


def remaining_for(callsign: str) -> int:
    """Quante cartoline puo' ancora mandare oggi questo nominativo."""
    _roll_over()
    mine = settings.qsl_daily_limit - _sent_today.get(callsign, 0)
    everyone = settings.qsl_daily_total - sum(_sent_today.values())
    return max(0, min(mine, everyone))


def _looks_like_an_address(text: str) -> bool:
    # Non si fa la guerra alle RFC: basta che ci sia una chiocciola in mezzo a
    # qualcosa, senza spazi e senza righe nuove — quelle servirebbero solo a
    # infilare intestazioni in piu'.
    if any(c in text for c in "\r\n\t") or " " in text.strip():
        return False
    left, _, right = text.strip().partition("@")
    return bool(left) and "." in right


@router.post("/v1/qsl/mail", response_model=CardOut)
def send_card(body: CardIn,
              account: Account = Depends(auth.current_account),
              db: Session = Depends(auth.session)) -> CardOut:
    del db  # il token basta: qui non si legge il log
    box = mailer.cards()
    if not box.ready:
        raise HTTPException(status.HTTP_503_SERVICE_UNAVAILABLE,
                            "questo Cloud non manda QSL per email")

    to = body.to.strip()
    if not _looks_like_an_address(to):
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, "indirizzo non valido")
    reply_to = body.replyTo.strip()
    if reply_to and not _looks_like_an_address(reply_to):
        raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, "indirizzo di risposta non valido")

    left = remaining_for(account.callsign)
    if left <= 0:
        raise HTTPException(status.HTTP_429_TOO_MANY_REQUESTS,
                            "per oggi il tetto delle QSL e' stato raggiunto: si riprende domani")

    card = b""
    if body.attachment:
        try:
            card = base64.b64decode(body.attachment, validate=True)
        except (binascii.Error, ValueError) as error:
            raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY,
                                "la cartolina non si legge") from error
        if len(card) > MAX_ATTACHMENT_BYTES:
            raise HTTPException(status.HTTP_413_REQUEST_ENTITY_TOO_LARGE, "cartolina troppo grande")
        if not card.startswith(PNG_MAGIC):
            raise HTTPException(status.HTTP_422_UNPROCESSABLE_ENTITY, "la cartolina non e' un PNG")

    subject = (body.subject.strip() or f"QSL {account.callsign}").replace("\n", " ").replace("\r", " ")
    # In fondo si dice chi ha mandato: la casella e' di servizio, l'operatore e'
    # un altro, e chi riceve deve poterlo capire senza indovinare.
    footer = (f"\n\n-- \nQSL di {account.callsign}, mandata con DecoDXLog.\n"
              f"Rispondendo a questa email si risponde direttamente all'operatore.\n")
    text = (body.body or f"QSL da {account.callsign}.") + footer

    ok = mailer.send(subject, text, to, mailbox=box, reply_to=reply_to or None or "",
                     attachment=card, attachment_name=body.attachmentName)
    if not ok:
        raise HTTPException(status.HTTP_502_BAD_GATEWAY, "la casella non ha accettato la QSL")

    _sent_today[account.callsign] = _sent_today.get(account.callsign, 0) + 1
    log.info("QSL di %s mandata a %s (ne restano %d oggi)",
             account.callsign, to, remaining_for(account.callsign))
    return CardOut(sent=True, remaining=remaining_for(account.callsign))
