"""DecoDXLog Cloud — mandare una email, quando serve avvisare una persona.

Una funzione sola, con smtplib della libreria standard: la stessa casella e la
stessa app password che usa gia' la community, lette dall'ambiente. Nessuna
dipendenza in piu' per una riga di posta.

La regola qui dentro e' che **non si solleva mai**: se la posta non parte — la
casella non risponde, la password e' scaduta, non c'e' rete — resta una riga nel
registro e il servizio va avanti. Un operatore non deve vedere un errore perche'
il server non e' riuscito ad avvisare qualcuno.
"""

from __future__ import annotations

import logging
import smtplib
from dataclasses import dataclass
from email.message import EmailMessage

from .settings import settings

log = logging.getLogger("decolog.mailer")


@dataclass(frozen=True)
class Mailbox:
    """Una casella da cui mandare: quale server, chi e' e come si chiama."""

    host: str
    port: int
    user: str
    password: str
    display: str = ""

    @property
    def ready(self) -> bool:
        return bool(self.host and self.user and self.password)


def notices() -> Mailbox:
    """La casella degli avvisi: le registrazioni da approvare."""
    return Mailbox(settings.smtp_host, settings.smtp_port,
                   settings.smtp_user, settings.smtp_password)


def cards() -> Mailbox:
    """La casella da cui escono le QSL.

    Sta separata da quella degli avvisi apposta: le QSL vanno a sconosciuti in
    giro per il mondo, e se quella casella finisce in lista nera non si porta
    dietro anche le email che avvisano chi tiene il servizio.
    """
    if settings.qsl_smtp_host and settings.qsl_smtp_user:
        return Mailbox(settings.qsl_smtp_host, settings.qsl_smtp_port,
                       settings.qsl_smtp_user, settings.qsl_smtp_password,
                       settings.qsl_from_name)
    return notices()


def configured() -> bool:
    """Se manca qualcosa non si prova nemmeno: cosi' i test girano senza rete."""
    return notices().ready and bool(settings.notify_email)


def send(subject: str, body: str, to: str = "", *, mailbox: Mailbox | None = None,
         reply_to: str = "", attachment: bytes = b"", attachment_name: str = "") -> bool:
    """Manda una email. Con `attachment` diventa un messaggio con allegato.

    Torna se e' partita davvero: chi chiama decide cosa dire a chi aspetta.
    """
    box = mailbox or notices()
    destination = to or settings.notify_email
    if not box.ready or not destination:
        log.info("email non mandata (SMTP non configurato): %s", subject)
        return False

    message = EmailMessage()
    message["From"] = f"{box.display} <{box.user}>" if box.display else box.user
    message["To"] = destination
    message["Subject"] = subject
    if reply_to:
        # Chi risponde deve arrivare all'operatore, non alla casella di servizio.
        message["Reply-To"] = reply_to
    message.set_content(body)
    if attachment:
        message.add_attachment(attachment, maintype="image", subtype="png",
                               filename=attachment_name or "qsl.png")

    try:
        # 465 vuole la cifratura dall'inizio, 587 la chiede dopo con STARTTLS:
        # sono i due modi in cui si trovano Gmail e Aruba.
        if box.port == 465:
            with smtplib.SMTP_SSL(box.host, box.port, timeout=30) as server:
                server.login(box.user, box.password)
                server.send_message(message)
        else:
            with smtplib.SMTP(box.host, box.port, timeout=30) as server:
                server.starttls()
                server.login(box.user, box.password)
                server.send_message(message)
    except Exception as error:  # noqa: BLE001 — qualunque cosa vada storta, il servizio resta in piedi
        log.warning("email non partita (%s): %s", subject, error)
        return False
    log.info("email mandata a %s: %s", destination, subject)
    return True
