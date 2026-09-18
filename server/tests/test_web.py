"""DecoLog Cloud — il log dal browser.

Le pagine sono in sola lettura e sono di chi entra: queste prove guardano che
non si veda niente senza accesso, che la ricerca cerchi davvero e che l'ADIF
esca com'e' entrato.
"""

from __future__ import annotations

import os
import tempfile

import pytest

os.environ.setdefault("DECOLOG_DATABASE_URL", "sqlite:///" + os.path.join(tempfile.mkdtemp(), "w.sqlite"))

from fastapi.testclient import TestClient  # noqa: E402

from decolog_cloud import models  # noqa: E402
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
    assert reply.status_code == 200
    return {"Authorization": "Bearer " + reply.json()["token"]}


def qso(uuid, call="DL9ZZT", when="2026-09-18T07:00:00Z", **fields):
    return {
        "uuid": uuid,
        "revision": 1,
        "call": call,
        "band": "20m",
        "mode": "MFSK",
        "submode": "FT2",
        "startedAt": when,
        "fields": {"CALL": call, "QSO_DATE": "20260918", "TIME_ON": "070000",
                   "BAND": "20m", "MODE": "MFSK", "SUBMODE": "FT2", "RST_SENT": "599", **fields},
    }


def sign_in(client, callsign="IU8LMC"):
    reply = client.post("/login", data={"callsign": callsign, "password": PASSWORD},
                        follow_redirects=False)
    assert reply.status_code == 303
    assert reply.headers["location"] == "/log"
    return reply


def test_without_a_session_there_is_nothing_to_see(client):
    account(client)
    home = client.get("/", follow_redirects=False)
    assert home.status_code == 200
    assert "Nominativo" in home.text

    for path in ("/log", "/qso/u1", "/export.adi"):
        reply = client.get(path, follow_redirects=False)
        assert reply.status_code in (303, 404)
        if reply.status_code == 303:
            assert reply.headers["location"] == "/"
    assert client.get("/log/rows").status_code == 401


def test_wrong_password_says_the_same_thing_as_an_unknown_call(client):
    account(client)
    bad = client.post("/login", data={"callsign": "IU8LMC", "password": "sbagliata!"})
    unknown = client.post("/login", data={"callsign": "XX0XXX", "password": PASSWORD})
    assert bad.status_code == unknown.status_code == 401
    assert "non validi" in bad.text and "non validi" in unknown.text


def test_the_log_shows_the_qsos(client):
    headers = account(client)
    client.post("/v1/sync/push", json={"qsos": [qso("u1", "DL9ZZT", NAME="Klaus"),
                                                qso("u2", "EA5XYZ", when="2026-09-18T08:00:00Z")]},
                headers=headers)
    sign_in(client)

    page = client.get("/log")
    assert page.status_code == 200
    assert "DL9ZZT" in page.text
    assert "EA5XYZ" in page.text
    assert "Klaus" in page.text
    # Il totale sta nel suo riquadro: <span class="count">2</span> QSO.
    assert ">2</span> QSO" in page.text


def test_search_filters_the_rows(client):
    headers = account(client)
    client.post("/v1/sync/push", json={"qsos": [qso("u1", "DL9ZZT"), qso("u2", "EA5XYZ")]},
                headers=headers)
    sign_in(client)

    rows = client.get("/log/rows", params={"q": "ea5"})
    assert "EA5XYZ" in rows.text
    assert "DL9ZZT" not in rows.text

    by_band = client.get("/log/rows", params={"band": "40m"})
    assert "DL9ZZT" not in by_band.text


def test_the_detail_page_shows_every_field(client):
    headers = account(client)
    client.post("/v1/sync/push", json={"qsos": [qso("u1", GRIDSQUARE="JO62", COMMENT="bel segnale")]},
                headers=headers)
    sign_in(client)

    page = client.get("/qso/u1")
    assert page.status_code == 200
    assert "JO62" in page.text
    assert "bel segnale" in page.text
    assert "GRIDSQUARE" in page.text


def test_export_gives_back_adif(client):
    headers = account(client)
    client.post("/v1/sync/push", json={"qsos": [qso("u1", "DL9ZZT")]}, headers=headers)
    sign_in(client)

    reply = client.get("/export.adi")
    assert reply.status_code == 200
    assert "IU8LMC-cloud.adi" in reply.headers["content-disposition"]
    assert "<EOH>" in reply.text
    assert "<CALL:6>DL9ZZT" in reply.text
    assert reply.text.rstrip().endswith("<EOR>")


def test_one_account_does_not_see_another(client):
    mine = account(client, "IU8LMC")
    account(client, "DL9ZZT")
    client.post("/v1/sync/push", json={"qsos": [qso("u1", "W1AW")]}, headers=mine)

    sign_in(client, "DL9ZZT")
    page = client.get("/log")
    assert "W1AW" not in page.text
    assert "0</span> QSO" in page.text or "0 QSO" in page.text.replace("\n", " ")


def test_logout_closes_the_door(client):
    account(client)
    sign_in(client)
    assert client.get("/log").status_code == 200

    client.post("/logout", follow_redirects=False)
    assert client.get("/log", follow_redirects=False).headers["location"] == "/"


def test_the_station_page_shows_profiles_and_settings(client):
    headers = account(client)
    client.post(
        "/v1/sync/push",
        json={
            "docs": [
                {"kind": "profile", "key": "p1", "revision": 1,
                 "data": {"name": "Casa", "stationCallsign": "IU8LMC", "myGridsquare": "JN70",
                          "myRig": "FT-991A", "isDefault": True}},
                {"kind": "setting", "key": "station", "revision": 3,
                 "data": {"ui/language": "it", "theme/current": "Darkcodium", "udp/port": 2238,
                          "layout/savedFilters": {"__qvariant__": "AAAACAAAAAAA"}}},
            ]
        },
        headers=headers,
    )
    sign_in(client)

    page = client.get("/station")
    assert page.status_code == 200
    assert "Casa" in page.text
    assert "JN70" in page.text
    assert "FT-991A" in page.text
    assert "predefinito" in page.text
    assert "theme/current" in page.text and "Darkcodium" in page.text
    # Anche le porte: la stazione dev'essere la stessa, non una che le somiglia.
    assert "udp/port" in page.text and "2238" in page.text
    # Un filtro salvato non e' testo: si dice cos'e', non si vomita il blob.
    assert "valore interno di Qt" in page.text
    assert "AAAACAAAAAAA" not in page.text
    assert "Revisione 3" in page.text


def test_the_station_page_needs_a_session(client):
    account(client)
    reply = client.get("/station", follow_redirects=False)
    assert reply.status_code == 303 and reply.headers["location"] == "/"
