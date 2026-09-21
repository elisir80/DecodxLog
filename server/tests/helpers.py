"""Quello che serve a piu' di una prova.

Da quando chi entra lo decide una persona, un account appena registrato non puo'
ancora usare il servizio: le prove devono fare la stessa cosa che fa chi riceve
l'email, cioe' aprire il collegamento e dire di si'. Meglio cosi' che spegnere
l'approvazione nelle prove: quello che si prova e' il servizio vero.
"""

from __future__ import annotations

from sqlalchemy import select

from decolog_cloud import models
from decolog_cloud.models import Account


def approve(client, callsign: str = "IU8LMC") -> None:
    """Approva un account appena registrato, dal suo collegamento."""
    with models.SessionLocal() as db:
        account = db.scalar(select(Account).where(Account.callsign == callsign.strip().upper()))
        assert account is not None, callsign
        if not account.approval_token:
            return  # gia' deciso, o approvazione spenta
        identifier, token = account.id, account.approval_token
    reply = client.post(f"/admin/signup/{identifier}/{token}", data={"answer": "si"})
    assert reply.status_code == 200, reply.text
