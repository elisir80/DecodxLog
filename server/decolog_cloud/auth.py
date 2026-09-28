"""DecoDXLog Cloud — chi sei.

La password si tiene con Argon2, il token e' una stringa casuale di cui il
server conserva solo l'impronta SHA-256: se il database finisce in mano a
qualcuno, i token non si usano lo stesso.
"""

from __future__ import annotations

import datetime as dt
import hashlib
import secrets
from dataclasses import dataclass

from argon2 import PasswordHasher
from argon2.exceptions import VerifyMismatchError
from fastapi import Depends, Header, HTTPException, status
from sqlalchemy import select
from sqlalchemy.orm import Session

from .models import Account, Member, SessionLocal, Token
from .settings import settings

_hasher = PasswordHasher()


def hash_password(password: str) -> str:
    return _hasher.hash(password)


def verify_password(password_hash: str, password: str) -> bool:
    try:
        _hasher.verify(password_hash, password)
        return True
    except (VerifyMismatchError, Exception):  # noqa: BLE001 - un hash rotto non e' una password buona
        return False


def new_token() -> str:
    return secrets.token_urlsafe(32)


def token_fingerprint(token: str) -> str:
    return hashlib.sha256(token.encode("utf-8")).hexdigest()


def session() -> Session:
    db = SessionLocal()
    try:
        yield db
    finally:
        db.close()


def issue_token(db: Session, account: Account, device: str) -> str:
    raw = new_token()
    expires = dt.datetime.now(dt.UTC) + dt.timedelta(days=settings.token_days)
    db.add(
        Token(
            account_id=account.id,
            token_hash=token_fingerprint(raw),
            device=device[:120],
            expires_at=expires,
        )
    )
    db.commit()
    return raw


def current_account(
    authorization: str = Header(default=""),
    db: Session = Depends(session),
) -> Account:
    if not authorization.lower().startswith("bearer "):
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "manca il token")
    raw = authorization.split(" ", 1)[1].strip()
    token = db.scalar(select(Token).where(Token.token_hash == token_fingerprint(raw)))
    if token is None:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "token sconosciuto")
    if token.expires_at is not None:
        expires = token.expires_at
        if expires.tzinfo is None:
            expires = expires.replace(tzinfo=dt.UTC)
        if expires < dt.datetime.now(dt.UTC):
            raise HTTPException(status.HTTP_401_UNAUTHORIZED, "token scaduto")
    token.last_seen = dt.datetime.now(dt.UTC)
    db.commit()
    account = db.get(Account, token.account_id)
    if account is None:
        raise HTTPException(status.HTTP_401_UNAUTHORIZED, "account sparito")
    if settings.approval_required and not account.approved:
        # 403 e non 401: il token e' buono, e' l'account che aspetta. Il
        # programma deve dirlo cosi' com'e', non chiedere di nuovo la password.
        raise HTTPException(
            status.HTTP_403_FORBIDDEN,
            "registrazione in attesa di approvazione: il Cloud si apre quando l'operatore del servizio dice di si'",
        )
    return account


# ── Il log su cui si lavora ───────────────────────────────────────────────────


@dataclass
class LogAccess:
    """Di chi e' il log e chi ci sta lavorando.

    Di solito sono la stessa persona (`role` owner). In un log condiviso `log`
    e' l'account che lo tiene — il club — e `user` l'operatore entrato con un
    invito (vedi team.py).
    """

    log: Account
    user: Account
    role: str

    @property
    def shared(self) -> bool:
        return self.log.id != self.user.id

    @property
    def can_write(self) -> bool:
        return self.role in ("owner", "operator")


def resolve_log(db: Session, user: Account, wanted: str) -> LogAccess:
    callsign = (wanted or "").strip().upper()
    if not callsign or callsign == user.callsign:
        return LogAccess(user, user, "owner")
    owner = db.scalar(select(Account).where(Account.callsign == callsign))
    member = owner and db.scalar(select(Member).where(Member.log_id == owner.id, Member.member_id == user.id))
    # Stessa risposta per il log che non c'e' e per quello dove non si e'
    # invitati: da fuori non si capisce quali nominativi hanno un log.
    if member is None:
        raise HTTPException(status.HTTP_403_FORBIDDEN, f"non fai parte del log di {callsign}")
    if settings.approval_required and not owner.approved:
        raise HTTPException(status.HTTP_403_FORBIDDEN, f"il log di {callsign} aspetta ancora il via libera")
    return LogAccess(owner, user, member.role)


def current_log(
    x_decolog_log: str = Header(default=""),
    account: Account = Depends(current_account),
    db: Session = Depends(session),
) -> LogAccess:
    """Il log della richiesta: il proprio, o quello scritto in X-DecoLog-Log."""
    return resolve_log(db, account, x_decolog_log)
