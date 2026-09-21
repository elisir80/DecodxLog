"""DecoDXLog Cloud — chi entra lo decide una persona.

La porta resta aperta: chiunque si registra e riceve il suo token. Ma finche'
qualcuno non ha detto di si', il sync risponde di no — e lo dice con una frase
che si capisce, non con un numero.
"""

from __future__ import annotations

import dataclasses
import os
import tempfile

import pytest

os.environ.setdefault("DECOLOG_DATABASE_URL", "sqlite:///" + os.path.join(tempfile.mkdtemp(), "t.sqlite"))

from fastapi.testclient import TestClient  # noqa: E402
from sqlalchemy import select, text  # noqa: E402

from decolog_cloud import approval, mailer, models  # noqa: E402
from decolog_cloud.main import app  # noqa: E402
from decolog_cloud.models import Account  # noqa: E402

PASSWORD = "una password lunga"


@pytest.fixture()
def client(tmp_path):
    url = "sqlite:///" + str(tmp_path / "cloud.sqlite").replace("\\", "/")
    engine = models.create_engine(url, future=True, connect_args={"check_same_thread": False})
    models.engine = engine
    models.SessionLocal.configure(bind=engine)
    models.Base.metadata.create_all(engine)
    # Il conto di chi ha gia' ricevuto un avviso sta in memoria: ogni prova
    # riparte da zero, se no la seconda si vede rifiutare l'email dalla prima.
    approval._last_notice.clear()
    with TestClient(app) as c:
        yield c


def signup(client, callsign="IU8LMC"):
    reply = client.post("/v1/auth/signup", json={"callsign": callsign, "password": PASSWORD})
    assert reply.status_code == 200, reply.text
    return {"Authorization": "Bearer " + reply.json()["token"]}


def link(callsign="IU8LMC"):
    with models.SessionLocal() as db:
        account = db.scalar(select(Account).where(Account.callsign == callsign))
        return account.id, account.approval_token


def test_who_signs_up_gets_a_token_but_not_the_cloud(client):
    headers = signup(client)
    reply = client.get("/v1/sync/status", headers=headers)
    assert reply.status_code == 403
    # Non un numero e basta: quello che c'e' scritto deve dire cosa sta
    # succedendo, perche' finisce sotto gli occhi di chi opera.
    assert "in attesa di approvazione" in reply.json()["detail"]


def test_after_the_yes_the_same_token_works(client):
    headers = signup(client)
    identifier, token = link()
    # La pagina si guarda prima di decidere: dice chi ha chiesto di entrare.
    page = client.get(f"/admin/signup/{identifier}/{token}")
    assert page.status_code == 200
    assert "IU8LMC" in page.text

    reply = client.post(f"/admin/signup/{identifier}/{token}", data={"answer": "si"})
    assert reply.status_code == 200
    # Nessuno deve rifare la password: il token di prima vale ancora.
    assert client.get("/v1/sync/status", headers=headers).status_code == 200


def test_the_link_is_good_once(client):
    signup(client)
    identifier, token = link()
    assert client.post(f"/admin/signup/{identifier}/{token}", data={"answer": "si"}).status_code == 200
    # Chi lo riapre — o chi lo ha intercettato — non trova piu' niente.
    assert client.post(f"/admin/signup/{identifier}/{token}", data={"answer": "no"}).status_code == 404
    assert client.get(f"/admin/signup/{identifier}/{token}").status_code == 404


def test_an_invented_link_opens_nothing(client):
    signup(client)
    identifier, _ = link()
    assert client.get(f"/admin/signup/{identifier}/chiavefinta").status_code == 404
    assert client.get("/admin/signup/999/chiavefinta").status_code == 404


def test_a_no_takes_the_account_away(client):
    signup(client)
    identifier, token = link()
    assert client.post(f"/admin/signup/{identifier}/{token}", data={"answer": "no"}).status_code == 200
    with models.SessionLocal() as db:
        assert db.scalar(select(Account).where(Account.callsign == "IU8LMC")) is None
    # Rifiutato per sbaglio: si registra di nuovo e arriva un altro avviso.
    assert client.post("/v1/auth/signup",
                       json={"callsign": "IU8LMC", "password": PASSWORD}).status_code == 200


def test_the_website_says_it_plainly(client):
    signup(client)
    reply = client.post("/login", data={"callsign": "IU8LMC", "password": PASSWORD},
                        follow_redirects=False)
    assert reply.status_code == 403
    assert "in attesa di approvazione" in reply.text
    identifier, token = link()
    client.post(f"/admin/signup/{identifier}/{token}", data={"answer": "si"})
    assert client.post("/login", data={"callsign": "IU8LMC", "password": PASSWORD},
                       follow_redirects=False).status_code == 303


def test_the_email_says_who_and_carries_the_link(client, monkeypatch):
    sent = []
    monkeypatch.setattr(mailer, "send", lambda subject, body, to="": sent.append((subject, body)) or True)
    signup(client, "DL9ZZT")
    assert len(sent) == 1
    subject, body = sent[0]
    assert "DL9ZZT" in subject and "DL9ZZT" in body
    identifier, token = link("DL9ZZT")
    assert f"/admin/signup/{identifier}/{token}" in body


