"""Lazy TTS registry and Fish Audio streaming service."""

from __future__ import annotations

import threading
import uuid
from collections.abc import AsyncIterator
from contextlib import AsyncExitStack
from typing import Any

import httpx


class TTSNotFound(KeyError):
    pass


class SpeechService:
    def __init__(
        self,
        config: Any,
        cache_max: int = 50,
        client: httpx.AsyncClient | None = None,
    ) -> None:
        self.config = config
        self.cache_max = cache_max
        self.client = client
        self.pending: dict[str, dict[str, str]] = {}
        self.bytes_cache: dict[str, bytes] = {}
        self._lock = threading.RLock()

    def register(self, text: str, voice: str) -> str:
        with self._lock:
            if len(self.pending) >= self.cache_max:
                self.pending.pop(next(iter(self.pending)))
            tts_id = str(uuid.uuid4())
            self.pending[tts_id] = {"texto": text, "voz": voice}
            return tts_id

    def get_cached(self, tts_id: str) -> bytes | None:
        with self._lock:
            return self.bytes_cache.get(tts_id)

    def get_pending(self, tts_id: str) -> dict[str, str] | None:
        with self._lock:
            return self.pending.get(tts_id)

    def _store_bytes(self, tts_id: str, audio: bytes) -> None:
        with self._lock:
            self.bytes_cache[tts_id] = audio
            if len(self.bytes_cache) > self.cache_max:
                self.bytes_cache.pop(next(iter(self.bytes_cache)))

    async def stream(self, tts_id: str) -> AsyncIterator[bytes]:
        params = self.get_pending(tts_id)
        if params is None:
            raise TTSNotFound(tts_id)
        buffer = bytearray()
        try:
            async with AsyncExitStack() as stack:
                client = self.client or await stack.enter_async_context(
                    httpx.AsyncClient(timeout=90)
                )
                async with client.stream(
                    "POST",
                    f"{self.config.FISH_BASE_URL}/v1/tts",
                    timeout=90,
                    headers={
                        "Authorization": f"Bearer {self.config.FISH_API_KEY}",
                        "model": self.config.FISH_MODELO,
                    },
                    json={
                        "text": params["texto"],
                        "reference_id": params["voz"],
                        "format": "mp3",
                        "chunk_length": 100,
                        "min_chunk_length": 30,
                        "latency": "balanced",
                    },
                ) as response:
                    response.raise_for_status()
                    async for chunk in response.aiter_bytes():
                        buffer.extend(chunk)
                        yield chunk
            if buffer:
                self._store_bytes(tts_id, bytes(buffer))
        except TTSNotFound:
            raise
        except httpx.HTTPStatusError as error:
            print(
                f"[TTS-STREAM] error {error.response.status_code}: "
                f"{error.response.text[:150]}"
            )
            yield b""
        except Exception as error:
            print(f"[TTS-STREAM] error: {error}")
            yield b""
