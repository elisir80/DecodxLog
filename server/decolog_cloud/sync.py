"""DecoDXLog Cloud — il sync vero e proprio.

Le regole sono quelle scritte in Fase 0:

* ogni modifica locale alza la revisione e mette il QSO in coda (`dirty`);
* chi spinge dice la revisione che conosceva: se il server ne ha una piu' alta
  c'e' un conflitto — **vince l'ultima modifica**, ma la versione che perde
  finisce nello storico, non nel cestino;
* il pull e' a cursore: `since` e' il numero dell'ultima modifica gia' vista;
* l'uuid lo fa il client, quindi rimandare due volte la stessa cosa non crea
  doppioni;
* due QSO diversi ma uguali nei fatti (stesso nominativo, banda, gruppo di modi
  e orario vicino) sono lo stesso QSO: il server lo dice al client, che decide.
"""

from __future__ import annotations

import datetime as dt

from sqlalchemy import select
from sqlalchemy.orm import Session

from .models import Account, Counter, Doc, DocHistory, Qso, QsoHistory

# Quanto possono distare due QSO per essere lo stesso: due minuti, dieci se
# almeno uno dei due e' stato scritto a mano.
DEDUP_SECONDS = 120
DEDUP_SECONDS_MANUAL = 600

_MODE_GROUPS = {
    "CW": "CW",
    "SSB": "PHONE", "USB": "PHONE", "LSB": "PHONE", "AM": "PHONE", "FM": "PHONE",
    "RTTY": "DATA", "PSK": "DATA", "MFSK": "DATA", "FT8": "DATA", "FT4": "DATA",
    "FT2": "DATA", "JT65": "DATA", "JT9": "DATA", "OLIVIA": "DATA", "DIGI": "DATA",
}


def mode_group(mode: str, submode: str = "") -> str:
    """Il gruppo di modi come lo intende LoTW: CW, fonia, dati."""
    for value in (submode or "", mode or ""):
        group = _MODE_GROUPS.get(value.strip().upper())
        if group:
            return group
    return (mode or "").strip().upper() or "DATA"


def parse_time(value: str | None) -> dt.datetime | None:
    if not value:
        return None
    text = value.strip().replace("Z", "+00:00")
    try:
        parsed = dt.datetime.fromisoformat(text)
    except ValueError:
        return None
    return parsed if parsed.tzinfo else parsed.replace(tzinfo=dt.UTC)


def _next_seq(db: Session, account: Account) -> int:
    counter = db.get(Counter, account.id)
    if counter is None:
        counter = Counter(account_id=account.id, value=0)
        db.add(counter)
    counter.value += 1
    return counter.value


def find_duplicate(db: Session, account: Account, record: dict, manual: bool = False) -> Qso | None:
    """Un QSO gia' sul server che sia lo stesso di questo, pur con un altro uuid."""
    call = (record.get("call") or "").strip().upper()
    band = (record.get("band") or "").strip().lower()
    started = parse_time(record.get("startedAt"))
    if not call or not band or started is None:
        return None
    window = dt.timedelta(seconds=DEDUP_SECONDS_MANUAL if manual else DEDUP_SECONDS)
    group = mode_group(record.get("mode", ""), record.get("submode", ""))
    rows = db.scalars(
        select(Qso).where(
            Qso.account_id == account.id,
            Qso.call == call,
            Qso.band == band,
            Qso.mode_group == group,
            Qso.deleted.is_(False),
        )
    ).all()
    for row in rows:
        when = row.started_at
        if when is None:
            continue
        if when.tzinfo is None:
            when = when.replace(tzinfo=dt.UTC)
        if abs((when - started).total_seconds()) <= window.total_seconds():
            return row
    return None


