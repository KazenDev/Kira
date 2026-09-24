"""Pure sentence segmentation used by the streaming TTS scheduler."""

from __future__ import annotations

import re


def dividir_frases(texto: str, max_frases: int = 6) -> list[str]:
    text = (texto or "").strip()
    if not text:
        return []
    parts = [part.strip() for part in re.split(r"(?<=[.?!…])\s+", text) if part.strip()]
    phrases: list[str] = []
    current = ""
    for part in parts:
        current = (current + " " + part).strip() if current else part
        if len(current) >= 20:
            phrases.append(current)
            current = ""
    if current:
        phrases.append(current)
    if len(phrases) > max_frases:
        phrases = phrases[: max_frases - 1] + [" ".join(phrases[max_frases - 1 :])]
    return [phrase for phrase in phrases if phrase]


def _termina_en_punto(frase: str) -> bool:
    value = frase.rstrip("\"»”)' ").strip()
    return bool(value) and value[-1] in ".?!…"
