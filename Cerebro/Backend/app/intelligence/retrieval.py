"""Pure memory retrieval and evidence parsing policies."""

from __future__ import annotations

import re
import unicodedata

REFLEXION_TOP_N = 30
RECENCIA_DECAIMIENTO = 0.995
RECENCIA_PESO = 0.5
RELEVANCIA_PESO = 3.0
IMPORTANCIA_PESO = 2.0

_PALABRAS_VACIAS = {
    "para", "porque", "como", "pero", "cuando", "sobre", "desde", "hasta",
    "esto", "esta", "este", "ellos", "ellas", "tambien", "entonces", "mucho",
    "poco", "puedo", "puede", "tengo", "tiene", "hacer", "decir", "dijo",
    "dice", "cosa", "cosas", "mismo", "misma", "todos", "todas", "antes",
    "despues", "siempre", "nunca", "ahora", "alli", "donde", "quien",
}


def _normalizar(valores: dict) -> dict:
    if not valores:
        return {}
    minimum, maximum = min(valores.values()), max(valores.values())
    if maximum == minimum:
        return {key: 0.5 for key in valores}
    return {key: (value - minimum) / (maximum - minimum) for key, value in valores.items()}


def _tokens(texto: str) -> set[str]:
    plain = "".join(
        char for char in unicodedata.normalize("NFD", texto.lower())
        if unicodedata.category(char) != "Mn"
    )
    return {
        word for word in re.findall(r"[a-zñ]{4,}", plain)
        if word not in _PALABRAS_VACIAS
    }


def memoria_recuperar(
    recuerdos: list[dict],
    pregunta: str,
    top_n: int = REFLEXION_TOP_N,
) -> list[dict]:
    if not recuerdos:
        return []
    ordered = list(reversed(recuerdos))
    novelty = {record["id"]: RECENCIA_DECAIMIENTO ** (index + 1) for index, record in enumerate(ordered)}
    importance = {record["id"]: float(record.get("importancia", 5)) for record in recuerdos}
    question_tokens = _tokens(pregunta)
    relevance = {}
    for record in recuerdos:
        record_tokens = _tokens(record.get("texto", ""))
        if question_tokens and record_tokens:
            intersection = len(question_tokens & record_tokens)
            relevance[record["id"]] = intersection / len(question_tokens | record_tokens) if intersection else 0.0
        else:
            relevance[record["id"]] = 0.0

    novelty = _normalizar(novelty)
    importance = _normalizar(importance)
    relevance = _normalizar(relevance)
    scores = {
        record_id: (
            RECENCIA_PESO * novelty.get(record_id, 0.0)
            + RELEVANCIA_PESO * relevance.get(record_id, 0.0)
            + IMPORTANCIA_PESO * importance.get(record_id, 0.0)
        )
        for record_id in novelty
    }
    top = sorted(scores.items(), key=lambda item: item[1], reverse=True)[:top_n]
    selected = {record_id for record_id, _ in top}
    return [record for record in recuerdos if record["id"] in selected]


def memoria_extraer_pensamiento(linea: str, relevantes: list[dict]):
    clean = linea.strip().lstrip("-•*").strip()
    clean = re.sub(r"^\d+[\).\-]\s*", "", clean)
    if len(clean) < 15:
        return None
    match = re.match(
        r"^(.*?)\s*\(\s*(?:por\s+)?(?:los\s+|las\s+)?(?:recuerdos?|registros?|memorias?)?\s*([\d\s,]+)\s*\)\s*[.,;:! ]*\s*$",
        clean,
        re.I,
    )
    if not match:
        return None
    text = match.group(1).strip().rstrip(",;")
    ids = []
    for number in re.split(r"[,\s]+", match.group(2)):
        if number.isdigit():
            index = int(number) - 1
            if 0 <= index < len(relevantes):
                ids.append(relevantes[index]["id"])
    if not ids:
        return None
    return text, ids
