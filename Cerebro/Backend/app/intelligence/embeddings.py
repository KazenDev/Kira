"""Embedding providers for Kira's local-first RAG memory.

FastEmbed is optional at import time. The application can always start and
fall back to lexical retrieval; the model is loaded only when the RAG index or
query path actually needs it.
"""

from __future__ import annotations

import hashlib
import os
import re
import threading
import unicodedata
from pathlib import Path
from typing import Any

import numpy as np


class EmbeddingProvider:
    """Small provider contract shared by tests and production backends."""

    name = "none"
    dimension = 0
    version = 1
    min_similarity = 0.0

    def available(self) -> bool:
        return False

    def embed_documents(self, texts: list[str]) -> np.ndarray | None:
        return None

    def embed_query(self, text: str) -> np.ndarray | None:
        return None

    def fingerprint(self) -> str:
        return f"{self.name}:{self.dimension}:v{self.version}"


class NullEmbeddingProvider(EmbeddingProvider):
    """Explicitly disabled provider; lexical FTS remains available."""


def _tokens(text: str) -> list[str]:
    plain = "".join(
        char
        for char in unicodedata.normalize("NFD", text.lower())
        if unicodedata.category(char) != "Mn"
    )
    return re.findall(r"[a-z0-9ñ]+", plain)


class HashEmbeddingProvider(EmbeddingProvider):
    """Deterministic test provider; never use it as a production semantic model."""

    name = "test-hash"
    dimension = 64
    version = 1

    def available(self) -> bool:
        return True

    def embed_documents(self, texts: list[str]) -> np.ndarray:
        return np.asarray([self._one(text) for text in texts], dtype=np.float32)

    def embed_query(self, text: str) -> np.ndarray:
        return self._one(text)

    def _one(self, text: str) -> np.ndarray:
        vector = np.zeros(self.dimension, dtype=np.float32)
        for token in _tokens(text):
            digest = hashlib.blake2b(token.encode("utf-8"), digest_size=8).digest()
            number = int.from_bytes(digest, "little")
            index = number % self.dimension
            sign = 1.0 if number & 1 else -1.0
            vector[index] += sign
        norm = float(np.linalg.norm(vector))
        return vector / norm if norm else vector


