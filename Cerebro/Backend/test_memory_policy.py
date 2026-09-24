"""Pruebas de admisión de memoria: hechos, experiencias y basura."""

from __future__ import annotations

import asyncio
import json
import os
import tempfile
from pathlib import Path

os.environ["KIRA_SIN_SERIAL"] = "1"
os.environ["KIRA_RAG_PROVIDER"] = "hash"
os.environ["KIRA_RAG_ALLOW_DOWNLOAD"] = "0"

import kira_server as ks  # noqa: E402
from app.intelligence.embeddings import HashEmbeddingProvider  # noqa: E402
from app.intelligence.rag_store import RagStore  # noqa: E402

FALLOS: list[str] = []


def check(condition: bool, name: str, detail: object = "") -> None:
    if condition:
        print(f"  [OK ] {name}")
    else:
        print(f"  [FALLA] {name} -> {detail}")
        FALLOS.append(name)


async def test_admission_policy() -> None:
    print("== 1) no cada frase es memoria ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    old_texto = ks.texto_ia
    old_extract = ks.memoria_extraer_factos
    old_rag_enabled = ks.config.RAG_ENABLED
    state = {"memo": "Kira recordará esta charla.", "importance": "4"}
    with tempfile.TemporaryDirectory(prefix="kira_memory_policy_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", HashEmbeddingProvider())
            ks.config.RAG_ENABLED = False

            async def no_extract(*args, **kwargs):
                return []

            async def texto_falso(sistema: str, usuario: str, *args, **kwargs):
                if "cuaderno de experiencias" in sistema:
                    return state["memo"]
                if "califica la importancia" in sistema:
                    return state["importance"]
                return "NADA"

            ks.memoria_extraer_factos = no_extract
            ks.texto_ia = texto_falso
            turnos = [
                {"rol": "user", "contenido": "dame un código"},
                {"rol": "assistant", "contenido": "sí, aquí va"},
            ]

            await ks.memoria_memo_post_charla("kira", "Kira", turnos)
            check(ks.memoria_cargar("kira") == [], "un memo de importancia baja no se guarda")

            state["memo"] = "Kira conservó algo importante para esta conversación."
            state["importance"] = "4"
            await ks.memoria_memo_post_charla("kira", "Kira", turnos)
            check(ks.memoria_cargar("kira") == [], "NADA / memo trivial se descarta")

            state["memo"] = "Hoy el usuario compartí algo que cambió nuestra forma de conversar."
            state["importance"] = "8"
            await ks.memoria_memo_post_charla("kira", "Kira", turnos)
            rows = ks.memoria_cargar("kira")
            check(len(rows) == 1, "un memo significativo sí se guarda", rows)
            check(
                bool(rows) and rows[0].get("memory_class") == "episodic",
                "el memo se separa como experiencia",
                rows,
            )
        finally:
            await ks.background_tasks.cancel_all()
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag
            ks.texto_ia = old_texto
            ks.memoria_extraer_factos = old_extract
            ks.config.RAG_ENABLED = old_rag_enabled


async def test_borrado_y_tombstones() -> None:
    print("== 2) borrar desde Ajustes conserva el rastro ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    with tempfile.TemporaryDirectory(prefix="kira_memory_clear_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", HashEmbeddingProvider())
            ks.rag_store.ensure_schema()
            ks.memoria_agregar("kira", "recuerdo para borrar", "observacion", 7)
            ks.memoria_agregar("kira", "otro recuerdo", "observacion", 6)
            count = ks.memoria_borrar_todas("kira", "prueba de Ajustes")
            check(count == 2, "el borrado cuenta cada recuerdo activo", count)
            check(ks.memoria_cargar("kira") == [], "no quedan recuerdos activos")
            raw_path = Path(tmp) / "kira.jsonl"
            historico = [
                json.loads(line)
                for line in raw_path.read_text(encoding="utf-8").splitlines()
                if line.strip()
            ]
            check(
                len(historico) == 4
                and sum(record.get("tipo") == "olvido" for record in historico) == 2,
                "el JSONL conserva dos originales y dos tombstones",
                historico,
            )
            check(ks.rag_store.stats()["active"] == 0, "el índice derivado queda sin activos")
        finally:
            await ks.background_tasks.cancel_all()
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag


def main() -> int:
    asyncio.run(test_admission_policy())
    asyncio.run(test_borrado_y_tombstones())
    print()
    if FALLOS:
        print(f"FALLARON {len(FALLOS)} chequeo(s):")
        for failure in FALLOS:
            print(f"  - {failure}")
        return 1
    print("POLÍTICA DE MEMORIA OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
