"""DecoDXLog Cloud — le tabelle.

Il server non ricostruisce il log: tiene il QSO come l'ha mandato il client (i
campi ADIF in un documento JSON) piu' i pochi dati che servono al sync e alla
ricerca — uuid, revisione, quando e' cambiato, se e' cancellato.

Cosi' il giorno che DecoDXLog impara un campo nuovo il server non va toccato.
"""

from __future__ import annotations

import datetime as dt
import uuid as uuidlib

from sqlalchemy import (
    BigInteger,
    Boolean,
    DateTime,
    ForeignKey,
    Index,
    Integer,
    String,
    Text,
    UniqueConstraint,
    create_engine,
)
from sqlalchemy.orm import DeclarativeBase, Mapped, mapped_column, relationship, sessionmaker
from sqlalchemy.types import JSON

from .settings import settings


class Base(DeclarativeBase):
    pass


def _now() -> dt.datetime:
    return dt.datetime.now(dt.UTC)


def new_uuid() -> str:
    return str(uuidlib.uuid4())


class Account(Base):
    """Un operatore. Il nominativo e' il nome utente: qui non c'e' altro da sapere.

    `approved` dice se puo' usare il servizio: chi si registra nasce in attesa e
    ci entra quando qualcuno, leggendo l'email di avviso, dice di si'.
    `approval_token` e' la chiave che sta in quel collegamento, e si cancella
    appena la decisione e' presa: il collegamento vale una volta sola.
    """

    __tablename__ = "account"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    callsign: Mapped[str] = mapped_column(String(32), unique=True, index=True)
    password_hash: Mapped[str] = mapped_column(Text)
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)
    approved: Mapped[bool] = mapped_column(Boolean, default=False)
    approved_at: Mapped[dt.datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    approval_token: Mapped[str] = mapped_column(String(64), default="")
    # Da dove e' arrivata la registrazione: nell'email serve a capire chi e'.
    signup_ip: Mapped[str] = mapped_column(String(64), default="")

    tokens: Mapped[list["Token"]] = relationship(back_populates="account", cascade="all, delete-orphan")


class Token(Base):
    """Un dispositivo collegato. Del token si tiene solo l'impronta."""

    __tablename__ = "token"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    account_id: Mapped[int] = mapped_column(ForeignKey("account.id", ondelete="CASCADE"), index=True)
    token_hash: Mapped[str] = mapped_column(String(64), unique=True, index=True)
    device: Mapped[str] = mapped_column(String(120), default="")
    created_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)
    last_seen: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)
    expires_at: Mapped[dt.datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)

    account: Mapped[Account] = relationship(back_populates="tokens")


class Qso(Base):
    """Un QSO come sta sul server.

    `seq` e' il numero che ordina le modifiche: il cursore del pull e' quello,
    non l'orario, cosi' due dispositivi che scrivono nello stesso secondo non si
    perdono per strada.
    """

    __tablename__ = "qso"
    __table_args__ = (
        UniqueConstraint("account_id", "uuid", name="uq_qso_account_uuid"),
        Index("ix_qso_account_seq", "account_id", "seq"),
        Index("ix_qso_dedup", "account_id", "call", "band", "mode_group"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    account_id: Mapped[int] = mapped_column(ForeignKey("account.id", ondelete="CASCADE"), index=True)
    uuid: Mapped[str] = mapped_column(String(36), index=True)
    revision: Mapped[int] = mapped_column(Integer, default=1)
    seq: Mapped[int] = mapped_column(BigInteger, index=True)
    deleted: Mapped[bool] = mapped_column(Boolean, default=False)
    updated_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)
    # Quello che serve per il confronto dei duplicati, estratto dai campi.
    call: Mapped[str] = mapped_column(String(32), default="")
    band: Mapped[str] = mapped_column(String(16), default="")
    mode_group: Mapped[str] = mapped_column(String(16), default="")
    started_at: Mapped[dt.datetime | None] = mapped_column(DateTime(timezone=True), nullable=True)
    # Il QSO intero, campi ADIF compresi.
    fields: Mapped[dict] = mapped_column(JSON, default=dict)
    # Chi l'ha mandato per ultimo: utile in diagnostica.
    device: Mapped[str] = mapped_column(String(120), default="")


class QsoHistory(Base):
    """La versione che ha perso un conflitto: non si butta via niente."""

    __tablename__ = "qso_history"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    account_id: Mapped[int] = mapped_column(ForeignKey("account.id", ondelete="CASCADE"), index=True)
    uuid: Mapped[str] = mapped_column(String(36), index=True)
    revision: Mapped[int] = mapped_column(Integer)
    recorded_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)
    reason: Mapped[str] = mapped_column(String(32), default="conflict_lost")
    fields: Mapped[dict] = mapped_column(JSON, default=dict)


