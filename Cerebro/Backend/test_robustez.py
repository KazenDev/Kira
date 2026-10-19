"""test_robustez.py - Lo que NO puede romperse aunque el mundo haga cosas raras.

Pruebas de propiedades (cientos de casos al azar, semilla fija: reproducible)
sobre las piezas que, si fallan, se notan en la feria:

  1. SegmentadorTTS: el texto llega de a pedazos de tamaño al azar; lo que
     se manda a la voz, junto, tiene que ser EXACTAMENTE el texto (sin
     perder ni repetir una palabra: eso era la voz cortada o duplicada).
  2. quitar_muletillas: idempotente y nunca agrega texto.
  3. marcas_de_alineacion: tiempos y letras siempre crecientes, y el total
     es exactamente lo que se ve en pantalla (sin audio tags).
  4. /api/chat/stream con basura (tipos raros, historial roto, foto falsa,
     10 mil letras): nunca un 500 ni una excepcion; siempre cierra.

    ./.venv/bin/python test_robustez.py
"""
from __future__ import annotations

import asyncio
import json
import os
import random
import re
import sys

os.environ.setdefault("KIRA_SIN_SERIAL", "1")
os.environ.setdefault("KIRA_AUTH_REQUIRED", "0")

from app.domain.streaming import quitar_muletillas  # noqa: E402
from app.domain.tts import SegmentadorTTS, quitar_tags_audio  # noqa: E402
from app.services.speech import marcas_de_alineacion  # noqa: E402

FALLOS = 0
AZAR = random.Random(20261018)


def check(nombre: str, cond: bool, detalle: str = "") -> None:
    global FALLOS
    if not cond:
        FALLOS += 1
    print(f"  [{'OK ' if cond else 'FALLA'}] {nombre}" + (f"  -> {detalle[:200]}" if detalle and not cond else ""))


PALABRAS = ["hola", "Kira", "¿qué", "tal?", "bueno...", "la", "luz", "del", "sol,", "jaja", "¡ey!",
            "mirá.", "y", "vos", "[laughs]", "eso", "es", "rarísimo", "ah", "no", "sé", "...", "—", "ok.",
            "90%", "a", "las", "5:47", "Pitalito", "[whispers]", "cielo", "azul", "nada", "¿sí?", "…"]


def texto_al_azar() -> str:
    n = AZAR.randint(1, 60)
    return " ".join(AZAR.choice(PALABRAS) for _ in range(n))


def normal(t: str) -> str:
    return re.sub(r"\s+", " ", t).strip()


def test_segmentador() -> None:
    print("\n== 1) SegmentadorTTS: 400 textos al azar, llegando de a pedazos al azar ==")
    malos = 0
    ejemplo = ""
    for _ in range(400):
        texto = texto_al_azar()
        seg = SegmentadorTTS()
        frases: list[str] = []
        i = 0
        while i < len(texto):
            i = min(len(texto), i + AZAR.randint(1, 12))
            frases += seg.avanzar(texto[:i])
        frases += seg.terminar(texto)
        if normal(" ".join(frases)) != normal(texto) or any(not f.strip() for f in frases):
            malos += 1
            ejemplo = ejemplo or f"{texto!r} -> {frases!r}"
    check("lo que va a la voz == el texto (nada perdido, nada repetido)", malos == 0, ejemplo)


def test_muletillas() -> None:
    print("\n== 2) quitar_muletillas: idempotente y nunca agrega ==")
    extras = ["Buena pregunta.", "¡Qué buena pregunta!", "buena esa", "Ah, buena esa...", "che,", "pero che"]
    malos_idem = malos_largo = 0
    ej = ""
    for _ in range(400):
        partes = [texto_al_azar()]
        if AZAR.random() < 0.6:
            partes.insert(AZAR.randint(0, 1), AZAR.choice(extras))
        t = " ".join(partes)
        una = quitar_muletillas(t)
        if quitar_muletillas(una) != una:
            malos_idem += 1
            ej = ej or f"{t!r} -> {una!r} -> {quitar_muletillas(una)!r}"
        if len(una) > len(t):
            malos_largo += 1
    check("aplicarlo dos veces = una vez", malos_idem == 0, ej)
    check("nunca alarga el texto", malos_largo == 0)


def test_marcas() -> None:
    print("\n== 3) marcas_de_alineacion: crecientes y con el total de lo que se ve ==")
    malos = 0
    ej = ""
    for _ in range(300):
        texto = texto_al_azar()
        t, inicios = 0.0, []
        for _c in texto:
            t += AZAR.uniform(0.01, 0.12)
            inicios.append(round(t, 3))
        marcas = marcas_de_alineacion(list(texto), inicios, t + 0.1)
        tiempos = [m[0] for m in marcas]
        letras = [m[1] for m in marcas]
        visible = len(quitar_tags_audio(texto))
        ok = (tiempos == sorted(tiempos) and letras == sorted(letras)
              and letras[-1] == visible)
        if not ok:
            malos += 1
            ej = ej or f"{texto!r}: total {letras[-1]} vs visible {visible}"
    check("tiempos y letras crecientes; total == texto visible", malos == 0, ej)
    check("sin caracteres: solo la marca inicial", marcas_de_alineacion([], [], None) == [[0.0, 0]])


