"""Le QSL che passano dal Cloud.

Qui non si prova che l'email arrivi — quello lo prova una cartolina mandata a se
stessi — ma tutto quello che sta prima: chi puo' mandare, quanto puo' mandare,
e cosa si rifiuta. Il freno e' la parte che conta: una casella condivisa che
manda a sconosciuti e' quella che finisce in lista nera, e ci finirebbe per
tutti insieme.
"""

from __future__ import annotations

import base64
import dataclasses
import os
import tempfile

import pytest

os.environ.setdefault("DECOLOG_DATABASE_URL", "sqlite:///" + os.path.join(tempfile.mkdtemp(), "t.sqlite"))

from fastapi.testclient import TestClient  # noqa: E402

from decolog_cloud import mailer, models, qslmail  # noqa: E402
from decolog_cloud.main import app  # noqa: E402

from .helpers import approve  # noqa: E402

PASSWORD = "una password lunga"
# Un PNG vero, il piu' piccolo che esista: 1x1 pixel.
PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mNk"
    "YPhfDwAChwGA60e6kgAAAABJRU5ErkJggg==")


@pytest.fixture()
def client(tmp_path, monkeypatch):
    url = "sqlite:///" + str(tmp_path / "cloud.sqlite").replace("\\", "/")
    engine = models.create_engine(url, future=True, connect_args={"check_same_thread": False})
    models.engine = engine
    models.SessionLocal.configure(bind=engine)
    models.Base.metadata.create_all(engine)
    qslmail._sent_today.clear()
    qslmail._day = None
    # Una casella finta, pronta: quello che parte si registra invece di partire.
    monkeypatch.setattr(mailer, "cards", lambda: mailer.Mailbox(
        "smtps.aruba.it", 465, "decodxlog@example.it", "segreta", "DecoDXLog"))
    with TestClient(app) as c:
        yield c


@pytest.fixture()
def outbox(monkeypatch):
    sent = []

    def fake(subject, body, to="", *, mailbox=None, reply_to="",
             attachment=b"", attachment_name=""):
        sent.append({"subject": subject, "body": body, "to": to, "reply_to": reply_to,
                     "attachment": attachment, "name": attachment_name,
                     "from": mailbox.user if mailbox else ""})
        return True

    monkeypatch.setattr(mailer, "send", fake)
    return sent


def headers(client, callsign="IU8LMC", outbox=None):
    reply = client.post("/v1/auth/signup", json={"callsign": callsign, "password": PASSWORD})
    assert reply.status_code == 200, reply.text
    approve(client, callsign)
    # La registrazione manda l'avviso a chi tiene il servizio: non e' una QSL,
    # e nelle prove che contano le QSL non deve dare fastidio.
    if outbox is not None:
        outbox.clear()
    return {"Authorization": "Bearer " + reply.json()["token"]}


def card(**extra):
    body = {"to": "dl9zzt@example.de", "subject": "QSL IU8LMC",
            "body": "Grazie per il QSO.", "replyTo": "iu8lmc@example.it",
            "attachmentName": "DL9ZZT-20260218.png",
            "attachment": base64.b64encode(PNG).decode()}
    body.update(extra)
    return body


def test_the_card_leaves_from_the_service_mailbox_but_answers_go_to_the_operator(client, outbox):
    reply = client.post("/v1/qsl/mail", json=card(), headers=headers(client, outbox=outbox))
    assert reply.status_code == 200, reply.text
    assert reply.json()["sent"] is True

    assert len(outbox) == 1
    out = outbox[0]
    assert out["from"] == "decodxlog@example.it"
    assert out["to"] == "dl9zzt@example.de"
    # Chi risponde arriva all'operatore, non alla casella di servizio.
    assert out["reply_to"] == "iu8lmc@example.it"
    assert out["attachment"] == PNG
    assert out["name"] == "DL9ZZT-20260218.png"
    # E in fondo c'e' scritto da chi viene: una QSL anonima non serve a niente.
    assert "IU8LMC" in out["body"]


def test_without_a_token_nothing_goes_out(client, outbox):
    assert client.post("/v1/qsl/mail", json=card()).status_code == 401
    assert outbox == []


