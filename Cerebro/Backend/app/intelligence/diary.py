"""Pure hard limits and guardrails for Kira's self-authored diary."""

from __future__ import annotations

from datetime import datetime
from typing import Any

DIARIO_MAX_YO_SOY = 400
DIARIO_MAX_OPINIONES = 20
DIARIO_MAX_GUSTOS = 10
DIARIO_MAX_PERSONAS = 10


def diario_aplicar(
    viejo: dict,
    nuevo: dict,
    ids_validos: set[str],
) -> tuple[dict[str, Any], list[str]]:
    rejections: list[str] = []
    yo_soy = str(nuevo.get("yo_soy", "")).strip()[:DIARIO_MAX_YO_SOY]
    opinions = []
    for opinion in nuevo.get("opiniones", [])[:DIARIO_MAX_OPINIONES]:
        if not isinstance(opinion, dict):
            continue
        text = str(opinion.get("texto", "")).strip()
        evidence = [item for item in opinion.get("evidencia", []) if item in ids_validos]
        if not text or len(text) < 10:
            rejections.append(f"opinion sin cuerpo: {text[:40]}")
            continue
        if not evidence:
            rejections.append(f"opinion sin evidencia valida: {text[:40]}")
            continue
        opinions.append(
            {
                "texto": text[:300],
                "evidencia": evidence[:6],
                "fecha": datetime.now().isoformat(timespec="seconds"),
            }
        )
    tastes = [str(item).strip()[:120] for item in nuevo.get("gustos", [])[:DIARIO_MAX_GUSTOS] if str(item).strip()]
    people = [str(item).strip()[:120] for item in nuevo.get("personas", [])[:DIARIO_MAX_PERSONAS] if str(item).strip()]

    had_content = bool(viejo.get("yo_soy") or viejo.get("opiniones"))
    has_content = bool(yo_soy or opinions)
    if had_content and not has_content:
        rejections.append("fusion vacia rechazada (anti-wipe)")
        return viejo, rejections

    return {
        "yo_soy": yo_soy,
        "opiniones": opinions,
        "gustos": tastes,
        "personas": people,
        "actualizado": datetime.now().isoformat(timespec="seconds"),
    }, rejections
