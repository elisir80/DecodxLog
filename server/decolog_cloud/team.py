"""DecoDXLog Cloud — un log per piu' operatori.

Una stazione di club, un contest multi-operatore, una famiglia di radioamatori:
un log solo, piu' persone che ci scrivono, ognuna con il proprio account e la
propria password. Nessuno presta la password a nessuno.

Il log e' dell'account che lo tiene (il nominativo del club). Chi lo tiene crea
un invito — un codice da dire a voce o da mandare — e chi lo usa entra con il
proprio account. Da li' i suoi dispositivi possono sincronizzare quel log
dicendo quale vogliono (`X-DecoLog-Log: IQ8XX`, vedi auth.current_log):

* **operator** manda e prende i QSO del log condiviso;
* **viewer** li prende soltanto: un giudice di gara, un socio che guarda.

Viaggiano solo i QSO: profili, impostazioni e credenziali sigillate restano di
chi tiene il log, e nessuno che entra con un invito li vede o li cambia.

Del codice d'invito il server tiene solo l'impronta, come per i token: vale una
volta, e scade.
"""

from __future__ import annotations

import datetime as dt
import hashlib
import re
import secrets

from fastapi import APIRouter, Depends, HTTPException, status
from pydantic import BaseModel, Field
from sqlalchemy import select
from sqlalchemy.orm import Session

from . import auth
from .models import Account, Invite, Member

router = APIRouter(prefix="/v1/team")

ROLES = ("operator", "viewer")
# Niente lettere che si confondono dette a voce o lette da un foglio: 0/O, 1/I/L.
_ALPHABET = "ABCDEFGHJKMNPQRSTUVWXYZ23456789"


def _now() -> dt.datetime:
    return dt.datetime.now(dt.UTC)


def _aware(value: dt.datetime | None) -> dt.datetime | None:
    if value is not None and value.tzinfo is None:
        return value.replace(tzinfo=dt.UTC)
    return value


def normalize_code(code: str) -> str:
    """Il codice senza trattini, spazi e maiuscole: si scrive come viene."""
    return re.sub(r"[^A-Z0-9]", "", code.upper())


def code_fingerprint(code: str) -> str:
    return hashlib.sha256(normalize_code(code).encode("ascii")).hexdigest()


def new_code() -> str:
    raw = "".join(secrets.choice(_ALPHABET) for _ in range(12))
    return "-".join(raw[i:i + 4] for i in range(0, 12, 4))


def create_invite(db: Session, owner: Account, role: str, days: int) -> dict:
    if role not in ROLES:
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "il ruolo e' operator o viewer")
    code = new_code()
    expires = _now() + dt.timedelta(days=days)
    invite = Invite(log_id=owner.id, code_hash=code_fingerprint(code), role=role, expires_at=expires)
    db.add(invite)
    db.commit()
    return {"id": invite.id, "code": code, "role": role, "log": owner.callsign,
            "expiresAt": expires.isoformat(timespec="seconds")}


def team_of(db: Session, account: Account) -> dict:
    """Chi scrive nel mio log, in quali log scrivo io, gli inviti aperti."""
    members = []
    for row in db.scalars(select(Member).where(Member.log_id == account.id).order_by(Member.created_at)):
        who = db.get(Account, row.member_id)
        if who is not None:
            members.append({"callsign": who.callsign, "role": row.role,
                            "since": _aware(row.created_at).isoformat(timespec="seconds")})
    memberships = []
    for row in db.scalars(select(Member).where(Member.member_id == account.id).order_by(Member.created_at)):
        log = db.get(Account, row.log_id)
        if log is not None:
            memberships.append({"log": log.callsign, "role": row.role,
                                "since": _aware(row.created_at).isoformat(timespec="seconds")})
    invites = []
    for row in db.scalars(select(Invite).where(Invite.log_id == account.id, Invite.used_at.is_(None))
                          .order_by(Invite.created_at)):
        if _aware(row.expires_at) > _now():
            invites.append({"id": row.id, "role": row.role,
                            "createdAt": _aware(row.created_at).isoformat(timespec="seconds"),
                            "expiresAt": _aware(row.expires_at).isoformat(timespec="seconds")})
    return {"callsign": account.callsign, "members": members, "memberships": memberships, "invites": invites}


