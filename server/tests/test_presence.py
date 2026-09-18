"""DecoLog Cloud — dov'e' la stazione adesso.

La frequenza non e' log: cambia a ogni giro di VFO, non ha storia e non deve
svegliare gli altri dispositivi. Queste prove guardano proprio quello — che si
riscriva sopra, che non muova il cursore del sync, e che invecchiando torni a
essere trattini.
"""

from __future__ import annotations

import datetime as dt
import os
import tempfile

import pytest

os.environ.setdefault("DECOLOG_DATABASE_URL", "sqlite:///" + os.path.join(tempfile.mkdtemp(), "p.sqlite"))

from fastapi.testclient import TestClient  # noqa: E402

from decolog_cloud import models, web  # noqa: E402
from decolog_cloud.main import app  # noqa: E402


@pytest.fixture()
def client(tmp_path):
    url = "sqlite:///" + str(tmp_path / "cloud.sqlite").replace("\\", "/")
    engine = models.create_engine(url, future=True, connect_args={"check_same_thread": False})
    models.engine = engine
    models.SessionLocal.configure(bind=engine)
    models.Base.metadata.create_all(engine)
    with TestClient(app) as c:
        yield c


PASSWORD = "una password lunga"


def account(client, callsign="IU8LMC"):
    reply = client.post("/v1/auth/signup", json={"callsign": callsign, "password": PASSWORD})
    return {"Authorization": "Bearer " + reply.json()["token"]}


def sign_in(client, callsign="IU8LMC"):
    reply = client.post("/login", data={"callsign": callsign, "password": PASSWORD},
                        follow_redirects=False)
    assert reply.status_code == 303


IN_AIR = {"device": "shack", "frequencyHz": 14074000, "band": "20m", "mode": "FT8",
          "dxCall": "DL9ZZT", "transmitting": False, "client": "Decodium"}


def test_the_frequency_arrives_and_shows_up(client):
    headers = account(client)
    assert client.post("/v1/presence", json=IN_AIR, headers=headers).json() == {"ok": True}
    sign_in(client)

    page = client.get("/log")
    assert "14.074000 MHz" in page.text
    assert "FT8" in page.text and "DL9ZZT" in page.text


def test_it_is_overwritten_not_piled_up(client):
    headers = account(client)
    client.post("/v1/presence", json=IN_AIR, headers=headers)
    client.post("/v1/presence", json={**IN_AIR, "frequencyHz": 7074000, "band": "40m"},
                headers=headers)

    from decolog_cloud.models import Presence

    with models.SessionLocal() as db:
        rows = db.query(Presence).all()
        assert len(rows) == 1          # una riga per dispositivo, riscritta sopra
        assert rows[0].frequency_hz == 7074000


def test_the_vfo_does_not_wake_the_other_devices(client):
    headers = account(client)
    # Un QSO muove il cursore; la frequenza no, altrimenti ogni giro di VFO
    # farebbe scaricare tutto agli altri dispositivi.
    before = client.get("/v1/sync/status", headers=headers).json()["cursor"]
    for hz in (14074000, 14075000, 14076000):
        client.post("/v1/presence", json={**IN_AIR, "frequencyHz": hz}, headers=headers)
    after = client.get("/v1/sync/status", headers=headers).json()["cursor"]
    assert after == before

    pulled = client.get("/v1/sync/pull", params={"since": 0}, headers=headers).json()
    assert pulled["qsos"] == [] and pulled["docs"] == []


def test_transmitting_is_said(client):
    headers = account(client)
    client.post("/v1/presence", json={**IN_AIR, "transmitting": True}, headers=headers)
    sign_in(client)
    assert "TX" in client.get("/log").text


def test_a_station_that_went_quiet_is_not_on_air_any_more(client):
    headers = account(client)
    client.post("/v1/presence", json=IN_AIR, headers=headers)

    from decolog_cloud.models import Presence

    # Come se il computer di casa tacesse da mezz'ora.
    with models.SessionLocal() as db:
        row = db.query(Presence).first()
        row.updated_at = dt.datetime.now(dt.UTC) - dt.timedelta(minutes=30)
        db.commit()

    sign_in(client)
    page = client.get("/log")
    assert "14.074000 MHz" not in page.text
    assert "--.------ MHz" in page.text


def test_presence_needs_a_token(client):
    assert client.post("/v1/presence", json=IN_AIR).status_code == 401


def test_one_station_does_not_see_another(client):
    mine = account(client, "IU8LMC")
    account(client, "DL9ZZT")
    client.post("/v1/presence", json=IN_AIR, headers=mine)

    sign_in(client, "DL9ZZT")
    assert "14.074000 MHz" not in client.get("/log").text


def test_nonsense_does_not_get_stored(client):
    headers = account(client)
    # Una frequenza impossibile non passa la validazione.
    reply = client.post("/v1/presence", json={**IN_AIR, "frequencyHz": -1}, headers=headers)
    assert reply.status_code == 422
    assert isinstance(reply.json()["detail"], str)


def test_the_page_says_when_it_was_last_heard(client):
    headers = account(client)
    client.post("/v1/presence", json=IN_AIR, headers=headers)
    sign_in(client)
    # Il titolo della pillola dice chi sta parlando e da dove.
    page = client.get("/log")
    assert "Decodium" in page.text and "shack" in page.text
    assert web.PRESENCE_FRESH == dt.timedelta(minutes=3)
