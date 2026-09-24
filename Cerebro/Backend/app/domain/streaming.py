"""Pure helpers for provider JSON, normalized emotion and SSE framing."""

from __future__ import annotations

import json
import re
from typing import Any

EMOCIONES_VALIDAS = (
    "happy",
    "sad",
    "angry",
    "surprised",
    "neutral",
    "fastidio",
    "miedo",
    "cansado",
)
SINONIMOS_EMOCION = {
    "feliz": "happy", "contenta": "happy", "contento": "happy", "alegre": "happy",
    "confident": "happy", "confiada": "happy", "confiado": "happy", "entusiasta": "happy",
    "entusiasmada": "happy", "excited": "surprised", "emocionada": "surprised",
    "triste": "sad", "tristeza": "sad", "melancolica": "sad",
    "enojada": "angry", "enojado": "angry", "furiosa": "angry", "furioso": "angry", "molesta": "angry",
    "sorprendida": "surprised", "sorprendido": "surprised", "surprise": "surprised", "asombrada": "surprised",
    "asustada": "miedo", "asustado": "miedo", "fear": "miedo", "ansiosa": "miedo", "nerviosa": "miedo",
    "molesto": "fastidio", "annoyed": "fastidio", "irritada": "fastidio", "fastidiada": "fastidio",
    "cansada": "cansado", "tired": "cansado", "sleepy": "cansado", "agotada": "cansado",
    "calm": "neutral", "calma": "neutral", "neutralidad": "neutral",
}
REGEX_MESSAGE = re.compile(r'"message"\s*:\s*"((?:[^"\\]|\\.)*)')


def normalizar_emocion(valor: Any) -> str:
    value = str(valor or "").strip().lower()
    if value in EMOCIONES_VALIDAS:
        return value
    return SINONIMOS_EMOCION.get(value, "neutral")


def limpiar_json_ia(texto: str) -> str:
    text = (texto or "").strip()
    if text.startswith("```"):
        text = re.sub(r"^```[A-Za-z]*\s*", "", text)
        text = re.sub(r"\s*```$", "", text).strip()
    start, end = text.find("{"), text.rfind("}")
    if start != -1 and end != -1 and (start > 0 or end < len(text) - 1):
        text = text[start : end + 1]
    return text.strip()


def extraer_message_parcial(buffer: str) -> str | None:
    match = REGEX_MESSAGE.search(buffer)
    if not match:
        return None
    try:
        return json.loads('"' + match.group(1) + '"')
    except Exception:
        return match.group(1)


def sse_event(datos: dict) -> str:
    return f"data: {json.dumps(datos, ensure_ascii=False)}\n\n"


def motivo_sin_salida(caja: dict) -> str:
    buffer = (caja.get("buffer") or "").strip()
    if not buffer:
        return "El cerebro no devolvió texto: la IA no respondió nada. Probá de nuevo."
    try:
        datos = json.loads(buffer)
    except Exception:
        return f"La IA devolvió una respuesta incompleta (JSON cortado): {buffer[:120]}"
    if isinstance(datos, dict) and datos.get("error"):
        error = datos["error"]
        mensaje = error.get("message") if isinstance(error, dict) else str(error)
        return f"La IA rechazó la llamada: {mensaje}"
    return f"La IA no devolvió el formato esperado: {buffer[:120]}"
