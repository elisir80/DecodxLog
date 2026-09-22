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
    # La porta resta aperta, ma chi entra lo decide una persona: l'account nasce
    # in attesa e il sync gli risponde di no finche' non e' approvato. A zero si
    # torna a com'era, cioe' dentro subito.
    approval_required: bool = os.environ.get("DECOLOG_APPROVAL", "1") not in ("0", "false", "no")
    # Dove sta il servizio visto da fuori: serve a scrivere i collegamenti
    # nell'email, che si aprono dal telefono.
    public_url: str = os.environ.get("DECOLOG_PUBLIC_URL", "https://cloud.ft2.it").rstrip("/")

    # ── L'email di avviso ────────────────────────────────────────────────────
    # Senza queste, l'avviso non parte e resta una riga nel registro: un server
    # non si deve piantare perche' non riesce a mandare una posta.
    smtp_host: str = os.environ.get("DECOLOG_SMTP_HOST", "smtp.gmail.com")
    smtp_port: int = _int("DECOLOG_SMTP_PORT", 587)
    smtp_user: str = os.environ.get("DECOLOG_SMTP_USER", "")
    smtp_password: str = os.environ.get("DECOLOG_SMTP_PASSWORD", "")
    notify_email: str = os.environ.get("DECOLOG_NOTIFY_EMAIL", "")
    # Una email per indirizzo ogni tanti minuti: chi prova cinquanta nominativi
    # di fila non riempie la casella di nessuno.
    notify_minutes: int = _int("DECOLOG_SIGNUP_NOTIFY_MINUTES", 10)

    # ── Le QSL che passano di qui ────────────────────────────────────────────
    # Una casella a parte da quella degli avvisi: le QSL vanno a sconosciuti, e
    # se finisce in lista nera non deve portarsi dietro gli avvisi di servizio.
    # Senza queste, l'inoltro delle QSL e' spento e il programma lo dice.
    qsl_smtp_host: str = os.environ.get("DECOLOG_QSL_SMTP_HOST", "")
    qsl_smtp_port: int = _int("DECOLOG_QSL_SMTP_PORT", 465)
    qsl_smtp_user: str = os.environ.get("DECOLOG_QSL_SMTP_USER", "")
    qsl_smtp_password: str = os.environ.get("DECOLOG_QSL_SMTP_PASSWORD", "")
    qsl_from_name: str = os.environ.get("DECOLOG_QSL_FROM_NAME", "DecoDXLog")
    # Quante cartoline al giorno per nominativo: e' il freno che impedisce a una
    # persona sola di bruciare la reputazione del dominio per tutti.
    qsl_daily_limit: int = _int("DECOLOG_QSL_DAILY_LIMIT", 100)
    # E quante in tutto, da tutti, nello stesso giorno.
    qsl_daily_total: int = _int("DECOLOG_QSL_DAILY_TOTAL", 500)


settings = Settings()