class FastEmbedProvider(EmbeddingProvider):
    """Local ONNX embeddings through Qdrant FastEmbed.

    E5 is asymmetric: queries use ``query:`` and stored passages use
    ``passage:``. Prefixes are explicit here because FastEmbed's generic helper
    does not guarantee model-specific prefixing.
    """

    name = "intfloat/multilingual-e5-small"
    dimension = 384
    version = 1
    min_similarity = 0.80

    def __init__(
        self,
        *,
        model_name: str | None = None,
        cache_dir: str | None = None,
        model_file: str | None = None,
        allow_download: bool = True,
        batch_size: int = 16,
        threads: int = 2,
        min_similarity: float | None = None,
    ) -> None:
        self.name = model_name or os.getenv(
            "KIRA_RAG_MODEL", "intfloat/multilingual-e5-small"
        )
        self.dimension = 384 if "e5-small" in self.name else 0
        self.min_similarity = float(
            min_similarity
            if min_similarity is not None
            else os.getenv("KIRA_RAG_MIN_SIMILARITY", "0.80")
        )
        self.cache_dir = cache_dir or os.getenv(
            "KIRA_RAG_CACHE_DIR",
            str(Path.home() / ".cache" / "kira" / "fastembed"),
        )
        self.model_file = model_file or os.getenv(
            "KIRA_RAG_MODEL_FILE",
            "onnx/model_O4.onnx",
        )
        self.allow_download = allow_download
        self.batch_size = max(1, int(batch_size))
        self.threads = max(1, int(threads))
        self._model: Any = None
        self._load_error: Exception | None = None
        self._model_lock = threading.RLock()
        self._embed_lock = threading.RLock()

    def available(self) -> bool:
        if self._load_error is not None:
            return False
        try:
            import fastembed  # noqa: F401
        except Exception:
            return False
        return True

    def fingerprint(self) -> str:
        return f"{self.name}:{self.dimension}:{self.model_file}:v{self.version}"

    def _ensure_model(self) -> Any:
        with self._model_lock:
            if self._model is not None:
                return self._model
            if self._load_error is not None:
                raise self._load_error
            try:
                from fastembed import TextEmbedding
                from fastembed.common.model_description import ModelSource, PoolingType

                # FastEmbed 0.8 incluye E5-large, pero no E5-small en su
                # catálogo por defecto. Lo registramos como modelo ONNX local.
                if self.name == "intfloat/multilingual-e5-small":
                    try:
                        TextEmbedding.add_custom_model(
                            model=self.name,
                            pooling=PoolingType.MEAN,
                            normalization=True,
                            sources=ModelSource(hf=self.name),
                            dim=384,
                            model_file=self.model_file,
                            description="Multilingual E5 small for local Kira RAG",
                            license="mit",
                        )
                    except Exception:
                        # Another component may already have registered it.
                        pass

                kwargs: dict[str, Any] = {
                    "model_name": self.name,
                    "cache_dir": self.cache_dir,
                    "threads": self.threads,
                }
                if not self.allow_download:
                    kwargs["local_files_only"] = True
                try:
                    self._model = TextEmbedding(**kwargs)
                except TypeError:
                    kwargs.pop("local_files_only", None)
                    self._model = TextEmbedding(**kwargs)
                return self._model
            except Exception as error:
                self._load_error = error
                raise

    def warmup(self) -> bool:
        """Load/cache the model and return whether it is ready."""
        try:
            self._ensure_model()
            return True
        except Exception as error:
            print(f"[RAG] no se pudo preparar el modelo: {type(error).__name__}: {error}")
            return False

    def _embed(self, texts: list[str]) -> np.ndarray | None:
        if not texts:
            return np.zeros((0, self.dimension), dtype=np.float32)
        if self.dimension <= 0:
            print(f"[RAG] modelo sin dimensión conocida: {self.name}")
            return None
        if not self.available():
            return None
        try:
            model = self._ensure_model()
            with self._embed_lock:
                vectors = list(
                    model.embed(texts, batch_size=self.batch_size)
                )
            result = np.asarray(vectors, dtype=np.float32)
            if result.ndim != 2 or result.shape[0] != len(texts):
                return None
            norms = np.linalg.norm(result, axis=1, keepdims=True)
            norms[norms == 0] = 1.0
            return result / norms
        except Exception as error:
            print(f"[RAG] embeddings no disponibles: {type(error).__name__}: {error}")
            return None

    def embed_documents(self, texts: list[str]) -> np.ndarray | None:
        return self._embed(["passage: " + str(text) for text in texts])

    def embed_query(self, text: str) -> np.ndarray | None:
        result = self._embed(["query: " + str(text)])
        return None if result is None else result[0]


def make_embedding_provider(
    *,
    provider_name: str | None = None,
    model_name: str | None = None,
    cache_dir: str | None = None,
    model_file: str | None = None,
    allow_download: bool | None = None,
    batch_size: int | None = None,
    threads: int | None = None,
    min_similarity: float | None = None,
) -> EmbeddingProvider:
    """Build the configured provider without downloading/loading a model."""
    provider = (provider_name or os.getenv("KIRA_RAG_PROVIDER", "fastembed")).strip().lower()
    if provider in {"none", "off", "disabled"}:
        return NullEmbeddingProvider()
    if provider in {"hash", "test"}:
        return HashEmbeddingProvider()
    if allow_download is None:
        allow_download = os.getenv("KIRA_RAG_ALLOW_DOWNLOAD", "1").strip().lower() not in {
            "0",
            "false",
            "no",
            "off",
        }
    return FastEmbedProvider(
        model_name=model_name,
        cache_dir=cache_dir,
        model_file=model_file,
        allow_download=allow_download,
        batch_size=batch_size or int(os.getenv("KIRA_RAG_BATCH_SIZE", "16")),
        threads=threads or int(os.getenv("KIRA_RAG_THREADS", "2")),
        min_similarity=min_similarity,
    )