# ── API ───────────────────────────────────────────────────────────────────────


class InviteIn(BaseModel):
    role: str = Field(default="operator", max_length=16)
    days: int = Field(default=7, ge=1, le=30)


class JoinIn(BaseModel):
    code: str = Field(min_length=6, max_length=64)


@router.get("")
def team(account: Account = Depends(auth.current_account), db: Session = Depends(auth.session)) -> dict:
    return team_of(db, account)


@router.post("/invite")
def invite(body: InviteIn, account: Account = Depends(auth.current_account),
           db: Session = Depends(auth.session)) -> dict:
    """Un codice per entrare nel mio log. Si vede adesso e mai piu'."""
    return create_invite(db, account, body.role.strip().lower(), body.days)


@router.post("/join")
def join(body: JoinIn, account: Account = Depends(auth.current_account),
         db: Session = Depends(auth.session)) -> dict:
    invite = db.scalar(select(Invite).where(Invite.code_hash == code_fingerprint(body.code)))
    # Codice sconosciuto, gia' usato o scaduto: la stessa risposta, da fuori
    # non si deve poter distinguere.
    if invite is None or invite.used_at is not None or _aware(invite.expires_at) <= _now():
        raise HTTPException(status.HTTP_404_NOT_FOUND, "codice d'invito non valido o scaduto")
    if invite.log_id == account.id:
        raise HTTPException(status.HTTP_400_BAD_REQUEST, "e' un invito al tuo stesso log")
    member = db.scalar(select(Member).where(Member.log_id == invite.log_id, Member.member_id == account.id))
    if member is None:
        member = Member(log_id=invite.log_id, member_id=account.id)
        db.add(member)
    member.role = invite.role
    invite.used_at = _now()
    invite.used_by = account.id
    db.commit()
    owner = db.get(Account, invite.log_id)
    return {"log": owner.callsign if owner else "", "role": member.role}


@router.delete("/members/{callsign}")
def remove_member(callsign: str, account: Account = Depends(auth.current_account),
                  db: Session = Depends(auth.session)) -> dict:
    """Chi tiene il log toglie un operatore: i suoi QSO restano nel log."""
    who = db.scalar(select(Account).where(Account.callsign == callsign.strip().upper()))
    row = who and db.scalar(select(Member).where(Member.log_id == account.id, Member.member_id == who.id))
    if row is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"{callsign.upper()} non scrive nel tuo log")
    db.delete(row)
    db.commit()
    return {"ok": True}


@router.delete("/memberships/{callsign}")
def leave(callsign: str, account: Account = Depends(auth.current_account),
          db: Session = Depends(auth.session)) -> dict:
    """Si esce da un log condiviso: i QSO mandati restano a chi lo tiene."""
    log = db.scalar(select(Account).where(Account.callsign == callsign.strip().upper()))
    row = log and db.scalar(select(Member).where(Member.log_id == log.id, Member.member_id == account.id))
    if row is None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, f"non scrivi nel log di {callsign.upper()}")
    db.delete(row)
    db.commit()
    return {"ok": True}


@router.delete("/invites/{invite_id}")
def cancel_invite(invite_id: int, account: Account = Depends(auth.current_account),
                  db: Session = Depends(auth.session)) -> dict:
    row = db.get(Invite, invite_id)
    if row is None or row.log_id != account.id or row.used_at is not None:
        raise HTTPException(status.HTTP_404_NOT_FOUND, "invito sconosciuto")
    db.delete(row)
    db.commit()
    return {"ok": True}
