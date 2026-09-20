"""DecoDXLog Cloud — configurazione del servizio.

Tutto da variabili d'ambiente, perche' il servizio gira in un container e le
scelte cambiano fra il portatile di chi sviluppa e il VPS che lo ospita.
"""

from __future__ import annotations

import os
from dataclasses import dataclass


def _int(name: str, default: int) -> int:
    try:
        return int(os.environ.get(name, default))
    except (TypeError, ValueError):
        return default


@dataclass(frozen=True)
class Settings:
    # PostgreSQL in servizio; SQLite basta a chi prova sul proprio computer.
    database_url: str = os.environ.get("DECOLOG_DATABASE_URL", "sqlite:///./decolog-cloud.sqlite")
    # Quanti QSO al massimo in una pagina di pull o in una spinta.
    page_size: int = _int("DECOLOG_PAGE_SIZE", 500)
    max_batch: int = _int("DECOLOG_MAX_BATCH", 1000)
    # Quanto dura un token prima che il client debba rifarlo.
    token_days: int = _int("DECOLOG_TOKEN_DAYS", 180)
    # La registrazione libera si chiude quando il servizio e' di una persona sola.
    allow_signup: bool = os.environ.get("DECOLOG_ALLOW_SIGNUP", "1") not in ("0", "false", "no")


settings = Settings()
