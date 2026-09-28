"""Un log per piu' operatori: inviti, ruoli, e che i documenti restino di chi tiene il log."""

from __future__ import annotations

import os
import tempfile

import pytest

os.environ.setdefault("DECOLOG_DATABASE_URL", "sqlite:///" + os.path.join(tempfile.mkdtemp(), "t.sqlite"))

from fastapi.testclient import TestClient  # noqa: E402

from decolog_cloud import models  # noqa: E402
from decolog_cloud.main import app  # noqa: E402

from .helpers import approve  # noqa: E402


@pytest.fixture()
def client(tmp_path):
    url = "sqlite:///" + str(tmp_path / "cloud.sqlite").replace("\\", "/")
    engine = models.create_engine(url, future=True, connect_args={"check_same_thread": False})
    models.engine = engine
    models.SessionLocal.configure(bind=engine)
    models.Base.metadata.create_all(engine)
    with TestClient(app) as c:
        yield c


def signup(client, callsign):
    reply = client.post("/v1/auth/signup", json={"callsign": callsign, "password": "una password lunga",
                                                 "device": "shack"})
    assert reply.status_code == 200, reply.text
    approve(client, callsign)
    return {"Authorization": "Bearer " + reply.json()["token"]}


def qso(uuid, call, when="2026-10-24T00:01:00Z"):
    return {"uuid": uuid, "revision": 1, "call": call, "band": "20m", "mode": "CW", "startedAt": when,
            "fields": {"CALL": call, "BAND": "20m", "MODE": "CW"}}


def test_an_operator_writes_in_the_club_log(client):
    club = signup(client, "IQ8XX")
    op = signup(client, "IU8LMC")

    # Il club crea l'invito; il codice si legge adesso e basta.
    invite = client.post("/v1/team/invite", json={"role": "operator"}, headers=club).json()
    assert len(invite["code"]) == 14 and invite["log"] == "IQ8XX"
    # Prima di entrare, il log del club non si tocca.
    shared = {**op, "X-DecoLog-Log": "IQ8XX"}
    assert client.get("/v1/sync/pull", headers=shared).status_code == 403

    # Il codice si scrive come viene: minuscole, senza trattini.
    joined = client.post("/v1/team/join", json={"code": invite["code"].replace("-", "").lower()}, headers=op)
    assert joined.status_code == 200, joined.text
    assert joined.json() == {"log": "IQ8XX", "role": "operator"}
    # Vale una volta sola.
    again = signup(client, "IZ8AAA")
    assert client.post("/v1/team/join", json={"code": invite["code"]}, headers=again).status_code == 404

    # L'operatore scrive nel log del club...
    pushed = client.post("/v1/sync/push", json={"device": "portatile", "qsos": [qso("a-1", "K1ABC")],
                                                "docs": [{"kind": "setting", "key": "station",
                                                          "data": {"theme/current": "mio"}}]},
                         headers=shared)
    assert pushed.status_code == 200, pushed.text
    assert pushed.json()["results"][0]["status"] == "applied"
    # ...ma le impostazioni restano del club.
    assert pushed.json()["docResults"][0]["status"] == "ignored"

    # Il club lo vede nel suo log, con chi l'ha scritto.
    pulled = client.get("/v1/sync/pull", headers=club).json()
    assert [q["uuid"] for q in pulled["qsos"]] == ["a-1"]
    with models.SessionLocal() as db:
        row = db.query(models.Qso).filter(models.Qso.uuid == "a-1").one()
        assert row.device == "IU8LMC · portatile"
        assert db.get(models.Account, row.account_id).callsign == "IQ8XX"

    # Il log personale dell'operatore resta vuoto.
    assert client.get("/v1/sync/pull", headers=op).json()["qsos"] == []
    # E l'operatore dal log del club non riceve i documenti del club.
    client.post("/v1/sync/push", json={"docs": [{"kind": "setting", "key": "station",
                                                "data": {"theme/current": "club"}}]}, headers=club)
    assert client.get("/v1/sync/pull", headers=shared).json()["docs"] == []
    assert client.get("/v1/sync/status", headers=shared).json()["callsign"] == "IQ8XX"


def test_a_viewer_only_reads(client):
    club = signup(client, "IQ8XX")
    judge = signup(client, "IK8JDG")
    code = client.post("/v1/team/invite", json={"role": "viewer"}, headers=club).json()["code"]
    assert client.post("/v1/team/join", json={"code": code}, headers=judge).status_code == 200
    client.post("/v1/sync/push", json={"qsos": [qso("c-1", "DL1AA")]}, headers=club)

    shared = {**judge, "X-DecoLog-Log": "IQ8XX"}
    assert [q["uuid"] for q in client.get("/v1/sync/pull", headers=shared).json()["qsos"]] == ["c-1"]
    refused = client.post("/v1/sync/push", json={"qsos": [qso("j-1", "W1AW")]}, headers=shared)
    assert refused.status_code == 403
    assert "solo guardare" in refused.json()["detail"]


