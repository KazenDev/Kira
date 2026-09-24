"""Resilient text-completion provider orchestration for Kira.

This service owns the exact legacy retry/fallback policy while remaining
independent from FastAPI and global application state.
"""

from __future__ import annotations

import asyncio
import json
import random
from collections.abc import AsyncIterator, Awaitable, Callable
from contextlib import AsyncExitStack
from typing import Any

import httpx

from app.domain.streaming import extraer_message_parcial, limpiar_json_ia, sse_event

TIMEOUT_IA = httpx.Timeout(connect=8.0, read=20.0, write=15.0, pool=8.0)
ESTADOS_REINTENTABLES = {408, 409, 429, 500, 502, 503, 504}
ESTADOS_SIN_REINTENTO = {401, 402, 403}
MAX_INTENTOS_IA = 3


class StreamVacio(Exception):
    """HTTP 200 completed without a single content delta."""


class AIProvider:
    def __init__(
        self,
        config: Any,
        *,
        client: httpx.AsyncClient | None = None,
        sleep: Callable[[int, httpx.Response | None], Awaitable[None]] | None = None,
        random_value: Callable[[], float] = random.random,
    ) -> None:
        self.config = config
        self.client = client
        self.sleep = sleep or self.esperar_reintento
        self.random_value = random_value

    async def esperar_reintento(
        self,
        intento: int,
        respuesta: httpx.Response | None = None,
    ) -> None:
        delay = min(0.5 * (2**intento), 8.0) * (1 - 0.25 * self.random_value())
        if respuesta is not None:
            try:
                retry_after = float(respuesta.headers.get("retry-after", ""))
                if 0 < retry_after <= 60:
                    delay = retry_after
            except (TypeError, ValueError):
                pass
        await asyncio.sleep(delay)

    @staticmethod
    def headers(provider: dict) -> dict:
        return {"Authorization": f"Bearer {provider['api_key']}"}

    @staticmethod
    def body(payload: dict, provider: dict) -> dict:
        return {**payload, **provider.get("extra", {}), "model": provider["modelo"]}

    @staticmethod
    def failure_reason(error: Exception | None) -> str:
        response = getattr(error, "response", None)
        if response is not None:
            return f"HTTP {response.status_code}"
        if isinstance(error, StreamVacio):
            return "HTTP 200 sin contenido"
        return type(error).__name__

    def has_fallback(self, index: int) -> bool:
        return index + 1 < len(self.config.PROVEEDORES_IA)

    def warn_fallback(self, index: int, error: Exception | None) -> None:
        current = self.config.PROVEEDORES_IA[index]["nombre"]
        next_provider = self.config.PROVEEDORES_IA[index + 1]["nombre"]
        print(
            f"[IA] {current} se agotó ({self.failure_reason(error)}) -> "
            f"CAE AL RESPALDO: {next_provider}"
        )

    async def post_json(self, payload: dict, intentos: int = MAX_INTENTOS_IA) -> dict:
        last_error: Exception | None = None
        for provider_index, provider in enumerate(self.config.PROVEEDORES_IA):
            body = self.body(payload, provider)
            for attempt in range(intentos):
                try:
                    async with AsyncExitStack() as stack:
                        client = self.client or await stack.enter_async_context(
                            httpx.AsyncClient(timeout=TIMEOUT_IA)
                        )
                        response = await client.post(
                            f"{provider['base_url']}/chat/completions",
                            headers=self.headers(provider),
                            json=body,
                            timeout=TIMEOUT_IA,
                        )
                        if response.status_code in ESTADOS_REINTENTABLES and attempt < intentos - 1:
                            print(
                                f"[IA] HTTP {response.status_code} ({provider['nombre']}), "
                                f"reintento {attempt + 1}/{intentos - 1}"
                            )
                            await self.sleep(attempt, response)
                            continue
                        response.raise_for_status()
                        data = response.json()
                        try:
                            data["choices"][0]["message"]["content"] = limpiar_json_ia(
                                data["choices"][0]["message"]["content"]
                            )
                        except (KeyError, IndexError, TypeError):
                            pass
                        return data
                except (httpx.TimeoutException, httpx.TransportError, httpx.HTTPStatusError) as error:
                    last_error = error
                    response = getattr(error, "response", None)
                    code = getattr(response, "status_code", None)
                    if code not in ESTADOS_SIN_REINTENTO and attempt < intentos - 1:
                        print(
                            f"[IA] {self.failure_reason(error)} en {provider['nombre']}, "
                            f"reintento {attempt + 1}/{intentos - 1}"
                        )
                        await self.sleep(attempt)
                        continue
                    break
            if self.has_fallback(provider_index):
                self.warn_fallback(provider_index, last_error)
        raise last_error if last_error else RuntimeError("IA no disponible")

    async def stream(
        self,
        messages: list,
        box: dict,
        temperature: float,
    ) -> AsyncIterator[str]:
        payload_base = {
            "model": self.config.MODELO_IA,
            "messages": messages,
            "response_format": self.config.JSON_MODE,
            "max_tokens": self.config.MAX_TOKENS,
            "temperature": round(temperature, 2),
            "stream": True,
        }
        buffer = ""
        provider_index = 0
        attempt = 0
        while True:
            provider = self.config.PROVEEDORES_IA[provider_index]
            payload = self.body(payload_base, provider)
            buffer = ""
            try:
                yield sse_event({"tipo": "esperando", "intento": attempt + 1})
                async with AsyncExitStack() as stack:
                    client = self.client or await stack.enter_async_context(
                        httpx.AsyncClient(timeout=TIMEOUT_IA)
                    )
                    async with client.stream(
                        "POST",
                        f"{provider['base_url']}/chat/completions",
                        headers=self.headers(provider),
                        json=payload,
                        timeout=TIMEOUT_IA,
                    ) as response:
                        if response.status_code in ESTADOS_REINTENTABLES and attempt < MAX_INTENTOS_IA - 1:
                            print(
                                f"[IA] HTTP {response.status_code} en stream ({provider['nombre']}), "
                                f"reintento {attempt + 1}"
                            )
                            await self.sleep(attempt, response)
                            attempt += 1
                            continue
                        response.raise_for_status()
                        async for line in response.aiter_lines():
                            if not line or not line.startswith("data:"):
                                continue
                            data = line[5:].strip()
                            if data == "[DONE]":
                                break
                            try:
                                chunk = json.loads(data)
                            except json.JSONDecodeError:
                                continue
                            try:
                                delta = chunk["choices"][0]["delta"].get("content", "")
                            except (KeyError, IndexError, TypeError):
                                continue
                            if not delta:
                                continue
                            buffer += delta
                            partial = extraer_message_parcial(buffer)
                            if partial is not None:
                                yield sse_event({"tipo": "delta", "texto": partial})
                if not buffer:
                    raise StreamVacio()
                break
            except (
                httpx.TimeoutException,
                httpx.TransportError,
                httpx.HTTPStatusError,
                StreamVacio,
            ) as error:
                response = getattr(error, "response", None)
                code = response.status_code if response is not None else (
                    "vacio" if isinstance(error, StreamVacio) else type(error).__name__
                )
                no_retry = code in ESTADOS_SIN_REINTENTO
                if not no_retry and attempt < MAX_INTENTOS_IA - 1:
                    print(
                        f"[IA] fallo de stream ({code}) en {provider['nombre']}, "
                        f"intento {attempt + 1}/{MAX_INTENTOS_IA}"
                    )
                    yield sse_event(
                        {"tipo": "reintento", "intento": attempt + 1, "estado": str(code)}
                    )
                    await self.sleep(attempt, response)
                    attempt += 1
                    continue
                if self.has_fallback(provider_index):
                    self.warn_fallback(provider_index, error)
                    provider_index += 1
                    attempt = 0
                    yield sse_event(
                        {
                            "tipo": "reintento",
                            "intento": MAX_INTENTOS_IA,
                            "estado": str(code),
                        }
                    )
                    continue
                print(
                    f"[IA] fallo de stream ({code}) en {provider['nombre']}, "
                    f"intento {attempt + 1}/{MAX_INTENTOS_IA}"
                )
                box["error"] = True
                if isinstance(error, StreamVacio):
                    yield sse_event(
                        {
                            "tipo": "error",
                            "mensaje": (
                                f"El cerebro contestó vacío tras {MAX_INTENTOS_IA} intentos. "
                                "Probá de nuevo."
                            ),
                        }
                    )
                elif isinstance(error, httpx.HTTPStatusError):
                    yield sse_event(
                        {
                            "tipo": "error",
                            "mensaje": (
                                f"La IA falló ({error.response.status_code}) tras "
                                f"{MAX_INTENTOS_IA} intentos"
                            ),
                        }
                    )
                else:
                    yield sse_event(
                        {
                            "tipo": "error",
                            "mensaje": (
                                f"El cerebro no responde tras {MAX_INTENTOS_IA} intentos"
                            ),
                        }
                    )
                return
            except Exception as error:
                box["error"] = True
                print(f"[STREAM] error: {error}")
                yield sse_event({"tipo": "error", "mensaje": f"La IA falló: {error}"})
                return
        buffer = limpiar_json_ia(buffer)
        box["buffer"] = buffer
        try:
            box["salida"] = json.loads(buffer)
        except Exception:
            box["salida"] = {}
