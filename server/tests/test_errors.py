"""DecoLog Cloud — gli errori detti con parole.

Un errore di validazione, da FastAPI, e' un elenco di oggetti: giusto per un
programma, illeggibile per chi sta davanti allo schermo — e un client che cerca
una frase in `detail` si ritrova con "status code 422" e nessuna idea di cosa
abbia sbagliato. E' successo per davvero: un utente con la password corta si e'
visto "Error transferring https://cloud.ft2.it/v1/auth/token - server replied
with status code 422".
"""

from __future__ import annotations

import os
import tempfile

import pytest

os.environ.setdefault("DECOLOG_DATABASE_URL", "sqlite:///" + os.path.join(tempfile.mkdtemp(), "e.sqlite"))

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


def test_a_short_password_says_so(client):
    reply = client.post("/v1/auth/token", json={"callsign": "IU8LMC", "password": "corta"})
    assert reply.status_code == 422
    detail = reply.json()["detail"]
    # Una frase, non un elenco: e' li' che il programma la cerca.
    assert isinstance(detail, str)
    assert "password" in detail and "8" in detail


def test_a_short_callsign_says_so(client):
    reply = client.post("/v1/auth/signup", json={"callsign": "IU", "password": "una password lunga"})
    assert reply.status_code == 422
    assert "nominativo" in reply.json()["detail"]


def test_a_missing_field_says_which_one(client):
    reply = client.post("/v1/auth/token", json={"callsign": "IU8LMC"})
    assert reply.status_code == 422
    detail = reply.json()["detail"]
    assert isinstance(detail, str) and "password" in detail


def test_more_than_one_thing_wrong_is_said_in_one_sentence(client):
    reply = client.post("/v1/auth/signup", json={"callsign": "IU", "password": "corta"})
    assert reply.status_code == 422
    detail = reply.json()["detail"]
    assert "nominativo" in detail and "password" in detail
    assert ";" in detail


def test_a_good_password_still_gets_the_usual_answer(client):
    # Le credenziali sbagliate restano un 401 con il messaggio di sempre: da
    # fuori non si deve capire quali nominativi esistono.
    client.post("/v1/auth/signup", json={"callsign": "IU8LMC", "password": "una password lunga"})
    reply = client.post("/v1/auth/token", json={"callsign": "IU8LMC", "password": "un'altra lunga"})
    assert reply.status_code == 401
    assert "non validi" in reply.json()["detail"]
