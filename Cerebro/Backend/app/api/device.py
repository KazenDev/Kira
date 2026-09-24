"""BLE relay and micro:bit control routes.

Behavior is intentionally identical to the legacy endpoints during phase 1.
Security and concurrency hardening are separate future phases.
"""

from __future__ import annotations

import asyncio
import time
from typing import Any

from fastapi import APIRouter, Depends, HTTPException
from fastapi.responses import JSONResponse

from ..dependencies import BackendContext, get_backend_context
from ..schemas import (
    BridgeErrorRequest,
    BridgeTokenRequest,
    ComandoRequest,
    EmocionRequest,
)

router = APIRouter()


@router.post("/api/ble/conectar")
async def api_ble_conectar(
    body: BridgeTokenRequest = BridgeTokenRequest(),
    services: BackendContext = Depends(get_backend_context),
):
    raw_token = body.token if "token" in body.model_fields_set else ""
    token = str(raw_token).strip()
    if not token:
        return JSONResponse({"ok": False, "error": "falta el token"}, status_code=400)
    manager = services.serial_manager()
    resultado = manager.relay_registrar(token)
    if resultado == "ocupado":
        return JSONResponse(
            {"ok": False, "error": "otro dispositivo tiene el puente"},
            status_code=409,
        )
    if resultado == "bucle":
        return JSONResponse(
            {"ok": False, "error": "demasiados registros seguidos: recargá la app"},
            status_code=429,
        )
    manager.ble_error = None
    print(f"[BLE RELAY] puente registrado (token {token[:8]}...): comandos por aire")
    return {"ok": True}


@router.post("/api/ble/error")
async def api_ble_error(
    datos: BridgeErrorRequest = BridgeErrorRequest(),
    services: BackendContext = Depends(get_backend_context),
):
    raw_message = datos.mensaje if "mensaje" in datos.model_fields_set else ""
    mensaje = str(raw_message)[:300]
    services.serial_manager().ble_error = mensaje
    print(f"[BLE RELAY] ERROR del navegador: {mensaje}")
    return {"ok": True}


@router.post("/api/ble/desconectar")
async def api_ble_desconectar(
    body: BridgeTokenRequest = BridgeTokenRequest(),
    services: BackendContext = Depends(get_backend_context),
):
    raw_token = body.token if "token" in body.model_fields_set else ""
    token = str(raw_token).strip()
    if token:
        services.serial_manager().relay_liberar(token)
        print("[BLE RELAY] puente retirado por el dueño")
    return {"ok": True}


@router.get("/api/ble/tx")
async def api_ble_tx(
    t: str = "",
    espera: float = 0,
    services: BackendContext = Depends(get_backend_context),
):
    manager = services.serial_manager()
    estado = manager.relay_latido(t)
    if estado == "ocupado":
        return JSONResponse({"error": "otro dispositivo tiene el puente"}, status_code=409)
    if estado != "ok":
        return JSONResponse({"error": "puente expirado, reconecta"}, status_code=410)

    espera = max(0.0, min(espera, 5.0))
    if espera > 0:
        fin = time.monotonic() + espera
        while time.monotonic() < fin:
            lineas = manager.relay_tomar_tx()
            if lineas:
                return {"lineas": lineas}
            await asyncio.sleep(0.03)
        return {"lineas": []}
    return {"lineas": manager.relay_tomar_tx()}


@router.get("/api/ble/ping")
async def api_ble_ping(
    t: str = "",
    services: BackendContext = Depends(get_backend_context),
):
    estado = services.serial_manager().relay_latido(t)
    if estado == "ocupado":
        return JSONResponse({"error": "otro dispositivo tiene el puente"}, status_code=409)
    if estado != "ok":
        return JSONResponse({"error": "puente expirado, reconecta"}, status_code=410)
    return {"ok": True}


@router.post("/api/ble/nack")
async def api_ble_nack(
    lineas: list[str] = [],
    t: str = "",
    services: BackendContext = Depends(get_backend_context),
):
    manager = services.serial_manager()
    estado = manager.relay_latido(t)
    if estado == "ocupado":
        return JSONResponse({"error": "otro dispositivo tiene el puente"}, status_code=409)
    if estado != "ok":
        return JSONResponse({"error": "puente expirado, reconecta"}, status_code=410)
    manager.relay_devolver(lineas)
    return {"ok": True, "devueltos": len(lineas)}


@router.post("/api/ble/rx")
async def api_ble_rx(
    lineas: list[str] = [],
    t: str = "",
    services: BackendContext = Depends(get_backend_context),
):
    manager = services.serial_manager()
    estado = manager.relay_latido(t)
    if estado == "ocupado":
        return JSONResponse({"error": "otro dispositivo tiene el puente"}, status_code=409)
    for linea in lineas:
        manager.relay_linea_entrante(linea)
    return {"ok": True, "cantidad": len(lineas)}


@router.post("/api/loading")
async def api_loading(services: BackendContext = Depends(get_backend_context)):
    services.serial_manager().enviar("LOADING")
    return {"ok": True}


@router.post("/api/voz")
async def api_voz(services: BackendContext = Depends(get_backend_context)):
    services.serial_manager().enviar("VOZ")
    return {"ok": True}


@router.post("/api/talk")
async def api_talk(services: BackendContext = Depends(get_backend_context)):
    services.serial_manager().enviar("TALK")
    return {"ok": True}


@router.post("/api/calla")
async def api_calla(services: BackendContext = Depends(get_backend_context)):
    services.serial_manager().enviar("CALLA")
    return {"ok": True}


@router.post("/api/emocion")
async def api_emocion(
    body: EmocionRequest,
    services: BackendContext = Depends(get_backend_context),
):
    raw_emotion = body.emotion if "emotion" in body.model_fields_set else "happy"
    emotion = str(raw_emotion).lower()
    command = services.config.EMOCION_SERIAL.get(emotion, "NEUTRAL")
    services.serial_manager().enviar(command)
    return {"ok": True}


@router.post("/api/sync")
async def api_sync(services: BackendContext = Depends(get_backend_context)):
    ultima = "HAPPY"
    for estado in services.estados.values():
        command = services.config.EMOCION_SERIAL.get(estado.emotion)
        if command:
            ultima = command
    services.serial_manager().enviar(ultima)
    return {"ok": True, "comando": ultima}


@router.post("/api/stop")
async def api_stop(services: BackendContext = Depends(get_backend_context)):
    services.serial_manager().enviar("STOP")
    return {"ok": True}


@router.post("/api/cancelar")
async def api_cancelar(services: BackendContext = Depends(get_backend_context)):
    # Descarta una captura manual sin convertirla en un AUDIO:END enviable.
    services.serial_manager().enviar("CANCELAR")
    return {"ok": True}


@router.post("/api/comando")
async def api_comando(
    body: ComandoRequest,
    services: BackendContext = Depends(get_backend_context),
):
    raw_command = body.cmd if "cmd" in body.model_fields_set else ""
    cmd = str(raw_command).strip().upper()
    if not cmd or len(cmd) > 24 or "\n" in cmd or "\r" in cmd:
        raise HTTPException(status_code=400, detail="comando invalido")
    services.serial_manager().enviar(cmd)
    return {"ok": True, "comando": cmd}
