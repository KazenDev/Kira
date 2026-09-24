"""Pruebas deterministas de memoria semántica y contexto de identidad.

No decide nombres con regex: el LLM devuelve hechos con confianza y evidencia;
el backend sólo valida el formato y que la evidencia sea del mensaje del usuario.
"""

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
from app.intelligence.embeddings import HashEmbeddingProvider, NullEmbeddingProvider  # noqa: E402
from app.intelligence.rag_store import RagStore  # noqa: E402

FALLOS: list[str] = []


def check(condition: bool, name: str, detail: object = "") -> None:
    if condition:
        print(f"  [OK ] {name}")
    else:
        print(f"  [FALLA] {name} -> {detail}")
        FALLOS.append(name)


def _respuesta_contenido(contenido: str) -> dict:
    return {"choices": [{"message": {"content": contenido}}]}


async def test_extraccion_semantica() -> None:
    print("== 1) extracción semántica con evidencia ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    old_post = ks.post_json_ia
    with tempfile.TemporaryDirectory(prefix="kira_identity_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", HashEmbeddingProvider())
            prompts: list[dict] = []

            async def fake_post(payload: dict) -> dict:
                prompts.append(payload)
                if "Ana" in payload["messages"][1]["content"]:
                    return _respuesta_contenido(json.dumps({
                        "facts": [{
                            "text": "El usuario se llama Ana",
                            "tipo": "identidad",
                            "campo": "nombre",
                            "key": "persona:nombre:eulises",
                            "importance": 10,
                            "confidence": 0.97,
                            "evidence": "ahora me llamo Ana",
                        }]
                    }))
                return _respuesta_contenido(json.dumps({
                    "facts": [{
                        "text": "El usuario se llama Eulises",
                        "tipo": "identidad",
                            "campo": "nombre",
                            "key": "persona:nombre:eulises",
                        "importance": 10,
                        "confidence": 0.98,
                        "evidence": "me llamo Eulises",
                    }]
                }))

            ks.post_json_ia = fake_post
            rows = await ks.memoria_extraer_factos(
                "kira",
                "Kira",
                [{"rol": "user", "contenido": "Hola, me llamo Eulises"}],
            )
            check(len(rows) == 1, "guarda un nombre sólo cuando la IA lo extrae", rows)
            identity_block = ks.memoria_identidad_bloque("kira")
            check("Eulises" in identity_block, "el nombre entra al contexto prioritario")
            check(identity_block.startswith("\n\n# IDENTIDAD"), "el bloque de identidad es una sección de prompt válida", identity_block[:40])
            check(prompts and "\n" in prompts[0]["messages"][1]["content"], "la transcripción usa líneas reales")

            # La evidencia tiene que pertenecer al usuario, no a una afirmación
            # de Kira ni a una frase inventada por el extractor.
            async def assistant_only_post(payload: dict) -> dict:
                return _respuesta_contenido(json.dumps({
                    "facts": [{
                        "text": "El usuario se llama Inventado",
                        "tipo": "identidad",
                            "campo": "nombre",
                            "key": "persona:nombre:eulises",
                        "importance": 10,
                        "confidence": 0.99,
                        "evidence": "El usuario se llama Inventado",
                    }]
                }))

            ks.post_json_ia = assistant_only_post
            before = len(ks.memoria_cargar("otro"))
            await ks.memoria_extraer_factos(
                "otro",
                "Kira",
                [
                    {"rol": "user", "contenido": "Mirá esta foto"},
                    {"rol": "assistant", "contenido": "El usuario se llama Inventado"},
                ],
            )
            check(len(ks.memoria_cargar("otro")) == before, "rechaza evidencia que sólo aparece en Kira")

            async def reasoning_post(payload: dict) -> dict:
                return {"choices": [{"message": {
                    "content": "",
                    "reasoning_content": json.dumps({
                        "facts": [{
                            "text": "El usuario prefiere el té",
                            "tipo": "preferencia",
                            "importance": 4,
                            "confidence": 0.9,
                            "evidence": "me gusta el té",
                        }]
                    }),
                }}]}

            ks.post_json_ia = reasoning_post
            rows = await ks.memoria_extraer_factos(
                "razon",
                "Kira",
                [{"rol": "user", "contenido": "Me gusta el té"}],
            )
            check(len(rows) == 1, "acepta contenido JSON del proveedor en reasoning_content", rows)

            async def date_post(payload: dict) -> dict:
                return _respuesta_contenido(json.dumps({
                    "facts": [{
                        "text": "Fecha de nacimiento: 2011/03/28",
                        "tipo": "identidad",
                        "campo": "fecha_nacimiento",
                        "key": "persona:nacimiento:2011-03-28",
                        "importance": 9,
                        "confidence": 0.99,
                        "evidence": "mi fecha de nacimiento es 2011/03/28",
                    }]
                }))

            ks.post_json_ia = date_post
            date_rows = await ks.memoria_extraer_factos(
                "fecha",
                "Kira",
                [{"rol": "user", "contenido": "Mi fecha de nacimiento es 2011/03/28"}],
            )
            check(
                date_rows and date_rows[0].get("tipo") == "dato_personal",
                "una fecha no se clasifica como identidad",
                date_rows,
            )
            check(
                "2011" not in ks.memoria_identidad_bloque("fecha"),
                "una fecha no entra al bloque de nombre",
            )

            # Un cambio explícito crea una versión nueva y deja vigente la última.
            ks.post_json_ia = fake_post
            await ks.memoria_extraer_factos(
                "kira",
                "Kira",
                [{"rol": "user", "contenido": "Ahora me llamo Ana"}],
            )
            actual = ks.memoria_identidad_actual("kira")
            check(actual is not None and "Ana" in actual.get("texto", ""), "un cambio de nombre supersede el anterior", actual)
            check(len(ks.memoria_cargar("kira")) == 1, "sólo queda vigente el nombre más reciente")

            ks.memoria_agregar(
                "manual",
                "El usuario se llama NoSemantico",
                "identidad",
                metadata={"identity": True},
            )
            check(
                ks.memoria_identidad_actual("manual") is None,
                "una marca manual no se presenta como identidad aprendida",
            )
        finally:
            await ks.background_tasks.cancel_all()
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag
            ks.post_json_ia = old_post


class _SerialFalso:
    def enviar(self, _comando: str) -> None:
        pass


async def test_captura_post_turno_normal() -> None:
    print("\n== 2) captura post-turno y chat siguiente ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    old_post = ks.post_json_ia
    old_serial = ks.serial_mgr
    old_guardar_turno = ks.guardar_turno
    with tempfile.TemporaryDirectory(prefix="kira_identity_capture_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", NullEmbeddingProvider())
            ks.serial_mgr = _SerialFalso()
            ks.guardar_turno = lambda *args, **kwargs: None
            chat_payloads: list[dict] = []

            async def post_falso(payload: dict) -> dict:
                system = str(payload.get("messages", [{}])[0].get("content", ""))
                if "extractor de memoria" in system:
                    return _respuesta_contenido(json.dumps({
                        "facts": [{
                            "text": "El usuario se llama Eulises",
                            "tipo": "identidad",
                            "campo": "nombre",
                            "key": "persona:nombre:eulises",
                            "importance": 10,
                            "confidence": 0.99,
                            "evidence": "me llamo Eulises",
                        }]
                    }))
                if "cuaderno de experiencias" in system:
                    return _respuesta_contenido("Kira recordará esta charla.")
                if "califica la importancia" in system:
                    return _respuesta_contenido("10")
                chat_payloads.append(payload)
                return _respuesta_contenido(json.dumps({
                    "emotion": "neutral",
                    "message": "Te llamás Eulises.",
                }))

            ks.post_json_ia = post_falso
            first = await ks.api_chat({
                "personaje": "kira",
                "mensaje": "Hola, me llamo Eulises",
                "historia": [],
            })
            check(first.get("message") == "Te llamás Eulises.", "el primer chat responde normalmente", first)
            pending = list(ks.background_tasks._tasks)
            if pending:
                await asyncio.gather(*pending)
            check(
                ks.memoria_identidad_actual("kira") is not None,
                "el extractor post-turno guarda la identidad",
            )

            await ks.api_chat({
                "personaje": "kira",
                "mensaje": "¿Cómo me llamo?",
                "historia": [],
            })
            check(
                len(chat_payloads) >= 2 and "Eulises" in chat_payloads[1]["messages"][0]["content"],
                "el siguiente chat recupera la identidad sin historial",
                chat_payloads[1]["messages"][0]["content"] if len(chat_payloads) > 1 else "",
            )
        finally:
            await ks.background_tasks.cancel_all()
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag
            ks.post_json_ia = old_post
            ks.serial_mgr = old_serial
            ks.guardar_turno = old_guardar_turno


async def test_contexto_en_chat_normal_y_stream() -> None:
    print("\n== 3) el nombre extraído aparece en ambos endpoints ==")
    old_memory_dir = ks.MEMORIA_DIR
    old_rag = ks.rag_store
    old_post = ks.post_json_ia
    old_stream = ks._stream_ia
    old_serial = ks.serial_mgr
    old_guardar_turno = ks.guardar_turno
    old_memo = ks.memoria_memo_post_charla
    with tempfile.TemporaryDirectory(prefix="kira_identity_endpoints_") as tmp:
        try:
            ks.MEMORIA_DIR = tmp
            ks.rag_store = RagStore(Path(tmp) / "rag" / "index.sqlite3", HashEmbeddingProvider())
            ks.serial_mgr = _SerialFalso()
            ks.guardar_turno = lambda *args, **kwargs: None
            ks.memoria_agregar(
                "kira",
                "El usuario se llama Eulises",
                "identidad",
                10,
                metadata={
                    "identity": True,
                    "source": "llm_memory_extraction",
                    "field": "nombre",
                },
            )

            normal_payloads: list[dict] = []

            async def post_falso(payload: dict) -> dict:
                normal_payloads.append(payload)
                return _respuesta_contenido(json.dumps({
                    "emotion": "neutral",
                    "message": "Te llamás Eulises.",
                }))

            async def memo_falso(*args, **kwargs):
                return None

            ks.post_json_ia = post_falso
            ks.memoria_memo_post_charla = memo_falso
            respuesta = await ks.api_chat({
                "personaje": "kira",
                "mensaje": "¿Cómo me llamo?",
                "historia": [],
            })
            check(respuesta.get("message") == "Te llamás Eulises.", "/api/chat conserva la respuesta", respuesta)
            check(
                bool(normal_payloads) and "Eulises" in normal_payloads[0]["messages"][0]["content"],
                "/api/chat incluye la identidad recordada",
                normal_payloads[0]["messages"][0]["content"] if normal_payloads else "",
            )

            seen: list[list[dict]] = []

            async def stream_falso(messages: list[dict], caja: dict, _temperatura: float):
                seen.append(messages)
                contenido = json.dumps({"emotion": "neutral", "message": "Te llamás Eulises."})
                caja["buffer"] = contenido
                caja["salida"] = json.loads(contenido)
                yield ks.sse_event({"tipo": "delta", "texto": "Te llamás Eulises."})

            ks._stream_ia = stream_falso
            streaming = await ks.api_chat_stream({
                "personaje": "kira",
                "mensaje": "¿Cómo me llamo?",
                "historia": [],
            })
            async for _event in streaming.body_iterator:
                pass
            check(bool(seen), "/api/chat/stream ejecuta el stream falso", seen)
            check(
                bool(seen) and "Eulises" in seen[0][0]["content"],
                "/api/chat/stream incluye la identidad recordada",
                seen[0][0]["content"] if seen else "",
            )
        finally:
            await ks.background_tasks.cancel_all()
            ks.MEMORIA_DIR = old_memory_dir
            ks.rag_store = old_rag
            ks.post_json_ia = old_post
            ks._stream_ia = old_stream
            ks.serial_mgr = old_serial
            ks.guardar_turno = old_guardar_turno
            ks.memoria_memo_post_charla = old_memo


def main() -> int:
    asyncio.run(test_extraccion_semantica())
    asyncio.run(test_captura_post_turno_normal())
    asyncio.run(test_contexto_en_chat_normal_y_stream())
    print()
    if FALLOS:
        print(f"FALLARON {len(FALLOS)} chequeo(s):")
        for failure in FALLOS:
            print(f"  - {failure}")
        return 1
    print("MEMORIA DE IDENTIDAD OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
