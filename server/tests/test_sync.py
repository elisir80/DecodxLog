"""DecoLog Cloud — le regole del sync, provate sul serio.

Ogni prova parte da un database vuoto: il servizio si comporta allo stesso modo
al primo QSO e al centomillesimo.
"""

from __future__ import annotations

import datetime as dt
import os
import tempfile

import pytest

os.environ.setdefault("DECOLOG_DATABASE_URL", "sqlite:///" + os.path.join(tempfile.mkdtemp(), "t.sqlite"))

from fastapi.testclient import TestClient  # noqa: E402

from decolog_cloud import models  # noqa: E402
from decolog_cloud.main import app  # noqa: E402
from decolog_cloud.sync import mode_group  # noqa: E402


@pytest.fixture()
def client(tmp_path, monkeypatch):
    # Un database per prova, nel suo temporaneo.
    url = "sqlite:///" + str(tmp_path / "cloud.sqlite").replace("\\", "/")
    engine = models.create_engine(url, future=True, connect_args={"check_same_thread": False})
    models.engine = engine
    models.SessionLocal.configure(bind=engine)
    models.Base.metadata.create_all(engine)
    with TestClient(app) as c:
        yield c


def signup(client, callsign="IU8LMC", password="una password lunga"):
    reply = client.post("/v1/auth/signup", json={"callsign": callsign, "password": password, "device": "prova"})
    assert reply.status_code == 200, reply.text
    return {"Authorization": "Bearer " + reply.json()["token"]}


def qso(uuid, call="DL9ZZT", revision=1, when="2026-09-18T07:00:00Z", **fields):
    return {
        "uuid": uuid,
        "revision": revision,
        "call": call,
        "band": "20m",
        "mode": "MFSK",
        "submode": "FT2",
        "startedAt": when,
        "fields": {"CALL": call, "BAND": "20m", "MODE": "MFSK", "SUBMODE": "FT2", **fields},
    }


def test_health(client):
    body = client.get("/v1/health").json()
    assert body["status"] == "ok"
    # Dice anche cosa sa fare: update.sh se ne serve per accorgersi di un
    # aggiornamento rimasto a meta'.
    assert {"qso", "docs", "web", "stats"} <= set(body["features"])


def test_signup_then_token(client):
    signup(client)
    again = client.post("/v1/auth/signup", json={"callsign": "IU8LMC", "password": "una password lunga"})
    assert again.status_code == 409

    ok = client.post("/v1/auth/token", json={"callsign": "iu8lmc", "password": "una password lunga"})
    assert ok.status_code == 200
    assert ok.json()["callsign"] == "IU8LMC"

    wrong = client.post("/v1/auth/token", json={"callsign": "IU8LMC", "password": "non e' quella"})
    assert wrong.status_code == 401
    # Nominativo inesistente: stessa risposta, per non dire chi c'e' e chi no.
    unknown = client.post("/v1/auth/token", json={"callsign": "XX0XXX", "password": "una password lunga"})
    assert unknown.status_code == 401
    assert unknown.json()["detail"] == wrong.json()["detail"]


def test_push_needs_a_token(client):
    assert client.post("/v1/sync/push", json={"qsos": []}).status_code == 401
    assert client.get("/v1/sync/pull").status_code == 401


def test_push_and_pull(client):
    headers = signup(client)
    reply = client.post("/v1/sync/push", json={"device": "shack", "qsos": [qso("u1"), qso("u2", "EA5XYZ")]},
                        headers=headers)
    assert reply.status_code == 200
    assert [r["status"] for r in reply.json()["results"]] == ["applied", "applied"]

    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert len(pulled["qsos"]) == 2
    assert pulled["more"] is False
    assert pulled["cursor"] == reply.json()["cursor"]

    # Dal cursore in poi non c'e' piu' niente: il secondo giro non riscarica tutto.
    again = client.get("/v1/sync/pull", params={"since": pulled["cursor"]}, headers=headers).json()
    assert again["qsos"] == []


def test_same_uuid_twice_is_not_two_qso(client):
    headers = signup(client)
    for _ in range(3):
        client.post("/v1/sync/push", json={"qsos": [qso("u1")]}, headers=headers)
    assert client.get("/v1/sync/status", headers=headers).json()["qsos"] == 1


def test_conflict_keeps_the_loser_in_history(client):
    headers = signup(client)
    client.post("/v1/sync/push", json={"qsos": [qso("u1", revision=1, NAME="Primo")]}, headers=headers)
    # Un altro dispositivo, che non sapeva della modifica, manda la stessa
    # revisione con un contenuto diverso.
    reply = client.post("/v1/sync/push", json={"qsos": [qso("u1", revision=1, NAME="Secondo")]}, headers=headers)
    result = reply.json()["results"][0]
    assert result["status"] == "conflict"
    assert result["revision"] == 2

    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert pulled["qsos"][0]["fields"]["NAME"] == "Secondo"

    with models.SessionLocal() as db:
        kept = db.query(models.QsoHistory).all()
        assert len(kept) == 1
        assert kept[0].fields["NAME"] == "Primo"


def test_old_revision_does_not_overwrite(client):
    headers = signup(client)
    client.post("/v1/sync/push", json={"qsos": [qso("u1", revision=5, NAME="Nuovo")]}, headers=headers)
    reply = client.post("/v1/sync/push", json={"qsos": [qso("u1", revision=2, NAME="Vecchio")]}, headers=headers)
    assert reply.json()["results"][0]["status"] == "stale"
    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert pulled["qsos"][0]["fields"]["NAME"] == "Nuovo"


