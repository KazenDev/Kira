"""Small server-side account and session store for Kira.

Passwords are never stored directly.  The implementation uses Python's
standard-library scrypt so the local build does not need another native
dependency.  Session cookies contain only opaque random tokens; the database
stores SHA-256 token hashes, and every authenticated mutation is protected by
a session-bound CSRF token.

The auth database is deliberately separate from every tenant's JSONL memory.
"""

from __future__ import annotations

import base64
import hashlib
import hmac
import re
import secrets
import sqlite3
import threading
import time
import uuid
from dataclasses import dataclass
from pathlib import Path
from typing import Any


# scrypt parameters are stored with every hash so they can be increased later
# without invalidating existing accounts.
_SCRYPT_N = 2**15
_SCRYPT_R = 8
_SCRYPT_P = 1
_SCRYPT_DKLEN = 32
_SCRYPT_MAXMEM = 128 * 1024 * 1024

_USERNAME_RE = re.compile(r"^[^\x00-\x1f]{3,32}$")


@dataclass(frozen=True, slots=True)
class AuthUser:
    id: str
    username: str
    created_at: str

    def public(self) -> dict[str, str]:
        return {"id": self.id, "username": self.username, "created_at": self.created_at}


@dataclass(frozen=True, slots=True)
class AuthSession:
    user: AuthUser
    csrf_hash: str
    expires_at: int


class AuthError(Exception):
    """Base class for expected account/session errors."""


class UsernameTaken(AuthError):
    pass


class InvalidCredentials(AuthError):
    pass


class InvalidSession(AuthError):
    pass


def _b64(value: bytes) -> str:
    return base64.urlsafe_b64encode(value).decode("ascii").rstrip("=")


def _unb64(value: str) -> bytes:
    return base64.urlsafe_b64decode(value + "=" * (-len(value) % 4))


def hash_password(password: str) -> str:
    """Hash a password with a unique salt and the stored scrypt parameters."""
    if not isinstance(password, str) or not password:
        raise ValueError("password vacío")
    salt = secrets.token_bytes(16)
    derived = hashlib.scrypt(
        password.encode("utf-8"),
        salt=salt,
        n=_SCRYPT_N,
        r=_SCRYPT_R,
        p=_SCRYPT_P,
        dklen=_SCRYPT_DKLEN,
        maxmem=_SCRYPT_MAXMEM,
    )
    return "$".join(
        (
            "scrypt",
            str(_SCRYPT_N),
            str(_SCRYPT_R),
            str(_SCRYPT_P),
            _b64(salt),
            _b64(derived),
        )
    )


def verify_password(password: str, encoded: str) -> bool:
    """Verify a password without leaking which parsing step failed."""
    try:
        parts = str(encoded).split("$")
        if len(parts) != 6 or parts[0] != "scrypt":
            return False
        _, n_text, r_text, p_text, salt_text, hash_text = parts
        salt = _unb64(salt_text)
        expected = _unb64(hash_text)
        actual = hashlib.scrypt(
            str(password).encode("utf-8"),
            salt=salt,
            n=int(n_text),
            r=int(r_text),
            p=int(p_text),
            dklen=len(expected),
            maxmem=_SCRYPT_MAXMEM,
        )
        return hmac.compare_digest(actual, expected)
    except (TypeError, ValueError, OverflowError):
        return False


def token_hash(token: str) -> str:
    return hashlib.sha256(str(token).encode("utf-8")).hexdigest()


def normalize_username(value: str) -> str:
    username = str(value or "").strip()
    if not _USERNAME_RE.fullmatch(username):
        raise ValueError("usuario inválido: usá entre 3 y 32 caracteres sin controles")
    if username.casefold() in {"admin", "root", "system"}:
        raise ValueError("ese nombre de usuario está reservado")
    return username