class Doc(Base):
    """Tutto quello che non e' un QSO ma fa parte del log di una stazione.

    Profili stazione, impostazioni, filtri salvati, regole d'avviso: cose
    diverse fra loro, che al server interessano allo stesso modo — un documento
    con un nome (`kind`/`key`), una revisione e il suo contenuto. Cosi' quando
    DecoDXLog impara a tenersi un'altra cosa, qui non si tocca niente.

    Condividono con i QSO lo stesso contatore: un pull solo porta tutto.
    """

    __tablename__ = "doc"
    __table_args__ = (
        UniqueConstraint("account_id", "kind", "key", name="uq_doc_account_kind_key"),
        Index("ix_doc_account_seq", "account_id", "seq"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    account_id: Mapped[int] = mapped_column(ForeignKey("account.id", ondelete="CASCADE"), index=True)
    kind: Mapped[str] = mapped_column(String(32), index=True)   # profile | setting | filter | alert
    key: Mapped[str] = mapped_column(String(120))               # uuid del profilo, nome dell'impostazione
    revision: Mapped[int] = mapped_column(Integer, default=1)
    seq: Mapped[int] = mapped_column(BigInteger, index=True)
    deleted: Mapped[bool] = mapped_column(Boolean, default=False)
    updated_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)
    data: Mapped[dict] = mapped_column(JSON, default=dict)
    device: Mapped[str] = mapped_column(String(120), default="")


class DocHistory(Base):
    """La versione di un documento che ha perso un conflitto."""

    __tablename__ = "doc_history"

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    account_id: Mapped[int] = mapped_column(ForeignKey("account.id", ondelete="CASCADE"), index=True)
    kind: Mapped[str] = mapped_column(String(32))
    key: Mapped[str] = mapped_column(String(120))
    revision: Mapped[int] = mapped_column(Integer)
    recorded_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)
    data: Mapped[dict] = mapped_column(JSON, default=dict)


class Presence(Base):
    """Dov'e' la stazione adesso: frequenza, banda, modo, se sta trasmettendo.

    Non e' log e non e' un documento: e' *ora*. Cambia ogni pochi secondi, non ha
    storia e non fa numero nel cursore del sync — altrimenti ogni giro di VFO
    sveglierebbe tutti i dispositivi. Una riga per dispositivo, riscritta sopra;
    se smette di arrivare, quella riga invecchia e la stazione risulta spenta.
    """

    __tablename__ = "presence"
    __table_args__ = (
        UniqueConstraint("account_id", "device", name="uq_presence_account_device"),
    )

    id: Mapped[int] = mapped_column(Integer, primary_key=True)
    account_id: Mapped[int] = mapped_column(ForeignKey("account.id", ondelete="CASCADE"), index=True)
    device: Mapped[str] = mapped_column(String(120), default="")
    updated_at: Mapped[dt.datetime] = mapped_column(DateTime(timezone=True), default=_now)
    frequency_hz: Mapped[int] = mapped_column(BigInteger, default=0)
    band: Mapped[str] = mapped_column(String(16), default="")
    mode: Mapped[str] = mapped_column(String(32), default="")
    dx_call: Mapped[str] = mapped_column(String(32), default="")
    transmitting: Mapped[bool] = mapped_column(Boolean, default=False)
    # Il programma che sta in aria: Decodium, WSJT-X, JTDX...
    client: Mapped[str] = mapped_column(String(64), default="")


class Counter(Base):
    """Il contatore delle modifiche, uno per account: e' il cursore del pull."""

    __tablename__ = "counter"

    account_id: Mapped[int] = mapped_column(ForeignKey("account.id", ondelete="CASCADE"), primary_key=True)
    value: Mapped[int] = mapped_column(BigInteger, default=0)


engine = create_engine(
    settings.database_url,
    future=True,
    connect_args={"check_same_thread": False} if settings.database_url.startswith("sqlite") else {},
)
SessionLocal = sessionmaker(bind=engine, autoflush=False, expire_on_commit=False, future=True)


# Le colonne arrivate dopo il primo giorno di servizio. `create_all` crea le
# tabelle che mancano ma non tocca quelle che ci sono gia': queste si aggiungono
# a mano, una volta, e chi c'era prima resta dentro — non si chiude fuori
# qualcuno che usava il servizio ieri.
_ADDED_COLUMNS = (
    ("account", "approved", "BOOLEAN NOT NULL DEFAULT TRUE", "UPDATE account SET approved = TRUE"),
    ("account", "approved_at", "TIMESTAMP WITH TIME ZONE", None),
    ("account", "approval_token", "VARCHAR(64) NOT NULL DEFAULT ''", None),
    ("account", "signup_ip", "VARCHAR(64) NOT NULL DEFAULT ''", None),
)


def _migrate() -> None:
    from sqlalchemy import inspect, text

    inspector = inspect(engine)
    if "account" not in inspector.get_table_names():
        return
    sqlite = engine.dialect.name == "sqlite"
    for table, column, ddl, backfill in _ADDED_COLUMNS:
        existing = {c["name"] for c in inspector.get_columns(table)}
        if column in existing:
            continue
        if sqlite:
            # SQLite non conosce i tipi con fuso e vuole il DEFAULT costante.
            ddl = ddl.replace("TIMESTAMP WITH TIME ZONE", "TIMESTAMP").replace("TRUE", "1")
        with engine.begin() as connection:
            connection.execute(text(f"ALTER TABLE {table} ADD COLUMN {column} {ddl}"))
            if backfill:
                connection.execute(text(backfill.replace("TRUE", "1") if sqlite else backfill))


def create_all() -> None:
    Base.metadata.create_all(engine)
    _migrate()
