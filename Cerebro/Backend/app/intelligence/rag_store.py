"""Local hybrid RAG index for Kira memories.

The JSONL memory files remain the source of truth. SQLite stores a rebuildable
FTS5/vector index with provenance and embedding metadata; losing this file is
safe because ``rebuild`` can recreate it.
"""

from __future__ import annotations

import hashlib
import re
import sqlite3
import threading
from collections.abc import Callable, Iterable
from datetime import datetime
from pathlib import Path
from typing import Any

import numpy as np

from .embeddings import EmbeddingProvider, NullEmbeddingProvider

SCHEMA_VERSION = 1
_FTS_STOP_WORDS = {
    "que", "qué", "de", "del", "la", "las", "el", "los", "un", "una",
    "unos", "unas", "y", "o", "a", "al", "en", "con", "por", "para", "es",
    "son", "fue", "ser", "estar", "que", "como", "qué", "mi", "mis", "tu",
    "tus", "su", "sus", "lo", "me", "te", "se", "si", "sí", "no", "mas",
    "más", "muy", "sobre", "hay", "tengo", "tienes",
}


class RagStore:
    def __init__(
        self,
        path: str | Path | Callable[[], str | Path],
        embedder: EmbeddingProvider | None = None,
        *,
        rrf_k: int = 60,
        vector_only_min_similarity: float = 0.84,
    ) -> None:
        self.path = path
        self.embedder = embedder or NullEmbeddingProvider()
        self.rrf_k = max(1, int(rrf_k))
        self.vector_only_min_similarity = float(vector_only_min_similarity)
        self._lock = threading.RLock()
        self._fts_available = True
        self._schema_ready = False

    def _db_path(self) -> Path:
        value = self.path() if callable(self.path) else self.path
        return Path(value)

    def _connect(self) -> sqlite3.Connection:
        path = self._db_path()
        path.parent.mkdir(parents=True, exist_ok=True)
        connection = sqlite3.connect(path, timeout=30)
        connection.row_factory = sqlite3.Row
        connection.execute("PRAGMA busy_timeout = 30000")
        connection.execute("PRAGMA journal_mode = WAL")
        connection.execute("PRAGMA synchronous = NORMAL")
        return connection

    def ensure_schema(self) -> None:
        with self._lock:
            if self._schema_ready:
                return
            try:
                with self._connect() as db:
                    db.executescript(
                        """
                        CREATE TABLE IF NOT EXISTS rag_meta (
                            key TEXT PRIMARY KEY,
                            value TEXT NOT NULL
                        );
                        CREATE TABLE IF NOT EXISTS rag_memories (
                            character TEXT NOT NULL,
                            memory_id TEXT NOT NULL,
                            text TEXT NOT NULL,
                            kind TEXT NOT NULL DEFAULT 'observacion',
                            importance REAL NOT NULL DEFAULT 5,
                            created_at TEXT NOT NULL DEFAULT '',
                            expires_at TEXT NOT NULL DEFAULT '',
                            active INTEGER NOT NULL DEFAULT 1,
                            supersedes TEXT,
                            source TEXT NOT NULL DEFAULT '',
                            content_hash TEXT NOT NULL,
                            embedding_model TEXT NOT NULL DEFAULT '',
                            embedding_version INTEGER NOT NULL DEFAULT 0,
                            embedding BLOB,
                            updated_at TEXT NOT NULL,
                            PRIMARY KEY (character, memory_id)
                        );
                        CREATE INDEX IF NOT EXISTS rag_memories_character
                            ON rag_memories(character, active);
                        CREATE INDEX IF NOT EXISTS rag_memories_hash
                            ON rag_memories(character, content_hash);
                        """
                    )
                    try:
                        db.execute(
                            "CREATE VIRTUAL TABLE IF NOT EXISTS rag_fts USING fts5("
                            "memory_key UNINDEXED, character UNINDEXED, text, "
                            "tokenize='unicode61 remove_diacritics 1')"
                        )
                    except sqlite3.OperationalError:
                        self._fts_available = False
                    db.execute(
                        "INSERT OR REPLACE INTO rag_meta(key, value) VALUES(?, ?)",
                        ("schema_version", str(SCHEMA_VERSION)),
                    )
            except sqlite3.DatabaseError as error:
                path = self._db_path()
                corrupt = path.with_name(
                    f"{path.name}.corrupt-{datetime.now().strftime('%Y%m%d%H%M%S')}"
                )
                try:
                    path.replace(corrupt)
                    print(f"[RAG] índice corrupto aislado en {corrupt.name}: {error}")
                except OSError as move_error:
                    print(f"[RAG] no se pudo aislar índice corrupto: {move_error}")
                    raise
                self._schema_ready = False
                return self.ensure_schema()
            self._schema_ready = True

    @staticmethod
    def _key(character: str, memory_id: str) -> str:
        return f"{character}:{memory_id}"

    @staticmethod
    def _hash(text: str) -> str:
        normalized = " ".join(str(text).split()).strip().lower()
        return hashlib.sha256(normalized.encode("utf-8")).hexdigest()

    @staticmethod
    def _now() -> str:
        return datetime.now().isoformat(timespec="seconds")

    def _embedding_bytes(
        self,
        text: str,
        old_model: str,
        old_version: int,
        old_embedding: bytes | None,
    ) -> bytes | None:
        model_name = self.embedder.fingerprint()
        if old_embedding is not None and old_model == model_name and old_version == self.embedder.version:
            return old_embedding
        if not self.embedder.available():
            return old_embedding
        vectors = self.embedder.embed_documents([text])
        if vectors is None or len(vectors) != 1:
            return old_embedding
        vector = np.asarray(vectors[0], dtype=np.float32)
        return vector.tobytes()

    def upsert_memory(
        self,
        character: str,
        record: dict,
        *,
        embed: bool = True,
    ) -> None:
        self.ensure_schema()
        text = str(record.get("texto", "")).strip()
        if not text:
            return
        memory_id = str(record.get("id") or "legacy-" + self._hash(text)[:16])
        key = self._key(character, memory_id)
        content_hash = self._hash(text)
        status = str(record.get("status", "active")).lower()
        active = 0 if status in {"inactive", "deleted", "olvidado", "superseded"} else 1
        if str(record.get("tipo", "")).lower() in {"olvido", "tombstone"}:
            active = 0
        created_at = str(record.get("fecha", ""))
        expires_at = str(record.get("expira", ""))
        importance = float(record.get("importancia", 5) or 5)
        source = str(record.get("source", ""))
        supersedes = record.get("supersedes")
        with self._lock:
            with self._connect() as db:
                old = db.execute(
                    "SELECT embedding_model, embedding_version, embedding "
                    "FROM rag_memories WHERE character=? AND memory_id=?",
                    (character, memory_id),
                ).fetchone()
                old_model = old["embedding_model"] if old else ""
                old_version = int(old["embedding_version"] or 0) if old else 0
                old_embedding = old["embedding"] if old else None
                model_name = self.embedder.fingerprint() if embed else old_model
                embedding = self._embedding_bytes(
                    text,
                    old_model,
                    old_version,
                    old_embedding,
                ) if embed else old_embedding
                if self._fts_available:
                    db.execute(
                        "DELETE FROM rag_fts WHERE memory_key=?",
                        (key,),
                    )
                db.execute(
                    """
                    INSERT OR REPLACE INTO rag_memories(
                        character, memory_id, text, kind, importance,
                        created_at, expires_at, active, supersedes, source,
                        content_hash, embedding_model, embedding_version,
                        embedding, updated_at
                    ) VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)
                    """,
                    (
                        character,
                        memory_id,
                        text,
                        str(record.get("tipo", "observacion")),
                        importance,
                        created_at,
                        expires_at,
                        active,
                        str(supersedes) if supersedes else None,
                        source,
                        content_hash,
                        model_name,
                        self.embedder.version if embed else old_version,
                        sqlite3.Binary(embedding) if embedding else None,
                        self._now(),
                    ),
                )
                if self._fts_available and active:
                    db.execute(
                        "INSERT INTO rag_fts(memory_key, character, text) VALUES(?,?,?)",
                        (key, character, text),
                    )

    def sync_memories(
        self,
        character: str,
        records: Iterable[dict],
        *,
        embed: bool = False,
    ) -> int:
        count = 0
        for record in records:
            try:
                self.upsert_memory(character, record, embed=embed)
                count += 1
            except Exception as error:
                print(f"[RAG] no se pudo indexar {character}: {type(error).__name__}: {error}")
        return count

    def near_duplicates(
        self,
        character: str,
        text: str,
        candidates: list[dict],
        *,
        threshold: float = 0.97,
        limit: int = 3,
    ) -> list[dict]:
        """Find very close semantic duplicates without suppressing mere topics."""
        self.ensure_schema()
        if not text.strip() or not candidates or not self.embedder.available():
            return []
        vector = self.embedder.embed_documents([text])
        if vector is None or len(vector) != 1:
            return []
        query_vector = np.asarray(vector[0], dtype=np.float32)
        allowed = {
            self._key(character, str(record.get("id", "")))
            for record in candidates
            if record.get("id")
        }
        with self._lock, self._connect() as db:
            rows = db.execute(
                "SELECT memory_id, embedding FROM rag_memories "
                "WHERE character=? AND active=1",
                (character,),
            ).fetchall()
            matches = []
            for row in rows:
                key = self._key(character, row["memory_id"])
                if key not in allowed or not row["embedding"]:
                    continue
                score = self._cosine(row["embedding"], query_vector)
                if score >= threshold:
                    matches.append((key, score))
        matches.sort(key=lambda item: item[1], reverse=True)
        result = []
        for key, score in matches[: max(1, limit)]:
            record = next(
                (
                    dict(item)
                    for item in candidates
                    if self._key(character, str(item.get("id", ""))) == key
                ),
                {"id": key.split(":", 1)[-1]},
            )
            record["_similarity"] = round(float(score), 6)
            result.append(record)
        return result

    def deactivate(self, character: str, memory_id: str) -> bool:
        self.ensure_schema()
        key = self._key(character, memory_id)
        with self._lock, self._connect() as db:
            cursor = db.execute(
                "UPDATE rag_memories SET active=0, updated_at=? "
                "WHERE character=? AND memory_id=?",
                (self._now(), character, memory_id),
            )
            if self._fts_available:
                db.execute("DELETE FROM rag_fts WHERE memory_key=?", (key,))
            return cursor.rowcount > 0

    def supersede(self, character: str, old_id: str, new_id: str) -> bool:
        self.ensure_schema()
        with self._lock, self._connect() as db:
            cursor = db.execute(
                "UPDATE rag_memories SET active=0, supersedes=?, updated_at=? "
                "WHERE character=? AND memory_id=?",
                (new_id, self._now(), character, old_id),
            )
            if self._fts_available:
                db.execute(
                    "DELETE FROM rag_fts WHERE memory_key=?",
                    (self._key(character, old_id),),
                )
            return cursor.rowcount > 0

    @staticmethod
    def _fts_query(query: str) -> str:
        tokens = [
            token
            for token in re.findall(r"[a-zA-Z0-9ñ]+", query.lower())
            if len(token) >= 3 and token not in _FTS_STOP_WORDS
        ]
        return " OR ".join(f'"{token}"*' for token in tokens[:24])

    @staticmethod
    def _not_expired(value: str, now: datetime) -> bool:
        try:
            return datetime.fromisoformat(value) >= now
        except (TypeError, ValueError):
            return True

    @staticmethod
    def _cosine(left: bytes, right: np.ndarray) -> float:
        vector = np.frombuffer(left, dtype=np.float32)
        if vector.size != right.size:
            return -1.0
        denominator = float(np.linalg.norm(vector) * np.linalg.norm(right))
        return float(np.dot(vector, right) / denominator) if denominator else 0.0

    def search(
        self,
        character: str,
        query: str,
        *,
        candidates: list[dict] | None = None,
        limit: int = 8,
        exclude_ids: set[str] | None = None,
        use_vectors: bool = True,
    ) -> list[dict]:
        """Return ranked copies of candidate records using hybrid RRF."""
        self.ensure_schema()
        candidate_map = {
            self._key(character, str(record.get("id", ""))): record
            for record in (candidates or [])
            if record.get("id")
        }
        excluded = {str(value) for value in (exclude_ids or set())}
        lexical: list[str] = []
        vector: list[tuple[str, float]] = []
        query_vector = None
        if use_vectors and self.embedder.available():
            query_vector = self.embedder.embed_query(query)
        with self._lock, self._connect() as db:
            rows = db.execute(
                "SELECT * FROM rag_memories WHERE character=? AND active=1",
                (character,),
            ).fetchall()
            now = datetime.now()
            rows = [
                row
                for row in rows
                if self._key(character, row["memory_id"]) in candidate_map
                and (
                    not row["expires_at"]
                    or self._not_expired(row["expires_at"], now)
                )
            ]
            valid_keys = {
                self._key(character, row["memory_id"])
                for row in rows
            }
            fts_query = self._fts_query(query)
            if self._fts_available and fts_query:
                try:
                    found = db.execute(
                        "SELECT memory_key FROM rag_fts WHERE rag_fts MATCH ? "
                        "ORDER BY bm25(rag_fts) LIMIT ?",
                        (fts_query, max(limit * 8, 32)),
                    ).fetchall()
                    lexical = [
                        row["memory_key"]
                        for row in found
                        if row["memory_key"] in valid_keys
                    ]
                except sqlite3.OperationalError:
                    lexical = []

            if query_vector is not None:
                query_vector = np.asarray(query_vector, dtype=np.float32)
                scored = [
                    (
                        self._key(character, row["memory_id"]),
                        self._cosine(row["embedding"], query_vector),
                    )
                    for row in rows
                    if row["embedding"]
                    and row["embedding_model"] == self.embedder.fingerprint()
                    and int(row["embedding_version"] or 0) == self.embedder.version
                ]
                if scored:
                    top_similarity = max(item[1] for item in scored)
                    # Si FTS no aporta una coincidencia, exigimos más precisión
                    # al vector para no inventar recuerdos para cualquier pregunta.
                    # Si FTS corrobora, permite el ranking relativo de E5.
                    minimum = (
                        self.embedder.min_similarity
                        if lexical
                        else self.vector_only_min_similarity
                    )
                    vector_floor = max(
                        minimum,
                        top_similarity - 0.06,
                    )
                    vector = sorted(
                        (
                            item
                            for item in scored
                            if item[1] >= vector_floor
                        ),
                        key=lambda item: item[1],
                        reverse=True,
                    )[:4]

        excluded_keys = {self._key(character, value) for value in excluded}
        scores: dict[str, float] = {}
        sources: dict[str, set[str]] = {}
        for rank, key in enumerate(lexical, 1):
            if key in excluded_keys:
                continue
            scores[key] = scores.get(key, 0.0) + 1.0 / (self.rrf_k + rank)
            sources.setdefault(key, set()).add("fts")
        for rank, (key, _similarity) in enumerate(vector, 1):
            if key in excluded_keys:
                continue
            scores[key] = scores.get(key, 0.0) + 1.0 / (self.rrf_k + rank)
            sources.setdefault(key, set()).add("vector")

        # Metadata is only a tie-breaker; RRF remains the primary fusion.
        for key, score in list(scores.items()):
            record = candidate_map.get(key, {})
            importance = float(record.get("importancia", 5) or 5) / 10.0
            scores[key] = score + (importance * 0.0001)

        ranked = sorted(scores, key=lambda key: scores[key], reverse=True)
        result: list[dict] = []
        for key in ranked[: max(1, limit)]:
            record = dict(candidate_map[key])
            record["_rag_score"] = round(float(scores[key]), 8)
            record["_rag_sources"] = sorted(sources.get(key, set()))
            result.append(record)
        return result

    def stats(self) -> dict:
        self.ensure_schema()
        with self._lock, self._connect() as db:
            total = db.execute("SELECT COUNT(*) AS n FROM rag_memories").fetchone()["n"]
            active = db.execute(
                "SELECT COUNT(*) AS n FROM rag_memories WHERE active=1"
            ).fetchone()["n"]
            vectors = db.execute(
                "SELECT COUNT(*) AS n FROM rag_memories WHERE embedding IS NOT NULL"
            ).fetchone()["n"]
        return {
            "memories": int(total),
            "active": int(active),
            "vectors": int(vectors),
            "fts": self._fts_available,
            "embedding_model": self.embedder.fingerprint(),
            "embedding_dimension": self.embedder.dimension,
            "min_similarity": float(self.embedder.min_similarity),
            "vector_only_min_similarity": float(self.vector_only_min_similarity),
            "db_path": str(self._db_path()),
        }

    def rebuild(
        self,
        character: str,
        records: Iterable[dict],
        *,
        embed: bool = True,
    ) -> int:
        self.ensure_schema()
        with self._lock, self._connect() as db:
            db.execute("DELETE FROM rag_memories WHERE character=?", (character,))
            if self._fts_available:
                db.execute("DELETE FROM rag_fts WHERE character=?", (character,))
        return self.sync_memories(character, records, embed=embed)
