"""Contrato de la app modular: rutas, shapes y separación de responsabilidades.

Ejecutar:
    .venv/bin/python test_app_contract.py

No llama proveedores externos, no abre serial y no toca memoria real.
"""

from __future__ import annotations

import asyncio
import os

os.environ["KIRA_SIN_SERIAL"] = "1"
os.environ.setdefault("KIRA_AUTH_REQUIRED", "0")

import httpx  # noqa: E402

import kira_server as ks  # noqa: E402
from app.dependencies import BackendContext  # noqa: E402
from app.domain.streaming import normalizar_emocion as domain_normalizar_emocion  # noqa: E402
from app.domain.tts import dividir_frases as domain_dividir_frases  # noqa: E402
from app.domain.vision import normalizar_foto as domain_normalizar_foto  # noqa: E402
from app.intelligence.diary import diario_aplicar as domain_diario_aplicar  # noqa: E402
from app.intelligence.retrieval import memoria_recuperar as domain_memoria_recuperar  # noqa: E402
from app.main import create_app  # noqa: E402
from app.tools.protocol import clave_tool as domain_clave_tool  # noqa: E402
from app.tools.protocol import extraer_tools as domain_extraer_tools  # noqa: E402

FALLOS: list[str] = []


def chequear(condicion: bool, descripcion: str, extra: object = "") -> None:
    if condicion:
        print(f"  [OK ] {descripcion}")
    else:
        print(f"  [FALLA] {descripcion} -> {extra}")
        FALLOS.append(descripcion)


class _ManagerFalso:
    conectado = False
    respondiendo = False
    puerto_actual = None
    ultimo_comando = ""
    ultimo_ack = None
    patron_leds = None
    ble_error = None
    enviados: list[str] = []
    registrados: list[str] = []
    arrancado = False
    cerrado = False

    def start(self) -> None:
        self.arrancado = True

    def close(self) -> None:
        self.cerrado = True

    def enviar(self, comando: str) -> None:
        self.enviados.append(comando)

    def relay_registrar(self, token: str) -> str:
        self.registrados.append(token)
        return "ok"

    def relay_liberar(self, token: str) -> None:
        self.registrados.remove(token)

    def relay_vivo(self) -> bool:
        return bool(self.registrados)


