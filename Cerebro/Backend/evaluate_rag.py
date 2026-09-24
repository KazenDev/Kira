"""Evalúa recuperación RAG contra consultas golden.

El archivo JSON tiene esta forma:

[
  {"query": "qué le gusta a Mateo", "expected_ids": ["abc123"]},
  {"query": "qué temperatura hace", "expected_text": ["21 grados"]}
]

Uso:

    .venv/bin/python evaluate_rag.py golden.json
    .venv/bin/python evaluate_rag.py golden.json --no-embeddings
"""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path

os.environ.setdefault("KIRA_SIN_SERIAL", "1")
os.environ.setdefault(
    "KIRA_RAG_DB",
    os.path.join(os.path.dirname(__file__), "memoria", "rag", "index.sqlite3"),
)

import kira_server as ks  # noqa: E402


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("golden", type=Path)
    parser.add_argument("--character", default="kira")
    parser.add_argument("--k", type=int, default=5)
    parser.add_argument(
        "--no-embeddings",
        action="store_true",
        help="evalúa sólo FTS5; por defecto sincroniza embeddings reales",
    )
    args = parser.parse_args()
    cases = json.loads(args.golden.read_text(encoding="utf-8"))
    candidates = ks.memoria_cargar(args.character)
    ks.rag_store.sync_memories(
        args.character,
        candidates,
        embed=not args.no_embeddings,
    )
    hits = 0
    reciprocal_rank = 0.0
    for case in cases:
        query = str(case.get("query", ""))
        expected_ids = {str(value) for value in case.get("expected_ids", [])}
        expected_text = [str(value).lower() for value in case.get("expected_text", [])]
        results = ks.rag_store.search(
            args.character,
            query,
            candidates=candidates,
            limit=args.k,
        )
        rank = None
        for position, result in enumerate(results, 1):
            text = str(result.get("texto", "")).lower()
            if str(result.get("id")) in expected_ids or any(item in text for item in expected_text):
                rank = position
                break
        if rank is not None:
            hits += 1
            reciprocal_rank += 1.0 / rank
        print(f"{'OK' if rank else 'MISS'} rank={rank or '-'} query={query!r}")
    total = max(1, len(cases))
    print(f"recall@{args.k}={hits / total:.3f}")
    print(f"mrr@{args.k}={reciprocal_rank / total:.3f}")
    return 0 if hits == len(cases) else 1


if __name__ == "__main__":
    raise SystemExit(main())
