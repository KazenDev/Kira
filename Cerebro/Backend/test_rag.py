"""Tests deterministas del índice RAG y del ciclo de vida de recuerdos."""

from __future__ import annotations

import asyncio
import json
import os
import tempfile
from datetime import datetime, timedelta
from pathlib import Path

os.environ["KIRA_SIN_SERIAL"] = "1"
os.environ["KIRA_RAG_PROVIDER"] = "hash"
os.environ["KIRA_RAG_ALLOW_DOWNLOAD"] = "0"

import kira_server as ks  # noqa: E402
from app.intelligence.embeddings import HashEmbeddingProvider, NullEmbeddingProvider  # noqa: E402
from app.intelligence.rag_store import RagStore  # noqa: E402

FALLOS: list[str] = []


def check(condition: bool, name: str, detail: object = "") -> None:
    if condition:
        print(f"  [OK ] {name}")
    else:
        print(f"  [FALLA] {name} -> {detail}")
        FALLOS.append(name)


def test_store() -> None:
    print("== 1) FTS5 + vectores + RRF ==")
    with tempfile.TemporaryDirectory(prefix="kira_rag_store_") as tmp:
        store = RagStore(Path(tmp) / "index.sqlite3", HashEmbeddingProvider())
        records = [
            {"id": "a", "texto": "A Mateo le gustan las tomboys y la música", "importancia": 7},
            {"id": "b", "texto": "La temperatura está en 21 grados", "importancia": 2},
            {"id": "c", "texto": "La gusta hablar de fútbol", "importancia": 4},
        ]
        store.sync_memories("kira", records, embed=True)
        found = store.search("kira", "tomboys", candidates=records)
        check(found and found[0]["id"] == "a", "FTS/vector encuentra el recuerdo correcto", found)
        check("vector" in found[0].get("_rag_sources", []), "el resultado registra vector", found)
        stats = store.stats()
        check(stats["memories"] == 3 and stats["vectors"] == 3, "stats cuenta filas/vectores", stats)
        check(stats["fts"], "FTS5 está activo", stats)

        with tempfile.TemporaryDirectory(prefix="kira_rag_fallback_") as fallback_tmp:
            fallback = RagStore(Path(fallback_tmp) / "index.sqlite3", NullEmbeddingProvider())
            fallback.sync_memories("kira", records, embed=True)
            fallback_rows = fallback.search("kira", "tomboys", candidates=records)
            check(fallback_rows and fallback_rows[0]["id"] == "a", "sin embeddings sigue FTS5", fallback_rows)
            focused = fallback.search("kira", "qué de la temperatura", candidates=records)
            check([row["id"] for row in focused] == ["b"], "FTS ignora palabras vacías", focused)
            check(fallback.stats()["vectors"] == 0, "fallback no inventa vectores", fallback.stats())

        # El vector sólo se relaja cuando FTS ya aporta una coincidencia.
        store.vector_only_min_similarity = 1.1
        sin_fts = store.search("kira", "palabra_inexistente_xyz", candidates=records, limit=3)
        check(
            all("vector" not in row.get("_rag_sources", []) for row in sin_fts),
            "vector sin evidencia FTS queda bloqueado",
            sin_fts,
        )
        con_fts = store.search("kira", "tomboys", candidates=records, limit=3)
        check(
            any("vector" in row.get("_rag_sources", []) for row in con_fts),
            "FTS habilita el ranking vectorial relativo",
            con_fts,
        )
        store.vector_only_min_similarity = 0.84

        store.supersede("kira", "a", "new")
        after = store.search("kira", "tomboys", candidates=records)
        check(all(item["id"] != "a" for item in after), "supersede excluye la versión vieja", after)
        store.deactivate("kira", "b")
        check(store.stats()["active"] == 1, "deactivate marca inactivo", store.stats())


