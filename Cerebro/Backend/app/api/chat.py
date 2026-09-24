"""Chat HTTP routes.

The orchestration implementations remain in the legacy runtime during phase 1;
this module owns the public route surface and delegates through BackendContext.
"""

from __future__ import annotations

from typing import Any

from fastapi import APIRouter, Depends

from ..dependencies import BackendContext, get_backend_context
from ..schemas import ChatRequest

router = APIRouter()


@router.post("/api/chat")
async def api_chat(
    body: ChatRequest,
    services: BackendContext = Depends(get_backend_context),
) -> Any:
    return await services.chat(body.model_dump())


@router.post("/api/chat/stream")
async def api_chat_stream(
    body: ChatRequest,
    services: BackendContext = Depends(get_backend_context),
) -> Any:
    return await services.chat_stream(body.model_dump())
