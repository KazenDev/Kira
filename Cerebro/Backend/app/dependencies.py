"""Shared application dependencies.

The legacy runtime still owns the concrete implementations during the first
refactor phase. Routers receive them through a small context object instead of
importing ``kira_server`` directly, which avoids a circular dependency and gives
later phases a stable seam for dependency injection.
"""

from __future__ import annotations

from collections.abc import Awaitable, Callable, Mapping
from dataclasses import dataclass
from typing import Any

from fastapi import Request


@dataclass(frozen=True, slots=True)
class BackendContext:
    """Runtime collaborators used by modular API routers."""

    config: Any
    auth_store: Any
    tenant_manager: Any
    auth_required: bool
    cargar_personaje: Callable[[str], dict]
    memoria_cargar: Callable[..., list[dict]]
    memoria_olvidar: Callable[..., dict | None]
    memoria_borrar_todas: Callable[[str, str], int]
    diario_cargar: Callable[[str], dict]
    memoria_estado_cargar: Callable[[str], dict]
    post_json_ia: Callable[[dict], Awaitable[dict]]
    serial_manager: Callable[[], Any]
    estados: Mapping[str, Any]
    conversations_load: Callable[[str, int], list[dict]]
    grabaciones_dir: Callable[[], Any]
    rag_stats: Callable[[], dict]
    chat: Callable[[dict], Awaitable[Any]]
    chat_stream: Callable[[dict], Awaitable[Any]]
    tts_stream: Callable[[str], Awaitable[Any]]
    transcribir: Callable[[Any], Awaitable[Any]]
    grabar: Callable[[dict], Awaitable[Any]]
    escuchar: Callable[[], Awaitable[Any]]


def get_backend_context(request: Request) -> BackendContext:
    """Return the immutable context attached to the current FastAPI app."""

    context = getattr(request.app.state, "backend_context", None)
    if context is None:
        raise RuntimeError("BackendContext no está asociado a la aplicación FastAPI")
    return context
