"""FastAPI application factory for Kira."""

from __future__ import annotations

from collections.abc import AsyncIterator, Callable
from contextlib import AbstractAsyncContextManager, asynccontextmanager

from fastapi import FastAPI
from fastapi.middleware.cors import CORSMiddleware

from .api import audio, auth, chat, device, system
from .auth import TenantAuthMiddleware
from .dependencies import BackendContext

LifespanFactory = Callable[[FastAPI], AbstractAsyncContextManager[None]]


def create_app(context: BackendContext, lifespan: LifespanFactory) -> FastAPI:
    """Create the application without changing the public ``kira_server:app``.

    CORS values intentionally match the current backend for now. Security
    hardening is tracked separately and is outside this phase.
    """

    @asynccontextmanager
    async def _lifespan(application: FastAPI) -> AsyncIterator[None]:
        async with lifespan(application):
            yield

    application = FastAPI(title="Kira — Cerebro", lifespan=_lifespan)
    application.add_middleware(
        CORSMiddleware,
        allow_origins=["*"],
        allow_methods=["*"],
        allow_headers=["*"],
    )
    application.add_middleware(TenantAuthMiddleware, backend_context=context)
    application.state.backend_context = context
    application.include_router(auth.router)
    application.include_router(system.router)
    application.include_router(device.router)
    application.include_router(chat.router)
    application.include_router(audio.router)
    return application
