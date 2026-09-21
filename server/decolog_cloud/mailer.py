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
from email.message import EmailMessage

from .settings import settings

log = logging.getLogger("decolog.mailer")


def configured() -> bool:
    """Se manca qualcosa non si prova nemmeno: cosi' i test girano senza rete."""
    return bool(settings.smtp_host and settings.smtp_user
                and settings.smtp_password and settings.notify_email)


def send(subject: str, body: str, to: str = "") -> bool:
    """Manda una email di solo testo. Torna se e' partita davvero."""
    destination = to or settings.notify_email
    if not configured() or not destination:
        log.info("email non mandata (SMTP non configurato): %s", subject)
        return False

    message = EmailMessage()
    message["From"] = settings.smtp_user
    message["To"] = destination
    message["Subject"] = subject
    message.set_content(body)

    try:
        # 465 vuole la cifratura dall'inizio, 587 la chiede dopo con STARTTLS:
        # sono i due modi in cui si trova Gmail.
        if settings.smtp_port == 465:
            with smtplib.SMTP_SSL(settings.smtp_host, settings.smtp_port, timeout=20) as server:
                server.login(settings.smtp_user, settings.smtp_password)
                server.send_message(message)
        else:
            with smtplib.SMTP(settings.smtp_host, settings.smtp_port, timeout=20) as server:
                server.starttls()
                server.login(settings.smtp_user, settings.smtp_password)
                server.send_message(message)
    except Exception as error:  # noqa: BLE001 — qualunque cosa vada storta, il servizio resta in piedi
        log.warning("email non partita (%s): %s", subject, error)
        return False
    log.info("email mandata a %s: %s", destination, subject)
    return True
