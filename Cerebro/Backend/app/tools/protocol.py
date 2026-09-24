"""Pure normalization and deduplication keys for model-requested tools."""

from __future__ import annotations

import json
from collections.abc import Mapping
from typing import Any


def extraer_tools(tool: Any) -> list[dict]:
    if tool is None or tool is False or tool == "":
        return []
    requested: list[dict] = []
    items = tool if isinstance(tool, list) else [tool]
    for item in items:
        if isinstance(item, str):
            item = {"nombre": item}
        if isinstance(item, dict):
            name = str(item.get("nombre") or item.get("name") or "").strip()
            if name:
                normalized = dict(item)
                normalized["nombre"] = name
                requested.append(normalized)
    return requested


def clave_tool(pedido: dict) -> str:
    args = {key: value for key, value in pedido.items() if key != "nombre"}
    return json.dumps([pedido["nombre"], args], sort_keys=True, ensure_ascii=False)


def catalogo_herramientas(herramientas: Mapping[str, Mapping[str, Any]]) -> str:
    lines = [f"- {name}: {item['descripcion']}" for name, item in herramientas.items()]
    return (
        "\n\n# Catálogo de herramientas (autoridad: esto manda sobre como se usan)\n"
        "Pedilas en el campo \"tool\": un objeto, o una LISTA de objetos si son varias "
        "independientes (se ejecutan todas juntas y volves con todos los resultados).\n"
        "Reglas:\n"
        "- Usá una sola cuando NO sepas el dato, cuando tengas que VERIFICAR algo, o cuando "
        "haga falta ACCIONAR el mundo real (medir, buscar, calcular, elegir, guardar, tocar la placa).\n"
        "- NO la uses si ya sabes la respuesta de tu conocimiento general o si el dato no cambió: "
        "por costumbre no se llaman. Pocas y bien elegidas > muchas.\n"
        "- Si una herramienta ya te dio resultado en esta misma charla, no la repitas igual.\n"
        "Catálogo:\n" + "\n".join(lines)
    )