def test_duplicate_with_another_uuid_is_recognised(client):
    headers = signup(client)
    client.post("/v1/sync/push", json={"qsos": [qso("u1", when="2026-09-18T07:00:00Z")]}, headers=headers)
    # Stesso nominativo, banda e gruppo di modi, un minuto dopo: e' lo stesso QSO.
    reply = client.post("/v1/sync/push", json={"qsos": [qso("u2", when="2026-09-18T07:01:00Z")]}, headers=headers)
    result = reply.json()["results"][0]
    assert result["status"] == "duplicate"
    assert result["serverUuid"] == "u1"
    assert client.get("/v1/sync/status", headers=headers).json()["qsos"] == 1

    # Mezz'ora dopo e' un collegamento diverso, e va tenuto.
    later = client.post("/v1/sync/push", json={"qsos": [qso("u3", when="2026-09-18T07:30:00Z")]}, headers=headers)
    assert later.json()["results"][0]["status"] == "applied"


def test_delete_travels_as_a_change(client):
    headers = signup(client)
    client.post("/v1/sync/push", json={"qsos": [qso("u1")]}, headers=headers)
    gone = dict(qso("u1", revision=2), deleted=True)
    client.post("/v1/sync/push", json={"qsos": [gone]}, headers=headers)

    pulled = client.get("/v1/sync/pull", headers=headers).json()
    assert pulled["qsos"][0]["deleted"] is True
    status = client.get("/v1/sync/status", headers=headers).json()
    assert status["qsos"] == 0 and status["deleted"] == 1


def test_pull_is_paged(client):
    headers = signup(client)
    batch = [qso(f"u{i}", call=f"IK{i}ABC", when=f"2026-09-18T07:{i:02d}:00Z") for i in range(6)]
    client.post("/v1/sync/push", json={"qsos": batch}, headers=headers)

    first = client.get("/v1/sync/pull", params={"limit": 4}, headers=headers).json()
    assert len(first["qsos"]) == 4 and first["more"] is True
    second = client.get("/v1/sync/pull", params={"since": first["cursor"], "limit": 4}, headers=headers).json()
    assert len(second["qsos"]) == 2 and second["more"] is False


def test_accounts_do_not_see_each_other(client):
    mine = signup(client, "IU8LMC")
    yours = signup(client, "DL9ZZT")
    client.post("/v1/sync/push", json={"qsos": [qso("u1")]}, headers=mine)
    assert client.get("/v1/sync/pull", headers=yours).json()["qsos"] == []


def test_mode_groups():
    assert mode_group("MFSK", "FT2") == "DATA"
    assert mode_group("CW") == "CW"
    assert mode_group("SSB") == "PHONE"
    assert mode_group("RTTY") == "DATA"


def test_token_expiry_is_checked(client):
    headers = signup(client)
    with models.SessionLocal() as db:
        token = db.query(models.Token).first()
        token.expires_at = dt.datetime.now(dt.UTC) - dt.timedelta(days=1)
        db.commit()
    assert client.get("/v1/sync/status", headers=headers).status_code == 401


def test_purge_empties_the_cloud_only_when_you_write_delete(client):
    headers = signup(client)
    client.post("/v1/sync/push", headers=headers, json={"qsos": [qso("p-1", when="2026-09-18T07:00:00Z"),
                                                        qso("p-2", when="2026-09-18T08:30:00Z")]})
    assert client.get("/v1/sync/status", headers=headers).json()["qsos"] == 2

    # Senza la parola giusta non si cancella niente.
    refused = client.post("/v1/account/purge", headers=headers, json={"confirm": "si"})
    assert refused.status_code == 400
    assert "DELETE" in refused.json()["detail"]
    assert client.get("/v1/sync/status", headers=headers).json()["qsos"] == 2

    done = client.post("/v1/account/purge", headers=headers, json={"confirm": "DELETE"})
    assert done.status_code == 200
    assert done.json()["deleted"]["qsos"] == 2

    # Il log e' vuoto e il cursore riparte da zero, ma si entra ancora.
    after = client.get("/v1/sync/status", headers=headers).json()
    assert after["qsos"] == 0 and after["deleted"] == 0 and after["cursor"] == 0
    assert client.get("/v1/sync/pull", headers=headers).json()["qsos"] == []
    # E quello che si manda dopo torna a contare da uno.
    client.post("/v1/sync/push", headers=headers, json={"qsos": [qso("p-3", when="2026-09-18T09:45:00Z")]})
    assert client.get("/v1/sync/status", headers=headers).json()["qsos"] == 1


def test_purge_does_not_touch_the_other_callsigns(client):
    mine = signup(client, "IU8LMC")
    theirs = signup(client, "DL9ZZT")
    client.post("/v1/sync/push", headers=mine, json={"qsos": [qso("m-1")]})
    client.post("/v1/sync/push", headers=theirs, json={"qsos": [qso("t-1")]})

    client.post("/v1/account/purge", headers=mine, json={"confirm": "DELETE"})
    assert client.get("/v1/sync/status", headers=mine).json()["qsos"] == 0
    assert client.get("/v1/sync/status", headers=theirs).json()["qsos"] == 1
