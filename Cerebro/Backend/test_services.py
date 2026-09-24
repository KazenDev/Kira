"""Pruebas deterministas de servicios, storage y ciclo de tareas.

Ejecutar:
    .venv/bin/python test_services.py
"""

from __future__ import annotations

import asyncio
import tempfile
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from types import SimpleNamespace

from app.services.memory import MemoryStore
import app.services.speech as speech_module
from app.services.speech import SpeechService
from app.services.storage import JsonStore
from app.services.tasks import BackgroundTasks

FALLOS: list[str] = []


def check(condition: bool, name: str, detail: object = "") -> None:
    if condition:
        print(f"  [OK ] {name}")
    else:
        print(f"  [FALLA] {name} -> {detail}")
        FALLOS.append(name)


def test_storage() -> None:
    print("== 1) JSON atómico y JSONL ==")
    with tempfile.TemporaryDirectory(prefix="kira_storage_") as tmp:
        store = JsonStore()
        path = Path(tmp) / "estado.json"
        store.write_json(path, {"contador": 7})
        check(store.read_json(path, {}) == {"contador": 7}, "write_json + read_json")
        check(not list(Path(tmp).glob("*.tmp")), "no quedan temporales tras os.replace")

        jsonl = Path(tmp) / "eventos.jsonl"
        store.append_jsonl(jsonl, {"n": 1})
        store.append_jsonl(jsonl, {"n": 2})
        check(store.read_jsonl(jsonl) == [{"n": 1}, {"n": 2}], "append_jsonl conserva líneas")


def test_memory_concurrency() -> None:
    print("\n== 2) memoria concurrente por personaje ==")
    with tempfile.TemporaryDirectory(prefix="kira_memory_") as tmp:
        store = MemoryStore(lambda: tmp, reflection_threshold=150, expiry_days=30)
        with ThreadPoolExecutor(max_workers=8) as pool:
            records = list(pool.map(lambda n: store.add_memory("kira", f"recuerdo {n}", importance=1), range(24)))
        state = store.load_state("kira")
        memories = store.load_memories("kira")
        check(len(records) == 24, "24 escrituras terminan", len(records))
        check(len(memories) == 24, "no se pierde ningún JSONL", len(memories))
        check(state["total"] == 24, "el contador total no se pisa", state)
        check(state["contador"] == 126, "el contador resta una sola vez por recuerdo", state)


def test_reflection_claim() -> None:
    print("\n== 3) claim atómico de reflexión ==")
    with tempfile.TemporaryDirectory(prefix="kira_reflection_") as tmp:
        store = MemoryStore(lambda: tmp)
        for index in range(6):
            store.add_memory("kira", f"vivencia {index}", importance=1)
        state = store.load_state("kira")
        state["reflexion_pendiente"] = True
        store.save_state("kira", state)

        def claim(_: int):
            return store.claim_reflection("kira", 6)[0]

        with ThreadPoolExecutor(max_workers=8) as pool:
            results = list(pool.map(claim, range(8)))
        check(results.count(True) == 1, "sólo una reflexión obtiene el claim", results)
        store.finish_reflection("kira")
        final = store.load_state("kira")
        check(final.get("reflexionando") is False, "finish deja el candado en false", final)
        check(final.get("reflexion_pendiente") is False, "finish limpia pendiente", final)

        state = store.load_state("kira")
        state["reflexion_pendiente"] = True
        store.save_state("kira", state)
        claimed, _ = store.claim_reflection("kira", 999)
        omitted = store.load_state("kira")
        check(not claimed, "memorias insuficientes no entran en reflexión", omitted)
        check(omitted.get("reflexiones_omitidas") == 1, "se registra el intento omitido", omitted)


def test_speech_registry() -> None:
    print("\n== 4) registro TTS lazy ==")
    service = SpeechService(SimpleNamespace(), cache_max=2)
    first = service.register("uno", "voice")
    service.register("dos", "voice")
    third = service.register("tres", "voice")
    check(service.get_pending(first) is None, "la entrada más viejo se evictiona")
    check(service.get_pending(third)["texto"] == "tres", "la última entrada queda disponible")
    service._store_bytes(third, b"audio")
    check(service.get_cached(third) == b"audio", "el audio generado vuelve a cache")


async def test_speech_stream() -> None:
    print("\n== 5) Fish Audio sigue siendo lazy ==")

    class FakeResponse:
        async def __aenter__(self):
            return self

        async def __aexit__(self, *args):
            return False

        def raise_for_status(self):
            return None

        async def aiter_bytes(self):
            yield b"mp3-a"
            yield b"mp3-b"

    class FakeClient:
        calls = 0

        def __init__(self, *args, **kwargs):
            pass

        async def __aenter__(self):
            return self

        async def __aexit__(self, *args):
            return False

        def stream(self, *args, **kwargs):
            FakeClient.calls += 1
            return FakeResponse()

    service = SpeechService(
        SimpleNamespace(
            FISH_BASE_URL="https://fish.invalid",
            FISH_API_KEY="fake",
            FISH_MODELO="fake",
        )
    )
    tts_id = service.register("hola", "voice")
    check(FakeClient.calls == 0, "registrar la URL no llama a Fish")
    original = speech_module.httpx.AsyncClient
    speech_module.httpx.AsyncClient = FakeClient
    try:
        chunks = [chunk async for chunk in service.stream(tts_id)]
    finally:
        speech_module.httpx.AsyncClient = original
    check(chunks == [b"mp3-a", b"mp3-b"], "el stream reenvía los chunks", chunks)
    check(service.get_cached(tts_id) == b"mp3-amp3-b", "el stream arma la cache lazy", service.get_cached(tts_id))


async def test_background_tasks() -> None:
    print("\n== 6) tareas background con referencias fuertes ==")
    registry = BackgroundTasks()
    event = asyncio.Event()

    async def endless():
        await event.wait()

    task = registry.spawn("test", endless())
    await asyncio.sleep(0)
    check(task in registry._tasks, "la tarea queda registrada")
    await registry.cancel_all()
    check(task.cancelled(), "shutdown cancela la tarea")
    check(not registry._tasks, "el registro queda limpio")


def main() -> int:
    test_storage()
    test_memory_concurrency()
    test_reflection_claim()
    test_speech_registry()
    asyncio.run(test_speech_stream())
    asyncio.run(test_background_tasks())
    print()
    if FALLOS:
        print(f"FALLARON {len(FALLOS)} chequeo(s):")
        for failure in FALLOS:
            print(f"  - {failure}")
        return 1
    print("SERVICIOS OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
