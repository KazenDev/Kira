"""Deterministic contracts for account sessions and tenant data isolation."""

from __future__ import annotations

import json
from pathlib import Path

from app.intelligence.embeddings import HashEmbeddingProvider
from app.intelligence.rag_store import RagStore
from app.services.auth import AuthStore
from app.services.memory import MemoryStore
from app.services.tenancy import TenantManager, scope_context


def test_auth_store_uses_opaque_sessions_and_bound_csrf(tmp_path: Path) -> None:
    store = AuthStore(tmp_path / "auth.sqlite3")
    user = store.register("alice", "correcta123")
    raw_token, raw_csrf, session = store.create_session(user)

    assert len(raw_token) >= 32
    assert store.get_session(raw_token).user.id == user.id
    assert store.check_csrf(session, raw_csrf)
    assert not store.check_csrf(session, "otro-token")
    assert store.authenticate("ALICE", "correcta123").id == user.id


def test_tenant_scopes_isolate_kira_and_copy_legacy_once(tmp_path: Path) -> None:
    legacy = tmp_path / "legacy"
    legacy.mkdir()
    (legacy / "kira.jsonl").write_text(
        json.dumps(
            {
                "id": "legacy1",
                "fecha": "2026-01-01T00:00:00",
                "expira": "2099-01-01T00:00:00",
                "tipo": "observacion",
                "texto": "memoria legacy",
                "importancia": 8,
            }
        )
        + "\n",
        encoding="utf-8",
    )

    def make_memory(path: Path) -> MemoryStore:
        return MemoryStore(lambda: str(path))

    def make_rag(path: Path) -> RagStore:
        return RagStore(path, HashEmbeddingProvider())

    manager = TenantManager(
        users_root=tmp_path / "users",
        legacy_memory_dir=legacy,
        legacy_conversations_dir=tmp_path / "conversaciones",
        make_memory_store=make_memory,
        make_rag_store=make_rag,
        make_states=lambda: {},
    )
    first = manager.scope_for("a" * 32, "alice")
    second = manager.scope_for("b" * 32, "bob")

    with scope_context(first):
        first.memory_store.add_memory("kira", "de alice", "observacion", 7)
    with scope_context(second):
        second.memory_store.add_memory("kira", "de bob", "observacion", 8)

    with scope_context(first):
        assert [row["texto"] for row in first.memory_store.load_memories("kira")] == ["de alice"]
    with scope_context(second):
        assert [row["texto"] for row in second.memory_store.load_memories("kira")] == ["de bob"]

    assert manager.claim_legacy_for_first_user("c" * 32, "carla") is True
    assert manager.claim_legacy_for_first_user("d" * 32, "dave") is False
    with scope_context(manager.scope_for("c" * 32, "carla")):
        assert [row["texto"] for row in manager.scope_for("c" * 32, "carla").memory_store.load_memories("kira")] == ["memoria legacy"]
