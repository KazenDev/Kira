"""Per-user data scopes for Kira.

The legacy runtime stores memories as JSONL and keeps a rebuildable RAG index.
This module gives each authenticated account its own directory and store
objects while retaining the old global objects as a compatibility fallback for
migrations and tests.

Only personal data is scoped: memories, diary, conversations, emotional state
and the derived RAG index.  Serial/BLE, provider clients and the TTS service
remain process-wide because they represent the one physical Kira device.
"""

from __future__ import annotations

import shutil
import threading
from contextlib import contextmanager
from contextvars import ContextVar, Token
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, Iterator

from .memory import MemoryStore


@dataclass(slots=True)
class TenantScope:
    user_id: str
    root: Path
    memory_dir: Path
    conversations_dir: Path
    recordings_dir: Path
    rag_path: Path
    memory_store: MemoryStore
    rag_store: Any
    states: dict[str, Any]
    username: str = ""


_current_scope: ContextVar[TenantScope | None] = ContextVar(
    "kira_tenant_scope", default=None
)


def current_scope() -> TenantScope | None:
    return _current_scope.get()


def set_current_scope(scope: TenantScope | None) -> Token:
    return _current_scope.set(scope)


def reset_current_scope(token: Token) -> None:
    _current_scope.reset(token)


@contextmanager
def scope_context(scope: TenantScope | None) -> Iterator[None]:
    token = set_current_scope(scope)
    try:
        yield
    finally:
        reset_current_scope(token)


class StoreProxy:
    """Dispatch store method calls to the current tenant, or the legacy store."""

    def __init__(self, legacy: Any, attribute: str) -> None:
        self._legacy = legacy
        self._attribute = attribute

    def _target(self) -> Any:
        scope = current_scope()
        return getattr(scope, self._attribute) if scope is not None else self._legacy

    def __getattr__(self, name: str) -> Any:
        return getattr(self._target(), name)

    def __repr__(self) -> str:
        return f"<StoreProxy {self._attribute}>"


class StatesProxy:
    """A tiny Mapping facade for the per-user emotional-state dictionaries."""

    def __init__(self, legacy: dict[str, Any]) -> None:
        self._legacy = legacy

    def _target(self) -> dict[str, Any]:
        scope = current_scope()
        return scope.states if scope is not None else self._legacy

    def __getitem__(self, key: str) -> Any:
        return self._target()[key]

    def __setitem__(self, key: str, value: Any) -> None:
        self._target()[key] = value

    def __contains__(self, key: object) -> bool:
        return key in self._target()

    def __iter__(self) -> Iterator[str]:
        return iter(self._target())

    def __len__(self) -> int:
        return len(self._target())

    def get(self, key: str, default: Any = None) -> Any:
        return self._target().get(key, default)

    def items(self) -> Any:
        return self._target().items()

    def keys(self) -> Any:
        return self._target().keys()

    def values(self) -> Any:
        return self._target().values()


class TenantManager:
    """Create and cache isolated data stores for authenticated user IDs."""

    def __init__(
        self,
        *,
        users_root: str | Path,
        legacy_memory_dir: str | Path,
        legacy_conversations_dir: str | Path,
        make_rag_store: Callable[[Path], Any],
        make_states: Callable[[], dict[str, Any]],
        make_memory_store: Callable[[Path], MemoryStore],
    ) -> None:
        self.users_root = Path(users_root)
        self.legacy_memory_dir = Path(legacy_memory_dir)
        self.legacy_conversations_dir = Path(legacy_conversations_dir)
        self.make_rag_store = make_rag_store
        self.make_states = make_states
        self.make_memory_store = make_memory_store
        self._scopes: dict[str, TenantScope] = {}
        self._lock = threading.RLock()
        self._legacy_claim_lock = threading.Lock()

    def user_root(self, user_id: str) -> Path:
        # IDs are generated hex UUIDs, never usernames.  Still reject anything
        # unexpected rather than allowing a path traversal if data is corrupt.
        safe = "".join(ch for ch in str(user_id) if ch.isalnum())
        if not safe or safe != str(user_id):
            raise ValueError("user_id inválido")
        return self.users_root / safe

    def scope_for(self, user_id: str, username: str = "") -> TenantScope:
        with self._lock:
            existing = self._scopes.get(str(user_id))
            if existing is not None:
                if username:
                    existing.username = username
                return existing
            root = self.user_root(user_id)
            memory_dir = root / "memoria"
            conversations_dir = root / "conversaciones"
            recordings_dir = root / "grabaciones"
            rag_path = memory_dir / "rag" / "index.sqlite3"
            memory_dir.mkdir(parents=True, exist_ok=True)
            conversations_dir.mkdir(parents=True, exist_ok=True)
            recordings_dir.mkdir(parents=True, exist_ok=True)
            scope = TenantScope(
                user_id=str(user_id),
                root=root,
                memory_dir=memory_dir,
                conversations_dir=conversations_dir,
                recordings_dir=recordings_dir,
                rag_path=rag_path,
                memory_store=self.make_memory_store(memory_dir),
                rag_store=self.make_rag_store(rag_path),
                states=self.make_states(),
                username=username,
            )
            self._scopes[str(user_id)] = scope
            return scope

    def claim_legacy_for_first_user(self, user_id: str, username: str = "") -> bool:
        """Copy the pre-auth Kira snapshot into the first account.

        The source is never moved or deleted.  A marker prevents a second
        registration from copying it again.  The RAG SQLite file is rebuilt
        from the JSONL copy instead of copied byte-for-byte.
        """
        with self._legacy_claim_lock:
            marker = self.users_root / ".legacy-claimed"
            if marker.exists():
                return False
            root = self.user_root(user_id)
            memory_dir = root / "memoria"
            conversations_dir = root / "conversaciones"
            memory_dir.mkdir(parents=True, exist_ok=True)
            conversations_dir.mkdir(parents=True, exist_ok=True)

            # Only Kira's legacy files are imported; unrelated test/demo data
            # must not leak into a real account.
            for name in ("kira.jsonl", "kira_estado.json", "diario_kira.json"):
                source = self.legacy_memory_dir / name
                target = memory_dir / name
                if source.exists() and not target.exists():
                    shutil.copy2(source, target)
            legacy_snapshots = self.legacy_memory_dir / "diario_snapshots"
            target_snapshots = memory_dir / "diario_snapshots"
            if legacy_snapshots.is_dir() and not target_snapshots.exists():
                shutil.copytree(legacy_snapshots, target_snapshots)
            legacy_conversation = self.legacy_conversations_dir / "kira.jsonl"
            target_conversation = conversations_dir / "kira.jsonl"
            if legacy_conversation.exists() and not target_conversation.exists():
                shutil.copy2(legacy_conversation, target_conversation)

            # Create the scope after copying so its RAG store is initialized from
            # the new JSONL.  FTS is immediate; embeddings backfill locally on
            # first use just like a normal new account.
            scope = self.scope_for(user_id, username)
            records = scope.memory_store.load_memories("kira", include_expired=True)
            try:
                scope.rag_store.rebuild("kira", records, embed=False)
            except Exception:
                # The JSONL remains authoritative; chat can rebuild FTS later.
                pass
            marker.write_text(str(user_id), encoding="utf-8")
            try:
                marker.chmod(0o600)
            except OSError:
                pass
            return True


__all__ = [
    "StatesProxy",
    "StoreProxy",
    "TenantManager",
    "TenantScope",
    "current_scope",
    "reset_current_scope",
    "scope_context",
    "set_current_scope",
]