def test_memory_lifecycle() -> None:
    print("\n== 2) JSONL append-only + supersede/tombstone ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    with tempfile.TemporaryDirectory(prefix="kira_rag_memory_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", HashEmbeddingProvider())
            first = ks.memoria_agregar("kira", "Mateo prefiere las tomboys", "observacion", 7)
            duplicate = ks.memoria_agregar(
                "kira",
                "  Mateo prefiere las tomboys  ",
                "observacion",
                7,
                deduplicate=True,
            )
            check(duplicate["id"] == first["id"], "deduplicación exacta no crea otra línea", duplicate)
            second = ks.memoria_actualizar(
                "kira", first["id"], "Mateo prefiere la música y las tomboys", 8, "corrección"
            )
            check(second is not None, "actualizar crea una versión nueva", second)
            active = ks.memoria_cargar("kira")
            check(len(active) == 1 and active[0]["id"] == second["id"], "la versión vieja queda fuera", active)
            raw_lines = Path(tmp, "kira.jsonl").read_text(encoding="utf-8").splitlines()
            check(len(raw_lines) == 2, "el JSONL conserva ambas líneas", raw_lines)
            tombstone = ks.memoria_olvidar("kira", second["id"], "el usuario pidió olvidarlo")
            check(tombstone is not None, "olvidar crea tombstone", tombstone)
            check(ks.memoria_cargar("kira") == [], "el recuerdo queda soft-deleted", ks.memoria_cargar("kira"))
            check(len(Path(tmp, "kira.jsonl").read_text(encoding="utf-8").splitlines()) == 3, "no se borra historia", "lines")
        finally:
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag


async def test_chat_retrieval() -> None:
    print("\n== 3) RAG integrado en memoria del chat ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    old_texto = ks.texto_ia
    old_rag_enabled = ks.config.RAG_ENABLED
    with tempfile.TemporaryDirectory(prefix="kira_rag_chat_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", HashEmbeddingProvider())
            ks.config.RAG_ENABLED = False
            records = [
                {
                    "id": "old",
                    "fecha": (datetime.now() - timedelta(hours=2)).isoformat(timespec="seconds"),
                    "expira": (datetime.now() + timedelta(days=10)).isoformat(timespec="seconds"),
                    "tipo": "observacion",
                    "texto": "Mateo me contó que le gustan las tomboys",
                    "importancia": 7,
                },
                {
                    "id": "other",
                    "fecha": (datetime.now() - timedelta(hours=2)).isoformat(timespec="seconds"),
                    "expira": (datetime.now() + timedelta(days=10)).isoformat(timespec="seconds"),
                    "tipo": "observacion",
                    "texto": "La temperatura del micro:bit es 21 grados",
                    "importancia": 2,
                },
            ]
            Path(tmp, "kira.jsonl").write_text(
                "\n".join(json.dumps(record, ensure_ascii=False) for record in records) + "\n",
                encoding="utf-8",
            )

            async def resumen_falso(*args, **kwargs):
                return "Kira recuerda los tomboys y la marca fresca de otra conversación."

            ks.texto_ia = resumen_falso
            block = await ks.memoria_recuerdos_bloque("kira", "Kira", "qué le gusta a Mateo", [])
            check("tomboys" in block.lower(), "el bloque RAG trae el recuerdo relevante", block)
            check("temperatura" not in block.lower(), "no mete el recuerdo irrelevante", block)

            fresh = {
                "id": "fresh-other-chat",
                "fecha": datetime.now().isoformat(timespec="seconds"),
                "expira": (datetime.now() + timedelta(days=10)).isoformat(timespec="seconds"),
                "tipo": "observacion",
                "texto": "marca fresca de otra conversación",
                "importancia": 6,
            }
            with Path(tmp, "kira.jsonl").open("a", encoding="utf-8") as handle:
                handle.write(json.dumps(fresh, ensure_ascii=False) + "\n")
            active_block = await ks.memoria_recuerdos_bloque(
                "kira", "Kira", "marca fresca", []
            )
            check("marca fresca" not in active_block.lower(), "una charla activa no duplica recuerdos frescos", active_block)
            new_chat_block = await ks.memoria_recuerdos_bloque(
                "kira", "Kira", "marca fresca", [], incluir_recientes=True
            )
            check("marca fresca" in new_chat_block.lower(), "un chat nuevo sí recupera lo reciente de otra sesión", new_chat_block)
            no_evidence = await ks.memoria_recuerdos_bloque(
                "kira", "Kira", "capital de Francia", [], incluir_recientes=True
            )
            check(no_evidence == "", "sin evidencia no inyecta recuerdos arbitrarios", no_evidence)
        finally:
            await ks.background_tasks.cancel_all()
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag
            ks.texto_ia = old_texto
            ks.config.RAG_ENABLED = old_rag_enabled


async def test_rag_summary_failure_keeps_local_results() -> None:
    print("\n== 4) RAG local sobrevive a un resumen IA caído ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    old_texto = ks.texto_ia
    old_rag_enabled = ks.config.RAG_ENABLED
    with tempfile.TemporaryDirectory(prefix="kira_rag_summary_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", HashEmbeddingProvider())
            ks.config.RAG_ENABLED = False
            old = datetime.now() - timedelta(hours=2)
            records = [
                {
                    "id": f"summary-{index}",
                    "fecha": old.isoformat(timespec="seconds"),
                    "expira": (old + timedelta(days=10)).isoformat(timespec="seconds"),
                    "tipo": "observacion",
                    "texto": f"Recuerdo {index}: tomboys, música y steal",
                    "importancia": 7,
                }
                for index in range(3)
            ]
            Path(tmp, "kira.jsonl").write_text(
                "\n".join(json.dumps(record, ensure_ascii=False) for record in records) + "\n",
                encoding="utf-8",
            )

            async def resumen_caido(*args, **kwargs):
                raise RuntimeError("proveedor de resumen no disponible")

            ks.texto_ia = resumen_caido
            block = await ks.memoria_recuerdos_bloque(
                "kira", "Kira", "recordame los tomboys", []
            )
            check("tomboys" in block.lower(), "conserva resultados locales aunque falle el resumen", block)
            check("- Recuerdo" in block, "inyecta la lista cruda como fallback", block)
        finally:
            await ks.background_tasks.cancel_all()
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag
            ks.texto_ia = old_texto
            ks.config.RAG_ENABLED = old_rag_enabled


async def test_llm_memory_extraction() -> None:
    print("\n== 5) memoria semántica, no regex determinista ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    old_post = ks.post_json_ia
    with tempfile.TemporaryDirectory(prefix="kira_llm_memory_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", HashEmbeddingProvider())

            async def fake_post(payload):
                return {
                    "choices": [{
                        "message": {
                            "content": json.dumps({
                                "facts": [{
                                    "text": "El usuario se llama Eulises",
                                    "tipo": "identidad",
                                    "campo": "nombre",
                                    "key": "persona:nombre:eulises",
                                    "importance": 10,
                                    "confidence": 0.98,
                                    "evidence": "me llamo Eulises",
                                }]
                            })
                        }
                    }]
                }

            ks.post_json_ia = fake_post
            rows = await ks.memoria_extraer_factos(
                "kira",
                "Kira",
                [{"rol": "user", "contenido": "Hola, me llamo Eulises"}],
            )
            check(len(rows) == 1, "el LLM extrae un nombre cuando está seguro", rows)
            check("Eulises" in ks.memoria_identidad_bloque("kira"), "el nombre queda en contexto prioritario")

            async def uncertain_post(payload):
                return {
                    "choices": [{
                        "message": {
                            "content": json.dumps({
                                "facts": [{
                                    "text": "El usuario se llama Inventado",
                                    "tipo": "identidad",
                                    "campo": "nombre",
                                    "key": "persona:nombre:eulises",
                                    "confidence": 0.4,
                                    "evidence": "no existe",
                                }]
                            })
                        }
                    }]
                }

            ks.post_json_ia = uncertain_post
            before = len(ks.memoria_cargar("kira"))
            await ks.memoria_extraer_factos(
                "kira",
                "Kira",
                [{"rol": "user", "contenido": "No quiero que inventes nada"}],
            )
            check(len(ks.memoria_cargar("kira")) == before, "no guarda hechos inciertos", before)
        finally:
            await ks.background_tasks.cancel_all()
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag
            ks.post_json_ia = old_post


def main() -> int:
    test_store()
    test_memory_lifecycle()
    asyncio.run(test_chat_retrieval())
    asyncio.run(test_rag_summary_failure_keeps_local_results())
    asyncio.run(test_llm_memory_extraction())
    print()
    if FALLOS:
        print(f"FALLARON {len(FALLOS)} chequeo(s):")
        for failure in FALLOS:
            print(f"  - {failure}")
        return 1
    print("RAG OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