async def test_app() -> None:
    print("== 1) armazón y contrato de rutas ==")
    chequear(isinstance(ks.app.state.backend_context, BackendContext), "create_app adjunta BackendContext")
    chequear(callable(create_app), "la fábrica create_app está importable")

    esperado = {
        "/api/personajes": {"get"},
        "/api/archivos/{nombre}": {"get"},
        "/grabaciones/{nombre}": {"get"},
        "/api/auth/register": {"post"},
        "/api/auth/login": {"post"},
        "/api/auth/me": {"get"},
        "/api/auth/logout": {"post"},
        "/api/memoria/{personaje}": {"get"},
        "/api/memoria/{personaje}/olvidar": {"post"},
        "/api/memoria/{personaje}/borrar-todo": {"post"},
        "/api/conversaciones/{personaje}": {"get"},
        "/api/titulo": {"post"},
        "/api/status": {"get"},
        "/api/rag/status": {"get"},
        "/api/ble/conectar": {"post"},
        "/api/ble/error": {"post"},
        "/api/ble/desconectar": {"post"},
        "/api/ble/tx": {"get"},
        "/api/ble/ping": {"get"},
        "/api/ble/nack": {"post"},
        "/api/ble/rx": {"post"},
        "/api/chat": {"post"},
        "/api/chat/stream": {"post"},
        "/api/tts-stream/{tts_id}": {"get"},
        "/api/loading": {"post"},
        "/api/voz": {"post"},
        "/api/talk": {"post"},
        "/api/calla": {"post"},
        "/api/emocion": {"post"},
        "/api/sync": {"post"},
        "/api/stop": {"post"},
        "/api/cancelar": {"post"},
        "/api/comando": {"post"},
        "/api/transcribir": {"post"},
        "/api/grabar": {"post"},
        "/api/escuchar": {"post"},
    }
    openapi = ks.app.openapi()
    actual = {ruta: set(metodos) for ruta, metodos in openapi["paths"].items()}
    chequear(actual == esperado, "las 36 rutas API conservan path y método", sorted(actual.items()))

    print("\n== 2) endpoints movidos a routers ==")
    transport = httpx.ASGITransport(app=ks.app)
    async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
        r = await client.get("/api/personajes")
        cuerpo = r.json()
        chequear(r.status_code == 200, "GET /api/personajes responde 200", r.text)
        chequear([p.get("id") for p in cuerpo.get("personajes", [])] == ["kira"], "Kira sigue siendo la única identidad")

        manager = _ManagerFalso()
        serial_original = ks.serial_mgr
        ks.serial_mgr = manager
        try:
            r = await client.get("/api/status")
            chequear(r.status_code == 200 and "microbit" in r.json(), "GET /api/status conserva el shape", r.text)

            r = await client.get("/api/rag/status")
            rag = r.json().get("rag", {})
            chequear(r.status_code == 200 and "rag" in r.json(), "GET /api/rag/status expone el índice", r.text)
            chequear(
                isinstance(rag.get("embedding_dimension"), int)
                and rag.get("embedding_dimension") >= 0,
                "GET /api/rag/status informa dimensión de embeddings",
                rag,
            )
            chequear(
                "db_path" in rag and "min_similarity" in rag,
                "GET /api/rag/status informa ruta y umbral activos",
                rag,
            )

            async with ks.app.router.lifespan_context(ks.app):
                check = ks.http_clients.ai is not None and ks.http_clients.tts is not None
            chequear(check, "el lifespan crea clientes HTTP compartidos")
            chequear(ks.http_clients.ai is None and ks.http_clients.tts is None, "el lifespan cierra los clientes HTTP")
            chequear(manager.arrancado and manager.cerrado, "el lifespan inicia y cierra el transporte serial")
            chequear(manager.enviados == ["HAPPY"], "el lifespan modular sigue enviando HAPPY", manager.enviados)

            r = await client.post("/api/loading")
            chequear(r.status_code == 200 and manager.enviados[-1] == "LOADING", "POST /api/loading sigue usando SerialManager", r.text)

            r = await client.post("/api/emocion", json={"emotion": "happy"})
            chequear(r.status_code == 200 and manager.enviados[-1] == "HAPPY", "POST /api/emocion conserva el mapping", r.text)

            r = await client.post(
                "/api/ble/conectar",
                json={"token": "contrato", "extra": "aceptado"},
            )
            chequear(r.status_code == 200 and manager.registrados == ["contrato"], "BridgeTokenRequest conserva extras y token", r.text)
        finally:
            ks.serial_mgr = serial_original

    print("\n== 3) dominio puro reexportado por compatibilidad ==")
    aliases = [
        (ks.normalizar_foto, domain_normalizar_foto, "vision"),
        (ks.normalizar_emocion, domain_normalizar_emocion, "streaming"),
        (ks.dividir_frases, domain_dividir_frases, "TTS"),
        (ks.memoria_recuperar, domain_memoria_recuperar, "memoria"),
        (ks.diario_aplicar, domain_diario_aplicar, "diario"),
        (ks.extraer_tools, domain_extraer_tools, "tools"),
        (ks.clave_tool, domain_clave_tool, "dedup tools"),
    ]
    for legacy, domain, name in aliases:
        chequear(legacy is domain, f"kira_server.{name} sigue siendo el módulo puro")

    print("\n== 4) schema permisivo + monkeypatch compatible ==")

    async def post_falso(payload: dict) -> dict:
        return {"choices": [{"message": {"content": '{"title":"Prueba modular"}'}}]}

    original_post = ks.post_json_ia
    ks.post_json_ia = post_falso
    try:
        async with httpx.AsyncClient(transport=transport, base_url="http://test") as client:
            r = await client.post(
                "/api/titulo",
                json={
                    "primer_mensaje": "hola",
                    "primera_respuesta": "hola Kira",
                    "campo_extra": "se acepta como antes",
                },
            )
            chequear(r.status_code == 200 and r.json() == {"title": "Prueba modular"}, "TituloRequest acepta campos extra", r.text)

            r = await client.post("/api/titulo", json={})
            chequear(r.status_code == 400, "body inválido conserva el error 400 legacy", r.text)

        prefs = ks._preferencias_memoria({"memoria_config": {"recordar": False, "usar": "false"}})
        chequear(
            prefs == {"recordar": False, "hechos": True, "experiencias": True, "usar": False},
            "memoria_config interpreta interruptores explícitos",
            prefs,
        )
        catalogo = ks.catalogo_herramientas(permitir_memoria=False, permitir_consulta_memoria=False)
        chequear(
            "guardar_recuerdo" not in catalogo and "buscar_recuerdos" not in catalogo,
            "apagar memoria también oculta sus tools al modelo",
            catalogo,
        )
    finally:
        ks.post_json_ia = original_post


def main() -> int:
    asyncio.run(test_app())
    print()
    if FALLOS:
        print(f"FALLARON {len(FALLOS)} chequeo(s):")
        for fallo in FALLOS:
            print(f"  - {fallo}")
        return 1
    print("CONTRATO MODULAR OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
