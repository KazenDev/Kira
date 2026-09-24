"""Lifespan-managed HTTP clients shared by AI and TTS services."""

from __future__ import annotations

import httpx


class HttpClients:
    def __init__(self) -> None:
        self.ai: httpx.AsyncClient | None = None
        self.tts: httpx.AsyncClient | None = None

    async def start(self) -> None:
        if self.ai is None:
            self.ai = httpx.AsyncClient()
        if self.tts is None:
            self.tts = httpx.AsyncClient()

    async def close(self) -> None:
        clients = [client for client in (self.ai, self.tts) if client is not None]
        for client in clients:
            await client.aclose()
        self.ai = None
        self.tts = None
