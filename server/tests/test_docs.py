"""DecoDXLog Cloud — i documenti: profili stazione e impostazioni.

Il log di una stazione non e' solo l'elenco dei QSO. Qui si controlla che
profili e impostazioni viaggino con le stesse regole, sullo stesso cursore.
"""

from __future__ import annotations

import os
import tempfile

import pytest

os.environ.setdefault("DECOLOG_DATABASE_URL", "sqlite:///" + os.path.join(tempfile.mkdtemp(), "d.sqlite"))

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


def account(client, callsign="IU8LMC"):
    reply = client.post("/v1/auth/signup", json={"callsign": callsign, "password": "una password lunga"})
    return {"Authorization": "Bearer " + reply.json()["token"]}


def profile(key="p1", revision=1, name="Casa", **extra):
    return {
        "kind": "profile",
        "key": key,
        "revision": revision,
        "data": {"name": name, "stationCallsign": "IU8LMC", "myGridsquare": "JN70", **extra},
    }


def test_a_profile_travels_like_a_qso(client):
    headers = account(client)
    reply = client.post("/v1/sync/push", json={"docs": [profile()]}, headers=headers)
    assert reply.status_code == 200
    assert reply.json()["docResults"][0]["status"] == "applied"

    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert pulled["qsos"] == []
    assert len(pulled["docs"]) == 1
    assert pulled["docs"][0]["kind"] == "profile"
    assert pulled["docs"][0]["data"]["myGridsquare"] == "JN70"


def test_qsos_and_documents_share_one_cursor(client):
    headers = account(client)
    client.post(
        "/v1/sync/push",
        json={
            "qsos": [{"uuid": "u1", "call": "DL9ZZT", "band": "20m", "mode": "CW",
                      "startedAt": "2026-09-18T07:00:00Z", "fields": {"CALL": "DL9ZZT"}}],
            "docs": [profile()],
        },
        headers=headers,
    )
    first = client.get("/v1/sync/pull", headers=headers).json()
    assert len(first["qsos"]) == 1 and len(first["docs"]) == 1

    # Dal cursore in poi non torna niente: un secondo dispositivo non riscarica
    # il mondo a ogni giro.
    again = client.get("/v1/sync/pull", params={"since": first["cursor"]}, headers=headers).json()
    assert again["qsos"] == [] and again["docs"] == []


def test_an_older_revision_does_not_win(client):
    headers = account(client)
    client.post("/v1/sync/push", json={"docs": [profile(revision=4, name="Nuovo")]}, headers=headers)
    reply = client.post("/v1/sync/push", json={"docs": [profile(revision=2, name="Vecchio")]}, headers=headers)
    assert reply.json()["docResults"][0]["status"] == "stale"

    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert pulled["docs"][0]["data"]["name"] == "Nuovo"


def test_a_conflict_keeps_the_loser(client):
    headers = account(client)
    client.post("/v1/sync/push", json={"docs": [profile(revision=1, name="Da casa")]}, headers=headers)
    reply = client.post("/v1/sync/push", json={"docs": [profile(revision=1, name="Dal portatile")]},
                        headers=headers)
    result = reply.json()["docResults"][0]
    assert result["status"] == "conflict"
    assert result["revision"] == 2

    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert pulled["docs"][0]["data"]["name"] == "Dal portatile"
    with models.SessionLocal() as db:
        kept = db.query(models.DocHistory).all()
        assert len(kept) == 1 and kept[0].data["name"] == "Da casa"


def test_settings_are_one_document(client):
    headers = account(client)
    values = {"ui/language": "it", "theme/name": "ocean", "awards/band": "20m"}
    client.post("/v1/sync/push",
                json={"docs": [{"kind": "setting", "key": "station", "revision": 1, "data": values}]},
                headers=headers)

    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert pulled["docs"][0]["key"] == "station"
    assert pulled["docs"][0]["data"]["theme/name"] == "ocean"


def test_a_deleted_profile_travels_too(client):
    headers = account(client)
    client.post("/v1/sync/push", json={"docs": [profile()]}, headers=headers)
    gone = dict(profile(revision=2), deleted=True)
    client.post("/v1/sync/push", json={"docs": [gone]}, headers=headers)

    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert pulled["docs"][0]["deleted"] is True


def test_documents_stay_inside_the_account(client):
    mine = account(client, "IU8LMC")
    yours = account(client, "DL9ZZT")
    client.post("/v1/sync/push", json={"docs": [profile()]}, headers=mine)
    assert client.get("/v1/sync/pull", headers=yours).json()["docs"] == []


def test_a_document_without_a_name_is_refused(client):
    headers = account(client)
    reply = client.post("/v1/sync/push",
                        json={"docs": [{"kind": "", "key": "", "revision": 1, "data": {}}]},
                        headers=headers)
    assert reply.json()["docResults"][0]["status"] == "rejected"
