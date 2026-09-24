"""Prepara y verifica el modelo local de embeddings de Kira.

No toca memorias ni conversaciones; sólo verifica que FastEmbed pueda cargar
el modelo en la cache local.

    .venv/bin/python prepare_rag_model.py
"""

from __future__ import annotations

import config
from app.intelligence.embeddings import make_embedding_provider


def main() -> int:
    provider = make_embedding_provider(
        provider_name=config.RAG_PROVIDER,
        model_name=config.RAG_MODEL,
        cache_dir=config.RAG_CACHE_DIR,
        model_file=config.RAG_MODEL_FILE,
        allow_download=config.RAG_ALLOW_DOWNLOAD,
        batch_size=config.RAG_BATCH_SIZE,
        threads=config.RAG_THREADS,
    )
    print(f"provider={provider.name}")
    print(f"cache={getattr(provider, 'cache_dir', 'test')}")
    if not provider.available():
        print("El paquete fastembed no está instalado.")
        return 1
    if not provider.warmup():
        return 1
    vector = provider.embed_query("Kira está lista para recordar")
    if vector is None:
        print("El modelo se cargó pero no devolvió un embedding.")
        return 1
    print(f"dimension={vector.shape[0]}")
    print("MODELO RAG LISTO")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
