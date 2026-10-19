"""Lazy TTS registry and ElevenLabs streaming service."""

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

    def register(self, text: str, voice: str, previous: str = "") -> str:
        """previous = lo que Kira ya dijo antes de esta frase: ElevenLabs lo
        usa para que la entonacion CONTINUE entre frases (si no, cada audio
        arranca como si fuera el primero y se nota el corte)."""
        with self._lock:
            if len(self.pending) >= self.cache_max:
                self.pending.pop(next(iter(self.pending)))
            tts_id = str(uuid.uuid4())
            self.pending[tts_id] = {"texto": text, "voz": voice, "previo": previous}
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
                # ElevenLabs: la voz va en la URL y el formato como query;
                # /stream devuelve el mp3 a medida que se genera.
                async with client.stream(
                    "POST",
                    f"{self.config.ELEVEN_BASE_URL}/v1/text-to-speech/"
                    f"{params['voz']}/stream",
                    timeout=90,
                    params={"output_format": self.config.ELEVEN_FORMATO},
                    headers={"xi-api-key": self.config.ELEVENLABS_API_KEY},
                    json={
                        "text": params["texto"],
                        "model_id": self.config.ELEVEN_MODELO,
                        **({"previous_text": params["previo"]} if params.get("previo") else {}),
                    },
                ) as response:
                    if response.status_code >= 400:
                        # El cuerpo del error se lee ACA ADENTRO, con la
                        # respuesta todavia abierta (aread: el cliente es
                        # async). Afuera del "async with" ya esta cerrada y
                        # leerla tira error: por eso antes el motivo real
                        # (401, sin saldo, modelo inexistente) no se veia.
                        cuerpo = (await response.aread())[:200].decode("utf-8", "replace")
                        print(f"[TTS-STREAM] error {response.status_code}: {cuerpo}")
                        yield b""
                        return
                    async for chunk in response.aiter_bytes():
                        buffer.extend(chunk)
                        yield chunk
            if buffer:
                self._store_bytes(tts_id, bytes(buffer))
        except TTSNotFound:
            raise
        except Exception as error:
            print(f"[TTS-STREAM] error: {error}")
            yield b""
