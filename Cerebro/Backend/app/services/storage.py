"""Small, thread-safe and atomic JSON/JSONL storage primitives."""

from __future__ import annotations

import json
import os
import threading
import uuid
from pathlib import Path
from typing import Any


class JsonStore:
    def __init__(self) -> None:
        self._locks: dict[str, threading.RLock] = {}
        self._locks_guard = threading.Lock()

    def lock_for(self, path: Path) -> threading.RLock:
        key = str(path.resolve())
        with self._locks_guard:
            return self._locks.setdefault(key, threading.RLock())

    def read_json(self, path: Path, default: Any) -> Any:
        with self.lock_for(path):
            try:
                with path.open(encoding="utf-8") as handle:
                    return json.load(handle)
            except (OSError, json.JSONDecodeError):
                return default

    def write_json(self, path: Path, value: Any) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with self.lock_for(path):
            temporary = path.with_name(f".{path.name}.{uuid.uuid4().hex}.tmp")
            try:
                with temporary.open("w", encoding="utf-8") as handle:
                    json.dump(value, handle, ensure_ascii=False, indent=1)
                    handle.flush()
                    os.fsync(handle.fileno())
                os.replace(temporary, path)
            finally:
                try:
                    temporary.unlink()
                except FileNotFoundError:
                    pass

    def append_jsonl(self, path: Path, value: Any) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with self.lock_for(path):
            with path.open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(value, ensure_ascii=False) + "\n")
                handle.flush()
                os.fsync(handle.fileno())

    def read_jsonl(self, path: Path) -> list[Any]:
        records: list[Any] = []
        with self.lock_for(path):
            try:
                with path.open(encoding="utf-8") as handle:
                    for line in handle:
                        line = line.strip()
                        if not line:
                            continue
                        try:
                            records.append(json.loads(line))
                        except json.JSONDecodeError:
                            continue
            except OSError:
                pass
        return records