def test_an_account_still_waiting_cannot_send(client, outbox):
    reply = client.post("/v1/auth/signup", json={"callsign": "DL9ZZT", "password": PASSWORD})
    token = {"Authorization": "Bearer " + reply.json()["token"]}
    # Registrato ma non ancora approvato: il Cloud non e' un ponte per chiunque.
    assert client.post("/v1/qsl/mail", json=card(), headers=token).status_code == 403
    assert outbox == []


def test_only_a_png_goes_through(client, outbox):
    token = headers(client)
    # Un file che non e' una cartolina non passa: di qui non si fa il corriere.
    bad = card(attachment=base64.b64encode(b"MZ non sono un png").decode())
    assert client.post("/v1/qsl/mail", json=bad, headers=token).status_code == 422
    # E nemmeno del base64 rotto.
    assert client.post("/v1/qsl/mail", json=card(attachment="non base64!!"),
                       headers=token).status_code == 422
    assert outbox == []


def test_an_address_with_a_newline_cannot_smuggle_headers(client, outbox):
    token = headers(client)
    sneaky = card(to="dl9zzt@example.de\nBcc: tutti@example.com")
    assert client.post("/v1/qsl/mail", json=sneaky, headers=token).status_code == 422
    assert client.post("/v1/qsl/mail", json=card(to="senza chiocciola"),
                       headers=token).status_code == 422
    assert outbox == []


def test_the_daily_ceiling_holds_and_says_how_many_are_left(client, outbox, monkeypatch):
    # Le impostazioni sono congelate: se ne fa una copia col tetto basso.
    monkeypatch.setattr(qslmail, "settings",
                        dataclasses.replace(qslmail.settings, qsl_daily_limit=3))
    token = headers(client, outbox=outbox)
    for expected in (2, 1, 0):
        reply = client.post("/v1/qsl/mail", json=card(), headers=token)
        assert reply.status_code == 200
        assert reply.json()["remaining"] == expected
    # La quarta no: il tetto e' quello che impedisce a una persona sola di
    # bruciare la reputazione del dominio per tutti.
    reply = client.post("/v1/qsl/mail", json=card(), headers=token)
    assert reply.status_code == 429
    assert "domani" in reply.json()["detail"]
    assert len(outbox) == 3


def test_one_station_cannot_eat_everyone_elses_share(client, outbox, monkeypatch):
    monkeypatch.setattr(qslmail, "settings",
                        dataclasses.replace(qslmail.settings, qsl_daily_limit=100,
                                            qsl_daily_total=2))
    mine = headers(client, "IU8LMC", outbox=outbox)
    assert client.post("/v1/qsl/mail", json=card(), headers=mine).status_code == 200
    assert client.post("/v1/qsl/mail", json=card(), headers=mine).status_code == 200
    # Il tetto di tutti e' finito: anche un altro nominativo aspetta domani.
    theirs = headers(client, "VK3ABC", outbox=outbox)
    assert client.post("/v1/qsl/mail", json=card(), headers=theirs).status_code == 429


def test_a_cloud_without_a_mailbox_says_so(client, outbox, monkeypatch):
    monkeypatch.setattr(mailer, "cards", lambda: mailer.Mailbox("", 465, "", ""))
    reply = client.post("/v1/qsl/mail", json=card(), headers=headers(client))
    assert reply.status_code == 503
    assert outbox == []


def test_health_says_whether_the_cloud_forwards_qsl(client):
    assert "qslmail" in client.get("/v1/health").json()["features"]


def test_the_card_mailbox_is_separate_from_the_notices_one(monkeypatch):
    # Se la casella delle QSL e' configurata, gli avvisi restano dove stavano:
    # una lista nera presa mandando cartoline non deve zittire il servizio.
    fake = dataclasses.replace(mailer.settings, qsl_smtp_host="smtps.aruba.it",
                               qsl_smtp_user="decodxlog@example.it",
                               qsl_smtp_password="segreta",
                               smtp_host="smtp.gmail.com", smtp_user="avvisi@example.com",
                               smtp_password="altra")
    monkeypatch.setattr(mailer, "settings", fake)
    assert mailer.cards().user == "decodxlog@example.it"
    assert mailer.notices().user == "avvisi@example.com"
    # Senza quella delle QSL si ricade sugli avvisi, invece di non mandare nulla.
    monkeypatch.setattr(mailer, "settings", dataclasses.replace(fake, qsl_smtp_host="", qsl_smtp_user=""))
    assert mailer.cards().user == "avvisi@example.com"
