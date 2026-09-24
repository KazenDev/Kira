"""Routes for characters, memory, titles and public device status."""

from __future__ import annotations

import asyncio
import json
from typing import Any

from fastapi import APIRouter, Depends, HTTPException

from ..dependencies import BackendContext, get_backend_context
from ..schemas import MemoriaBorrarTodoRequest, MemoriaOlvidarRequest, TituloRequest

router = APIRouter()

TITULO_PROMPT = (
    "Genera un titulo corto de 3-5 palabras con un emoji al inicio que resuma "
    "el tema de la conversacion. El emoji debe representar el tema. "
    "Escribi en el idioma de la conversacion (espanol). Sin comillas, sin "
    "markdown, sin texto extra. Solo JSON valido con la clave 'title'. "
    "Ejemplo: {'title': '\U0001f355 Receta de pizza'}"
)


@router.get("/api/personajes")
async def api_personajes(
    services: BackendContext = Depends(get_backend_context),
) -> dict[str, list[dict[str, Any]]]:
    lista = []
    for nombre in services.config.PERSONAJES:
        pj = services.cargar_personaje(nombre)
        lista.append(
            {
                "id": nombre,
                "nombre": pj.get("nombre"),
                "rol": pj.get("rol"),
                "descripcion": pj.get("descripcion", ""),
                "emoji": pj.get("emoji"),
                "color": pj.get("color"),
                "saludo": pj.get("saludo"),
            }
        )
    return {"personajes": lista}


def _clase_memoria(record: dict[str, Any]) -> str:
    declared = str(record.get("memory_class", "")).strip().lower()
    if declared in {"semantic", "episodic", "reflection"}:
        return declared
    if record.get("identity") is True or record.get("source") in {
        "llm_memory_extraction", "explicit_tool",
    }:
        return "semantic"
    if str(record.get("tipo", "")).lower() == "pensamiento":
        return "reflection"
    # Los registros anteriores a la clasificación se conservan como
    # experiencias; no se inventa un hecho durable sólo por su forma de texto.
    return "episodic"


def _memoria_publica(record: dict[str, Any]) -> dict[str, Any]:
    return {
        "id": str(record.get("id") or ""),
        "texto": record.get("texto", ""),
        "importancia": record.get("importancia", 5),
        "fecha": record.get("fecha", ""),
        "tipo": record.get("tipo", "observacion"),
        "clase": _clase_memoria(record),
    }


@router.get("/api/memoria/{personaje}")
async def api_memoria(
    personaje: str,
    limite: int = 8,
    services: BackendContext = Depends(get_backend_context),
) -> dict[str, Any]:
    personaje = personaje.lower()
    if personaje not in services.config.PERSONAJES:
        raise HTTPException(404, "personaje no existe")
    limite = max(1, min(limite, 20))
    todos = services.memoria_cargar(personaje)
    hechos = [r for r in todos if _clase_memoria(r) == "semantic"]
    experiencias = [r for r in todos if _clase_memoria(r) == "episodic"]
    reflexiones = [r for r in todos if _clase_memoria(r) == "reflection"]
    diario = services.diario_cargar(personaje)
    estado = services.memoria_estado_cargar(personaje)
    # `recuerdos` sigue existiendo para clientes viejos, pero la lista curada
    # pone los hechos duraderos primero y deja las experiencias como contexto.
    curados = (hechos[::-1] + (experiencias + reflexiones)[::-1])[:limite]
    return {
        "total_recuerdos": estado.get("total", 0),
        "total_activos": len(todos),
        "total_registros_historicos": estado.get("total", 0),
        "total_hechos": len(hechos),
        "total_experiencias": len(experiencias),
        "total_reflexiones": len(reflexiones),
        "hechos": [_memoria_publica(r) for r in hechos[::-1][:limite]],
        "experiencias": [_memoria_publica(r) for r in experiencias[::-1][:limite]],
        "reflexiones": [_memoria_publica(r) for r in reflexiones[::-1][:limite]],
        "recuerdos": [_memoria_publica(r) for r in curados],
        "diario": {
            "yo_soy": diario.get("yo_soy", ""),
            "opiniones": [o.get("texto", "") for o in diario.get("opiniones", [])[:10]],
            "gustos": diario.get("gustos", [])[:10],
            "actualizado": diario.get("actualizado"),
        },
    }