def test_one_email_per_address_every_so_often(client, monkeypatch):
    sent = []
    monkeypatch.setattr(mailer, "send", lambda subject, body, to="": sent.append(subject) or True)
    # Chi prova nominativi a raffica dallo stesso indirizzo non riempie la
    # casella di nessuno: il primo avvisa, gli altri no.
    for call in ("DL9ZZT", "VK3ABC", "JA1ZZZ"):
        signup(client, call)
    assert len(sent) == 1
    # Ma gli account ci sono tutti e tre, e aspettano tutti e tre.
    with models.SessionLocal() as db:
        assert len(approval.pending(db)) == 3


def test_a_callsign_already_there_sends_nothing(client, monkeypatch):
    signup(client, "IU8LMC")
    sent = []
    monkeypatch.setattr(mailer, "send", lambda subject, body, to="": sent.append(subject) or True)
    again = client.post("/v1/auth/signup", json={"callsign": "IU8LMC", "password": PASSWORD})
    assert again.status_code == 409
    assert sent == []


def test_without_smtp_the_signup_still_works(client, monkeypatch):
    # Nessuna casella configurata: l'avviso non parte, ma la registrazione si'.
    monkeypatch.setattr(mailer, "configured", lambda: False)
    headers = signup(client, "F5DEF")
    assert client.get("/v1/sync/status", headers=headers).status_code == 403


def test_who_was_already_in_stays_in(tmp_path):
    """La colonna nuova non chiude fuori chi usava il servizio ieri."""
    url = "sqlite:///" + str(tmp_path / "vecchio.sqlite").replace("\\", "/")
    engine = models.create_engine(url, future=True, connect_args={"check_same_thread": False})
    # Il database com'era prima: la tabella account senza le colonne nuove.
    with engine.begin() as connection:
        connection.execute(text(
            "CREATE TABLE account (id INTEGER PRIMARY KEY, callsign VARCHAR(32) UNIQUE, "
            "password_hash TEXT, created_at TIMESTAMP)"))
        connection.execute(text(
            "INSERT INTO account (callsign, password_hash, created_at) "
            "VALUES ('IU8LMC', 'x', '2026-01-01 00:00:00')"))

    models.engine = engine
    models.SessionLocal.configure(bind=engine)
    models.create_all()

    with models.SessionLocal() as db:
        account = db.scalar(select(Account).where(Account.callsign == "IU8LMC"))
        assert account is not None
        assert account.approved is True
        assert approval.pending(db) == []


def with_mailbox():
    """Le impostazioni con una casella dentro. Sono congelate: se ne fa una copia."""
    return dataclasses.replace(
        mailer.settings,
        smtp_host="smtp.gmail.com",
        smtp_port=587,
        smtp_user="mittente@gmail.com",
        smtp_password="una app password",
        notify_email="iu8lmc@gmail.com",
    )


class FakeSmtp:
    """Un server di posta finto: registra cosa gli e' stato detto e in che ordine."""

    steps: list[str] = []
    message = None

    def __init__(self, host, port, timeout=0):
        FakeSmtp.steps.append(f"connect {host}:{port}")

    def __enter__(self):
        return self

    def __exit__(self, *_):
        FakeSmtp.steps.append("close")
        return False

    def starttls(self):
        FakeSmtp.steps.append("starttls")

    def login(self, user, password):
        FakeSmtp.steps.append(f"login {user}")

    def send_message(self, message):
        FakeSmtp.steps.append("send")
        FakeSmtp.message = message


def test_the_email_is_built_and_sent_the_way_gmail_wants(monkeypatch):
    # Con la porta 587 si apre in chiaro e si cifra dopo, con STARTTLS: e'
    # l'ordine che vuole Gmail, e se si sbaglia non parte niente.
    FakeSmtp.steps = []
    monkeypatch.setattr("smtplib.SMTP", FakeSmtp)
    monkeypatch.setattr(mailer, "settings", with_mailbox())

    assert mailer.send("prova", "il corpo") is True
    assert FakeSmtp.steps == ["connect smtp.gmail.com:587", "starttls",
                              "login mittente@gmail.com", "send", "close"]
    assert FakeSmtp.message["To"] == "iu8lmc@gmail.com"
    assert FakeSmtp.message["From"] == "mittente@gmail.com"
    assert FakeSmtp.message["Subject"] == "prova"
    assert FakeSmtp.message.get_content().strip() == "il corpo"


def test_a_mail_server_that_says_no_does_not_take_the_service_down(monkeypatch):
    def explode(*_args, **_kwargs):
        raise OSError("la casella non risponde")

    monkeypatch.setattr("smtplib.SMTP", explode)
    monkeypatch.setattr(mailer, "settings", with_mailbox())
    # Non solleva: torna False e basta. Il servizio non si pianta per una posta.
    assert mailer.send("prova", "il corpo") is False