def test_lineas_pegadas() -> None:
    print("\n== 3b) BLE: dos lineas pegadas (se perdio el \\n) se separan ==")
    from app.hardware.serial_transport import separar_pegadas
    check("EVENTO + AMBIENTE pegados",
          separar_pegadas("EVENTO:DESPERTARAMBIENTE:26:26:0:24") == ["EVENTO:DESPERTAR", "AMBIENTE:26:26:0:24"])
    check("un eco de comando NO se corta", separar_pegadas("ACK:JUEGO:1") == ["ACK:JUEGO:1"])
    check("LED + EVENTO pegados", separar_pegadas("LED:" + "1" * 25 + "EVENTO:TOQUE") == ["LED:" + "1" * 25, "EVENTO:TOQUE"])
    nombres = ["EVENTO:MAREO", "JUEGO:DADO:4", "AMBIENTE:1:2:3:4", "ACK:LOADING", "EVENTO:TOQUE"]
    malos = 0
    for _ in range(300):
        lineas = [AZAR.choice(nombres) for _ in range(AZAR.randint(1, 4))]
        if separar_pegadas("".join(lineas)) != lineas:
            malos += 1
    check("300 combinaciones pegadas al azar vuelven a separarse", malos == 0)


async def test_endpoint_basura() -> None:
    print("\n== 4) /api/chat/stream con basura: nunca 500, siempre cierra ==")
    import httpx
    import kira_server as ks

    async def stream_falso(messages, caja, temperatura):
        cuerpo = {"emotion": "happy", "message": "Listo.", "tool": None}
        caja["buffer"] = json.dumps(cuerpo)
        caja["salida"] = cuerpo
        return
        yield  # noqa

    async def nada(*a, **k):
        return ""

    originales = (ks._stream_ia, ks.memoria_recuerdos_bloque, ks.memoria_memo_post_charla, ks.cargar_personaje)
    pj_original = ks.cargar_personaje

    def pj_sin_voz(nombre):
        p = pj_original(nombre)
        p["voice_id"] = "PENDIENTE"
        return p

    ks._stream_ia = stream_falso
    ks.memoria_recuerdos_bloque = nada
    ks.memoria_memo_post_charla = nada
    ks.cargar_personaje = pj_sin_voz
    cuerpos = [
        {"personaje": "kira", "mensaje": ""},
        {"personaje": "kira", "mensaje": "x" * 10000},
        {"personaje": "kira", "mensaje": None},
        {"personaje": "kira", "mensaje": 12345},
        {"personaje": "kira", "mensaje": ["lista"]},
        {"personaje": "nadie", "mensaje": "hola"},
        {"personaje": "kira", "mensaje": "hola", "historia": "no soy una lista"},
        {"personaje": "kira", "mensaje": "hola", "historia": [1, "dos", None, {"rol": 3}]},
        {"personaje": "kira", "mensaje": "hola", "historia": [{"rol": "user", "contenido": None}] * 300},
        {"personaje": "kira", "mensaje": "hola", "imagen": "data:image/jpeg;base64,!!!no-es-base64!!!"},
        {"personaje": "kira", "mensaje": "hola", "imagen": "http://evil.example/x.jpg"},
        {"personaje": "kira", "mensaje": "hola", "evento": "NO_EXISTE"},
        {"personaje": "kira", "mensaje": "hola", "sesion": {"raro": True}},
        {"personaje": "kira", "mensaje": "hola", "memoria_config": "si"},
        {"personaje": "kira", "mensaje": "\u0000‮🙂" * 50},
        {},
    ]
    try:
        transport = httpx.ASGITransport(app=ks.app)
        async with httpx.AsyncClient(transport=transport, base_url="http://test", timeout=30) as cli:
            for c in cuerpos:
                etiqueta = json.dumps(c, ensure_ascii=False)[:60]
                try:
                    async with cli.stream("POST", "/api/chat/stream", json=c) as r:
                        crudo = "".join([p async for p in r.aiter_text()])
                    cerro = r.status_code != 200 or '"tipo": "fin"' in crudo or '"tipo": "error"' in crudo
                    check(f"{r.status_code} y cierra: {etiqueta}", r.status_code < 500 and cerro,
                          crudo[-160:])
                except Exception as e:  # una excepcion aca es un 500 en produccion
                    check(f"sin excepcion: {etiqueta}", False, f"{type(e).__name__}: {e}")
            # JSON roto de verdad (no un dict)
            r = await cli.post("/api/chat/stream", content=b"{esto no es json", headers={"content-type": "application/json"})
            check(f"JSON roto -> 4xx ({r.status_code})", 400 <= r.status_code < 500)
    finally:
        ks._stream_ia, ks.memoria_recuerdos_bloque, ks.memoria_memo_post_charla, ks.cargar_personaje = originales


def main() -> int:
    print("=" * 62)
    print("TESTS DE ROBUSTEZ — propiedades al azar y basura en el endpoint")
    print("=" * 62)
    test_segmentador()
    test_muletillas()
    test_marcas()
    test_lineas_pegadas()
    asyncio.run(test_endpoint_basura())
    print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"FALLAS: {FALLOS} ❌"))
    return 1 if FALLOS else 0


if __name__ == "__main__":
    sys.exit(main())
