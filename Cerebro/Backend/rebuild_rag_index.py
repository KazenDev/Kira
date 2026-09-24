"""Reconstruye el índice RAG desde los JSONL de memoria.

La fuente de verdad no se modifica: sólo se regenera `memoria/rag/index.sqlite3`
(o el override `KIRA_RAG_DB`).

    .venv/bin/python rebuild_rag_index.py
    .venv/bin/python rebuild_rag_index.py --no-embeddings
"""

from __future__ import annotations

import argparse
import os

os.environ.setdefault("KIRA_SIN_SERIAL", "1")
os.environ.setdefault(
    "KIRA_RAG_DB",
    os.path.join(os.path.dirname(__file__), "memoria", "rag", "index.sqlite3"),
)

import config  # noqa: E402
import kira_server as ks  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--no-embeddings",
        action="store_true",
        help="reconstruye sólo FTS5, sin descargar/cargar el modelo local",
    )
    args = parser.parse_args()
    total = 0
    for character in config.PERSONAJES:
        records = ks.memoria_cargar(character)
        count = ks.rag_store.rebuild(
            character,
            records,
            embed=not args.no_embeddings,
        )
        total += count
        print(f"{character}: {count} recuerdos indexados")
    print(f"total={total}")
    print(ks.rag_store.stats())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