@router.post("/api/memoria/{personaje}/olvidar")
async def api_memoria_olvidar(
    body: MemoriaOlvidarRequest,
    personaje: str,
    servicios: BackendContext = Depends(get_backend_context),
) -> dict[str, Any]:
    personaje = personaje.lower()
    if personaje not in servicios.config.PERSONAJES:
        raise HTTPException(404, "personaje no existe")
    memory_id = str(body.id or "").strip()
    if not memory_id:
        raise HTTPException(400, "falta el id de la memoria")
    tombstone = await asyncio.to_thread(
        servicios.memoria_olvidar,
        personaje,
        memory_id,
        str(body.motivo or ""),
    )
    if tombstone is None:
        raise HTTPException(404, "memoria no encontrada")
    return {"ok": True, "id": memory_id, "tombstone_id": tombstone.get("id")}


@router.post("/api/memoria/{personaje}/borrar-todo")
async def api_memoria_borrar_todo(
    body: MemoriaBorrarTodoRequest,
    personaje: str,
    servicios: BackendContext = Depends(get_backend_context),
) -> dict[str, Any]:
    personaje = personaje.lower()
    if personaje not in servicios.config.PERSONAJES:
        raise HTTPException(404, "personaje no existe")
    count = await asyncio.to_thread(
        servicios.memoria_borrar_todas,
        personaje,
        str(body.motivo or "memoria borrada desde Ajustes"),
    )
    return {"ok": True, "borradas": count}


@router.get("/api/conversaciones/{personaje}")
async def api_conversaciones(
    personaje: str,
    limite: int = 20,
    services: BackendContext = Depends(get_backend_context),
) -> dict[str, list[dict[str, Any]]]:
    personaje = personaje.lower()
    if personaje not in services.config.PERSONAJES:
        raise HTTPException(404, "personaje no existe")
    limite = max(1, min(limite, 100))
    turnos = await asyncio.to_thread(
        services.conversations_load,
        personaje,
        limite,
    )
    return {"turnos": turnos[::-1]}


@router.post("/api/titulo")
async def api_titulo(
    body: TituloRequest,
    services: BackendContext = Depends(get_backend_context),
) -> dict[str, str]:
    primer_mensaje = str(body.primer_mensaje).strip()
    primera_respuesta = str(body.primera_respuesta).strip()
    if not primer_mensaje:
        raise HTTPException(400, "primer_mensaje vacio")

    messages = [
        {"role": "system", "content": TITULO_PROMPT},
        {"role": "user", "content": primer_mensaje},
    ]
    if primera_respuesta:
        messages.append({"role": "assistant", "content": primera_respuesta})

    payload = {
        "model": services.config.MODELO_IA,
        "messages": messages,
        "response_format": services.config.JSON_MODE,
        "max_tokens": max(256, services.config.MIN_TOKENS_IA),
        "temperature": 0.7,
    }

    try:
        r = await services.post_json_ia(payload)
        content = r["choices"][0]["message"]["content"]
        salida = json.loads(content)
        titulo = str(salida.get("title", "")).strip()
        if not titulo:
            titulo = primer_mensaje[:30]
    except Exception as exc:
        print(f"[TITULO] fallo, uso fallback: {exc}")
        titulo = primer_mensaje[:30]

    return {"title": titulo}


@router.get("/api/status")
async def api_status(
    services: BackendContext = Depends(get_backend_context),
) -> dict[str, dict[str, Any]]:
    manager = services.serial_manager()
    return {
        "microbit": {
            "conectado": manager.conectado,
            "respondiendo": manager.respondiendo,
            "puerto": manager.puerto_actual,
            "ultimo_comando": manager.ultimo_comando,
            "ultimo_ack": manager.ultimo_ack,
            "leds": manager.patron_leds,
            "ble_relay": manager.relay_vivo(),
            "ble_error": manager.ble_error,
        }
    }


@router.get("/api/rag/status")
async def api_rag_status(
    services: BackendContext = Depends(get_backend_context),
) -> dict[str, Any]:
    return {"rag": services.rag_stats()}