def test_who_is_in_and_out(client):
    club = signup(client, "IQ8XX")
    op = signup(client, "IU8LMC")
    first = client.post("/v1/team/invite", json={"role": "operator", "days": 3}, headers=club).json()
    second = client.post("/v1/team/invite", json={"role": "viewer"}, headers=club).json()
    client.post("/v1/team/join", json={"code": first["code"]}, headers=op)

    team = client.get("/v1/team", headers=club).json()
    assert [m["callsign"] for m in team["members"]] == ["IU8LMC"]
    # Resta aperto solo l'invito non usato.
    assert [i["id"] for i in team["invites"]] == [second["id"]]
    mine = client.get("/v1/team", headers=op).json()
    assert mine["memberships"] == [{"log": "IQ8XX", "role": "operator", "since": mine["memberships"][0]["since"]}]

    # Un invito al proprio log non serve, un ruolo inventato nemmeno.
    own = client.post("/v1/team/invite", json={}, headers=op).json()["code"]
    assert client.post("/v1/team/join", json={"code": own}, headers=op).status_code == 400
    assert client.post("/v1/team/invite", json={"role": "admin"}, headers=club).status_code == 400

    # Il club cancella l'invito rimasto e toglie l'operatore.
    assert client.delete(f"/v1/team/invites/{second['id']}", headers=club).status_code == 200
    assert client.delete("/v1/team/members/iu8lmc", headers=club).status_code == 200
    assert client.get("/v1/sync/pull", headers={**op, "X-DecoLog-Log": "IQ8XX"}).status_code == 403
    assert client.get("/v1/team", headers=club).json()["members"] == []

    # Chi esce da se'.
    third = client.post("/v1/team/invite", json={}, headers=club).json()["code"]
    client.post("/v1/team/join", json={"code": third}, headers=op)
    assert client.delete("/v1/team/memberships/IQ8XX", headers=op).status_code == 200
    assert client.get("/v1/team", headers=op).json()["memberships"] == []
    # Un log che non esiste risponde come uno dove non si e' invitati.
    assert client.get("/v1/sync/pull", headers={**op, "X-DecoLog-Log": "NOPE"}).status_code == 403


def test_the_service_says_it_knows_teams(client):
    assert "team" in client.get("/v1/health").json()["features"]


def test_the_club_log_from_the_browser(client):
    club = signup(client, "IQ8XX")
    signup(client, "IU8LMC")
    client.post("/v1/sync/push", json={"qsos": [qso("w-1", "JA1XYZ")]}, headers=club)

    # Il club entra dal browser e crea un invito: il codice si legge nella pagina.
    assert client.post("/login", data={"callsign": "IQ8XX", "password": "una password lunga"},
                       follow_redirects=False).status_code == 303
    page = client.post("/team/invite", data={"role": "operator"})
    assert page.status_code == 200
    import re
    code = re.search(r"[A-Z2-9]{4}-[A-Z2-9]{4}-[A-Z2-9]{4}", page.text).group(0)
    client.post("/logout")

    # L'operatore entra con il codice, poi dal browser apre il log del club.
    token = client.post("/v1/auth/token", json={"callsign": "IU8LMC", "password": "una password lunga"}).json()
    op = {"Authorization": "Bearer " + token["token"]}
    assert client.post("/v1/team/join", json={"code": code}, headers=op).status_code == 200
    client.post("/login", data={"callsign": "IU8LMC", "password": "una password lunga"})
    assert "JA1XYZ" not in client.get("/log").text
    opened = client.get("/team/open/IQ8XX", follow_redirects=False)
    assert opened.status_code == 303
    assert "JA1XYZ" in client.get("/log").text
    # Le impostazioni del club non si cambiano da un operatore.
    station = client.get("/station")
    assert "Stai guardando il log di" in station.text
    client.post("/station/settings", data={"theme.current": "Darkcodium"})
    with models.SessionLocal() as db:
        owner = db.query(models.Account).filter(models.Account.callsign == "IQ8XX").one()
        assert db.query(models.Doc).filter(models.Doc.account_id == owner.id).count() == 0
    # E si torna al proprio.
    client.get("/team/open/-")
    assert "JA1XYZ" not in client.get("/log").text
