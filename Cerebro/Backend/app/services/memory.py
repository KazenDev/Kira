"""Kira memory repository with the existing JSON/JSONL formats preserved."""

from __future__ import annotations

from collections.abc import Callable
from datetime import datetime, timedelta
from pathlib import Path
from typing import Any
import uuid

from .storage import JsonStore


class MemoryStore:
    def __init__(
        self,
        base_dir: Callable[[], str],
        *,
        reflection_threshold: int = 150,
        expiry_days: int = 30,
    ) -> None:
        self.base_dir = base_dir
        self.reflection_threshold = reflection_threshold
        self.expiry_days = expiry_days
        self.storage = JsonStore()

    def state_path(self, pj_id: str) -> Path:
        return Path(self.base_dir()) / f"{pj_id}_estado.json"

    def memories_path(self, pj_id: str) -> Path:
        return Path(self.base_dir()) / f"{pj_id}.jsonl"

    def diary_path(self, pj_id: str) -> Path:
        return Path(self.base_dir()) / f"diario_{pj_id}.json"

    def load_state(self, pj_id: str) -> dict:
        return self.storage.read_json(
            self.state_path(pj_id),
            {"contador": self.reflection_threshold, "total": 0},
        )

    def save_state(self, pj_id: str, state: dict) -> None:
        self.storage.write_json(self.state_path(pj_id), state)

    def add_memory(
        self,
        pj_id: str,
        text: str,
        kind: str = "observacion",
        importance: int = 5,
        evidence: list[str] | None = None,
        metadata: dict | None = None,
        *,
        count: bool = True,
    ) -> dict:
        state_path = self.state_path(pj_id)
        with self.storage.lock_for(state_path):
            now = datetime.now()
            memory = {
                "id": uuid.uuid4().hex[:8],
                "fecha": now.isoformat(timespec="seconds"),
                "tipo": kind,
                "texto": text.strip(),
                "importancia": max(1, min(10, int(importance))),
                "expira": (now + timedelta(days=self.expiry_days)).isoformat(timespec="seconds"),
            }
            if evidence:
                memory["evidencia"] = evidence
            if metadata:
                for key in (
                    "source",
                    "status",
                    "supersedes",
                    "target_id",
                    "reason",
                    "tags",
                    "identity",
                    "name",
                    "memory_class",
                    "durable",
                    "field",
                    "memory_key",
                    "confidence",
                    "evidence",
                ):
                    if key in metadata:
                        memory[key] = metadata[key]
            self.storage.append_jsonl(self.memories_path(pj_id), memory)
            if not count:
                return memory
            state = self.storage.read_json(
                state_path,
                {"contador": self.reflection_threshold, "total": 0},
            )
            state["contador"] = state.get("contador", self.reflection_threshold) - memory["importancia"]
            state["total"] = state.get("total", 0) + 1
            if state["contador"] <= 0:
                state["reflexion_pendiente"] = True
                state["reflexiones"] = state.get("reflexiones", 0) + 1
                state["contador"] = self.reflection_threshold
            self.storage.write_json(state_path, state)
            return memory

    def claim_reflection(
        self,
        pj_id: str,
        minimum_memories: int,
    ) -> tuple[bool, list[dict]]:
        state_path = self.state_path(pj_id)
        with self.storage.lock_for(state_path):
            state = self.storage.read_json(
                state_path,
                {"contador": self.reflection_threshold, "total": 0},
            )
            if not state.get("reflexion_pendiente") or state.get("reflexionando"):
                return False, []
            memories = self.load_memories(pj_id)
            if len(memories) < minimum_memories:
                state["reflexion_pendiente"] = False
                state["reflexiones_omitidas"] = state.get("reflexiones_omitidas", 0) + 1
                self.storage.write_json(state_path, state)
                return False, []
            state["reflexionando"] = True
            self.storage.write_json(state_path, state)
            return True, memories

    def finish_reflection(self, pj_id: str) -> None:
        state = self.load_state(pj_id)
        state["reflexionando"] = False
        state["reflexion_pendiente"] = False
        state["reflexiones_hechas"] = state.get("reflexiones_hechas", 0) + 1
        self.save_state(pj_id, state)

    def find_by_text(self, pj_id: str, text: str) -> dict | None:
        normalized = " ".join(str(text).split()).strip().lower()
        for record in self.load_memories(pj_id):
            if " ".join(str(record.get("texto", "")).split()).strip().lower() == normalized:
                return record
        return None

    def load_memories(self, pj_id: str, include_expired: bool = False) -> list[dict]:
        now = datetime.now()
        raw_memories = [
            record
            for record in self.storage.read_jsonl(self.memories_path(pj_id))
            if isinstance(record, dict)
        ]
        superseded = {
            str(record.get("supersedes"))
            for record in raw_memories
            if record.get("supersedes")
        }
        forgotten = {
            str(record.get("target_id"))
            for record in raw_memories
            if record.get("target_id")
        }
        memories: list[dict] = []
        for record in raw_memories:
            record_id = str(record.get("id", ""))
            status = str(record.get("status", "active")).lower()
            if record_id in superseded or record_id in forgotten:
                continue
            if status in {"inactive", "deleted", "olvidado", "superseded"}:
                continue
            if str(record.get("tipo", "")).lower() in {"olvido", "tombstone"}:
                continue
            if not include_expired:
                try:
                    if datetime.fromisoformat(record["expira"]) < now:
                        continue
                except (KeyError, ValueError, TypeError):
                    pass
            memories.append(record)
        return memories

    def load_diary(self, pj_id: str) -> dict:
        return self.storage.read_json(
            self.diary_path(pj_id),
            {"yo_soy": "", "opiniones": [], "gustos": [], "personas": [], "actualizado": None},
        )

    def save_diary(self, pj_id: str, diary: dict) -> None:
        self.storage.write_json(self.diary_path(pj_id), diary)

    def snapshot_diary(self, pj_id: str) -> Path | None:
        current = self.load_diary(pj_id)
        if not current.get("actualizado"):
            return None
        folder = Path(self.base_dir()) / "diario_snapshots"
        folder.mkdir(parents=True, exist_ok=True)
        stamp = current["actualizado"].replace(":", "").replace("-", "")
        path = folder / f"{pj_id}_{stamp}.json"
        self.storage.write_json(path, current)
        return path