def apply_push(db: Session, account: Account, record: dict, device: str) -> dict:
    """Scrive un QSO che arriva dal client e racconta com'e' andata.

    Torna {uuid, status, revision, seq}: `status` e' "applied", "conflict"
    (il server aveva una revisione piu' alta e ha vinto il client, la vecchia e'
    nello storico), "duplicate" (esisteva gia' con un altro uuid) o "stale"
    (il client ha mandato roba vecchia e il server la ignora).
    """
    uuid = (record.get("uuid") or "").strip()
    if not uuid:
        return {"uuid": "", "status": "rejected", "reason": "uuid mancante"}

    revision = int(record.get("revision") or 1)
    fields = record.get("fields") or {}
    deleted = bool(record.get("deleted"))
    started = parse_time(record.get("startedAt"))
    call = (record.get("call") or "").strip().upper()
    band = (record.get("band") or "").strip().lower()
    group = mode_group(record.get("mode", ""), record.get("submode", ""))

    row = db.scalar(select(Qso).where(Qso.account_id == account.id, Qso.uuid == uuid))
    status = "applied"

    if row is None:
        twin = find_duplicate(db, account, record, manual=bool(record.get("manual")))
        if twin is not None:
            # Lo stesso collegamento con un altro uuid: il server tiene il suo e
            # lo dice, cosi' il client puo' allinearsi invece di litigare.
            return {
                "uuid": uuid,
                "status": "duplicate",
                "serverUuid": twin.uuid,
                "revision": twin.revision,
                "seq": twin.seq,
            }
        row = Qso(account_id=account.id, uuid=uuid)
        db.add(row)
    else:
        if revision < row.revision:
            # Il client e' indietro: vince quello che c'e' gia'.
            return {"uuid": uuid, "status": "stale", "revision": row.revision, "seq": row.seq}
        if revision == row.revision and row.fields != fields:
            # Stessa revisione, contenuto diverso: due dispositivi hanno scritto
            # senza sapere l'uno dell'altro. Vince chi arriva adesso, ma la
            # versione di prima resta nello storico.
            db.add(
                QsoHistory(
                    account_id=account.id,
                    uuid=uuid,
                    revision=row.revision,
                    fields=row.fields,
                    reason="conflict_lost",
                )
            )
            status = "conflict"
            revision = row.revision + 1

    row.revision = revision
    row.deleted = deleted
    row.fields = fields
    row.call = call
    row.band = band
    row.mode_group = group
    row.started_at = started
    row.device = device[:120]
    row.updated_at = dt.datetime.now(dt.UTC)
    row.seq = _next_seq(db, account)
    db.flush()
    return {"uuid": uuid, "status": status, "revision": row.revision, "seq": row.seq}


def apply_doc(db: Session, account: Account, record: dict, device: str) -> dict:
    """Scrive un documento (profilo, impostazione...) con le stesse regole dei QSO."""
    kind = (record.get("kind") or "").strip()
    key = (record.get("key") or "").strip()
    if not kind or not key:
        return {"kind": kind, "key": key, "status": "rejected", "reason": "kind o key mancante"}

    revision = int(record.get("revision") or 1)
    data = record.get("data") or {}
    deleted = bool(record.get("deleted"))

    row = db.scalar(
        select(Doc).where(Doc.account_id == account.id, Doc.kind == kind, Doc.key == key)
    )
    status = "applied"

    if row is None:
        row = Doc(account_id=account.id, kind=kind, key=key)
        db.add(row)
    else:
        if revision < row.revision:
            return {"kind": kind, "key": key, "status": "stale", "revision": row.revision, "seq": row.seq}
        if revision == row.revision and row.data != data:
            db.add(
                DocHistory(account_id=account.id, kind=kind, key=key, revision=row.revision, data=row.data)
            )
            status = "conflict"
            revision = row.revision + 1

    row.revision = revision
    row.deleted = deleted
    row.data = data
    row.device = device[:120]
    row.updated_at = dt.datetime.now(dt.UTC)
    row.seq = _next_seq(db, account)
    db.flush()
    return {"kind": kind, "key": key, "status": status, "revision": row.revision, "seq": row.seq}


def pull_docs(db: Session, account: Account, since: int, limit: int) -> list[dict]:
    """I documenti cambiati dopo `since`, nello stesso ordine dei QSO."""
    rows = db.scalars(
        select(Doc)
        .where(Doc.account_id == account.id, Doc.seq > since)
        .order_by(Doc.seq)
        .limit(limit)
    ).all()
    return [
        {
            "kind": row.kind,
            "key": row.key,
            "revision": row.revision,
            "seq": row.seq,
            "deleted": row.deleted,
            # Da dove arriva: un altro computer della stazione, o il browser.
            "device": row.device,
            "updatedAt": row.updated_at.isoformat() if row.updated_at else None,
            "data": row.data,
        }
        for row in rows
    ]


def pull(db: Session, account: Account, since: int, limit: int) -> tuple[list[dict], int, bool]:
    """I QSO cambiati dopo `since`, in ordine di modifica."""
    rows = db.scalars(
        select(Qso)
        .where(Qso.account_id == account.id, Qso.seq > since)
        .order_by(Qso.seq)
        .limit(limit + 1)
    ).all()
    more = len(rows) > limit
    rows = rows[:limit]
    records = [
        {
            "uuid": row.uuid,
            "revision": row.revision,
            "seq": row.seq,
            "deleted": row.deleted,
            "updatedAt": row.updated_at.isoformat() if row.updated_at else None,
            "fields": row.fields,
        }
        for row in rows
    ]
    cursor = rows[-1].seq if rows else since
    return records, cursor, more
