"""Lazy TTS registry and ElevenLabs streaming service."""

from __future__ import annotations

import base64
import json
import threading
import uuid
from collections.abc import AsyncIterator
from contextlib import AsyncExitStack
from typing import Any

import httpx


class TTSNotFound(KeyError):
    pass


def marcas_de_alineacion(caracteres: list[str], inicios: list[float],
                         fin: float | None = None) -> list[list[float]]:
    """De la alineacion de ElevenLabs (tiempo de cada caracter del texto que
    se le mando) a marcas [segundo, letras VISIBLES ya dichas], una por
    palabra. Con esto la pantalla revela el texto al ritmo de la voz.

    "Visible" = exactamente lo que deja quitar_tags_audio (se probo con
    fuzzing en test_robustez.py): los audio tags ([laughs]) suenan pero no se
    ven, los espacios dobles se juntan, el espacio antes de la puntuacion se
    va ("hola ... chau" se ve "hola... chau") y los bordes no cuentan."""
    # 1) sin los audio tags
    pares: list[tuple[str, float]] = []
    en_tag = False
    for c, t in zip(caracteres, inicios):
        if c == "[":
            en_tag = True
            continue
        if en_tag:
            en_tag = c != "]"
            continue
        pares.append((c, float(t)))
    # 2) la misma limpieza de espacios que la pantalla
    limpio: list[tuple[str, float]] = []
    for i, (c, t) in enumerate(pares):
        if c in " \t":
            if not limpio or limpio[-1][0] in " \t\n":
                continue                       # inicio o espacio repetido
            sig = next((x for x, _ in pares[i + 1:] if not x.isspace()), None)
            if sig is None or sig in ",.;:!?…":
                continue                       # al final, o antes de puntuacion
        elif c.isspace() and (not limpio or sig_es_puntuacion(pares, i)):
            continue
        limpio.append((c, t))
    # 3) una marca por palabra
    marcas: list[list[float]] = [[0.0, 0]]
    for k, (c, t) in enumerate(limpio):
        if c.isspace() and marcas[-1][1] != k:
            marcas.append([round(t, 3), k])
    if fin is not None and limpio:
        marcas.append([round(float(fin), 3), len(limpio)])
    return marcas


def sig_es_puntuacion(pares: list[tuple[str, float]], i: int) -> bool:
    sig = next((x for x, _ in pares[i + 1:] if not x.isspace()), None)
    return sig is None or sig in ",.;:!?…"


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
        self.tiempos_cache: dict[str, list[list[float]]] = {}
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

    def get_tiempos(self, tts_id: str) -> list[list[float]] | None:
        with self._lock:
            return self.tiempos_cache.get(tts_id)

    def _store_tiempos(self, tts_id: str, marcas: list[list[float]]) -> None:
        with self._lock:
            self.tiempos_cache[tts_id] = marcas
            if len(self.tiempos_cache) > self.cache_max:
                self.tiempos_cache.pop(next(iter(self.tiempos_cache)))

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
        caracteres: list[str] = []
        inicios: list[float] = []
        fin: float | None = None
        try:
            async with AsyncExitStack() as stack:
                client = self.client or await stack.enter_async_context(
                    httpx.AsyncClient(timeout=90)
                )
                # ElevenLabs: la voz va en la URL y el formato como query.
                # /stream/with-timestamps manda el mp3 a medida que se genera
                # (en base64, una linea JSON por trozo) JUNTO con el tiempo de
                # cada caracter: misma latencia que /stream (medido) y la
                # pantalla puede escribir el texto al ritmo de la voz.
                async with client.stream(
                    "POST",
                    f"{self.config.ELEVEN_BASE_URL}/v1/text-to-speech/"
                    f"{params['voz']}/stream/with-timestamps",
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
                    async for linea in response.aiter_lines():
                        if not linea.strip():
                            continue
                        try:
                            trozo = json.loads(linea)
                        except ValueError:
                            continue
                        alineacion = trozo.get("alignment") or {}
                        caracteres.extend(alineacion.get("characters") or [])
                        inicios.extend(alineacion.get("character_start_times_seconds") or [])
                        fines = alineacion.get("character_end_times_seconds") or []
                        if fines:
                            fin = fines[-1]
                        audio = base64.b64decode(trozo.get("audio_base64") or "")
                        if audio:
                            buffer.extend(audio)
                            yield audio
            if buffer:
                self._store_bytes(tts_id, bytes(buffer))
                if caracteres:
                    self._store_tiempos(tts_id, marcas_de_alineacion(caracteres, inicios, fin))
        except TTSNotFound:
            raise
        except Exception as error:
            print(f"[TTS-STREAM] error: {error}")
            yield b""
