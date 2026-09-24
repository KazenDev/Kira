"""TTS, recording, transcription and VAD listening routes."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from fastapi import APIRouter, Depends, File, HTTPException, UploadFile
from fastapi.responses import FileResponse

from ..dependencies import BackendContext, get_backend_context
from ..schemas import GrabarRequest

router = APIRouter()


@router.get("/api/archivos/{nombre}")
@router.get("/grabaciones/{nombre}")
async def api_archivo(
    nombre: str,
    services: BackendContext = Depends(get_backend_context),
) -> Any:
    if Path(nombre).name != nombre or not nombre or nombre.startswith("."):
        raise HTTPException(404, "archivo no encontrado")
    path = Path(services.grabaciones_dir()) / nombre
    if not path.is_file():
        raise HTTPException(404, "archivo no encontrado")
    return FileResponse(path)


@router.get("/api/tts-stream/{tts_id}")
async def api_tts_stream(
    tts_id: str,
    services: BackendContext = Depends(get_backend_context),
) -> Any:
    return await services.tts_stream(tts_id)


@router.post("/api/transcribir")
async def api_transcribir(
    archivo: UploadFile = File(...),
    services: BackendContext = Depends(get_backend_context),
) -> Any:
    return await services.transcribir(archivo)


@router.post("/api/grabar")
async def api_grabar(
    body: GrabarRequest,
    services: BackendContext = Depends(get_backend_context),
) -> Any:
    return await services.grabar(body.model_dump())


@router.post("/api/escuchar")
async def api_escuchar(
    services: BackendContext = Depends(get_backend_context),
) -> Any:
    return await services.escuchar()