class AuthStore:
    """SQLite-backed users and opaque server-side sessions."""

    def __init__(self, path: str | Path, *, session_days: int = 30) -> None:
        self.path = Path(path)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.session_seconds = max(3600, int(session_days) * 86400)
        self._lock = threading.RLock()
        self._ensure_schema()

    def _connect(self) -> sqlite3.Connection:
        connection = sqlite3.connect(self.path, timeout=30, check_same_thread=False)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA busy_timeout=30000")
        connection.execute("PRAGMA journal_mode=WAL")
        connection.execute("PRAGMA synchronous=NORMAL")
        return connection

    def _ensure_schema(self) -> None:
        with self._lock, self._connect() as db:
            db.executescript(
                """
                CREATE TABLE IF NOT EXISTS users (
                    id TEXT PRIMARY KEY,
                    username TEXT NOT NULL,
                    username_key TEXT NOT NULL UNIQUE,
                    password_hash TEXT NOT NULL,
                    created_at TEXT NOT NULL
                );
                CREATE TABLE IF NOT EXISTS sessions (
                    token_hash TEXT PRIMARY KEY,
                    user_id TEXT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
                    csrf_hash TEXT NOT NULL,
                    created_at INTEGER NOT NULL,
                    last_used_at INTEGER NOT NULL,
                    expires_at INTEGER NOT NULL
                );
                CREATE INDEX IF NOT EXISTS sessions_user_id ON sessions(user_id);
                CREATE INDEX IF NOT EXISTS sessions_expiry ON sessions(expires_at);
                """
            )
        try:
            self.path.chmod(0o600)
        except OSError:
            pass

    @staticmethod
    def _row_to_user(row: sqlite3.Row) -> AuthUser:
        return AuthUser(
            id=str(row["id"]),
            username=str(row["username"]),
            created_at=str(row["created_at"]),
        )

    def count_users(self) -> int:
        with self._lock, self._connect() as db:
            row = db.execute("SELECT COUNT(*) AS n FROM users").fetchone()
        return int(row["n"] if row else 0)

    def register(self, username: str, password: str) -> AuthUser:
        display = normalize_username(username)
        key = display.casefold()
        password_hash = hash_password(password)
        user = AuthUser(
            id=uuid.uuid4().hex,
            username=display,
            created_at=time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        )
        try:
            with self._lock, self._connect() as db:
                db.execute(
                    "INSERT INTO users(id, username, username_key, password_hash, created_at) "
                    "VALUES(?, ?, ?, ?, ?)",
                    (user.id, user.username, key, password_hash, user.created_at),
                )
        except sqlite3.IntegrityError as exc:
            raise UsernameTaken("ese usuario ya existe") from exc
        return user

    def authenticate(self, username: str, password: str) -> AuthUser:
        key = str(username or "").strip().casefold()
        with self._lock, self._connect() as db:
            row = db.execute(
                "SELECT * FROM users WHERE username_key=?", (key,)
            ).fetchone()
        if row is None:
            # Do a real verification for unknown users too, reducing account
            # enumeration through a cheap timing difference.
            hash_password(str(password or ""))
            raise InvalidCredentials("usuario o contraseña incorrectos")
        if not verify_password(str(password or ""), str(row["password_hash"])):
            raise InvalidCredentials("usuario o contraseña incorrectos")
        return self._row_to_user(row)

    def create_session(self, user: AuthUser) -> tuple[str, str, AuthSession]:
        raw_token = secrets.token_urlsafe(32)
        raw_csrf = secrets.token_urlsafe(32)
        now = int(time.time())
        expires = now + self.session_seconds
        with self._lock, self._connect() as db:
            db.execute(
                "INSERT INTO sessions(token_hash, user_id, csrf_hash, created_at, "
                "last_used_at, expires_at) VALUES(?, ?, ?, ?, ?, ?)",
                (token_hash(raw_token), user.id, token_hash(raw_csrf), now, now, expires),
            )
        return raw_token, raw_csrf, AuthSession(user, token_hash(raw_csrf), expires)

    def get_session(self, raw_token: str | None) -> AuthSession:
        if not raw_token:
            raise InvalidSession("sesión inválida")
        now = int(time.time())
        digest = token_hash(raw_token)
        with self._lock, self._connect() as db:
            row = db.execute(
                "SELECT s.token_hash, s.csrf_hash, s.expires_at, "
                "u.id, u.username, u.created_at "
                "FROM sessions s JOIN users u ON u.id=s.user_id "
                "WHERE s.token_hash=?",
                (digest,),
            ).fetchone()
            if row is None or int(row["expires_at"]) <= now:
                if row is not None:
                    db.execute("DELETE FROM sessions WHERE token_hash=?", (digest,))
                raise InvalidSession("sesión inválida o expirada")
            db.execute(
                "UPDATE sessions SET last_used_at=? WHERE token_hash=?",
                (now, digest),
            )
        return AuthSession(
            user=AuthUser(str(row["id"]), str(row["username"]), str(row["created_at"])),
            csrf_hash=str(row["csrf_hash"]),
            expires_at=int(row["expires_at"]),
        )

    def check_csrf(self, session: AuthSession, raw_csrf: str | None) -> bool:
        return bool(raw_csrf) and hmac.compare_digest(token_hash(raw_csrf or ""), session.csrf_hash)

    def delete_session(self, raw_token: str | None) -> None:
        if not raw_token:
            return
        with self._lock, self._connect() as db:
            db.execute("DELETE FROM sessions WHERE token_hash=?", (token_hash(raw_token),))

    def delete_user_sessions(self, user_id: str) -> None:
        with self._lock, self._connect() as db:
            db.execute("DELETE FROM sessions WHERE user_id=?", (user_id,))

    def cleanup_expired(self) -> int:
        now = int(time.time())
        with self._lock, self._connect() as db:
            cursor = db.execute("DELETE FROM sessions WHERE expires_at<=?", (now,))
            return int(cursor.rowcount or 0)


__all__ = [
    "AuthError",
    "AuthSession",
    "AuthStore",
    "AuthUser",
    "InvalidCredentials",
    "InvalidSession",
    "UsernameTaken",
    "hash_password",
    "normalize_username",
    "token_hash",
    "verify_password",
]
