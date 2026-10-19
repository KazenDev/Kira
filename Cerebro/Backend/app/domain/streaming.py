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


# Muletillas de asistente que el modelo suelta igual aunque el prompt las
# prohiba (medido con eval_charla.py: "buena esa" en 1 de cada 3 respuestas a
# una pregunta). Se borran del texto ACUMULADO, asi pantalla, voz e historial
# ven lo mismo.
_MULETILLA = re.compile(
    r"(^|[.!?…]\s+|,\s+)¡?(?:qu[eé]\s+)?buena\s+(?:pregunta|esa)\b(?:,\s*parce)?[.,:!?…]*\s*",
    re.I,
)


# "che" es argentino: el voseo del prompt a veces arrastra al modelo para alla
_CHE = re.compile(r",?\s*\bche\b(?!\s+guevara)(?=[\s,.!?…]|$),?", re.I)


def quitar_muletillas(texto: str) -> str:
    if texto and _CHE.search(texto):
        texto = _CHE.sub("", texto).lstrip()
        texto = texto[:1].upper() + texto[1:] if texto[:1].islower() else texto

    def _cambio(m: re.Match) -> str:
        return m.group(1) + "\0"   # marca: lo que sigue va con mayuscula/minuscula

    limpio = _MULETILLA.sub(_cambio, texto or "")
    if "\0" not in limpio:
        return texto

    partes = limpio.split("\0")
    out = partes[0]
    for resto in partes[1:]:
        if resto:
            despues_de_coma = out.rstrip().endswith(",")
            resto = (resto[0].lower() if despues_de_coma else resto[0].upper()) + resto[1:]
        out += resto
    out = out.strip()
    # "Ah, " + nada (la muletilla era todo lo que habia hasta ahora)
    return out[:-1] if out.endswith(",") else out


def como_objeto(salida):
    """La salida de la IA tiene que ser UN objeto {emotion, message, tool}.
    A veces el modelo devuelve una LISTA con el objeto adentro (medido: con el
    ejemplo de varias herramientas en el prompt) y el backend se caia a mitad
    de la respuesta con 'list' object has no attribute 'get'."""
    if isinstance(salida, dict):
        return salida
    if isinstance(salida, list):
        for item in salida:
            if isinstance(item, dict) and ("message" in item or "emotion" in item or "tool" in item):
                return item
    return {}


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
