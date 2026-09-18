"""DecoLog Cloud — chi sei.

La password si tiene con Argon2, il token e' una stringa casuale di cui il
server conserva solo l'impronta SHA-256: se il database finisce in mano a
qualcuno, i token non si usano lo stesso.
"""

from __future__ import annotations

import datetime as dt
import hashlib
import secrets

from argon2 import PasswordHasher
from argon2.exceptions import VerifyMismatchError
from fastapi import Depends, Header, HTTPException, status
from sqlalchemy import select
from sqlalchemy.orm import Session

from .models import Account, SessionLocal, Token
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
    return account
