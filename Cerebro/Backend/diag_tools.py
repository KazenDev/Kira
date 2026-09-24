"""diag_tools.py — batería contra DeepSeek REAL: mide qué herramienta (si
alguna) elige la IA para cada pedido. Corre EN PROCESO (ASGITransport, sin
servidor) con memoria de Kira AISLADA (directorio temporal) y sin TTS.

Correr: .venv/bin/python diag_tools.py
        .venv/bin/python diag_tools.py --vueltas 2   (repetir la bateria)
"""
import asyncio
import json
import sys
import tempfile

import httpx

import kira_server as ks

# (mensaje, herramientas esperadas en orden; [] = NO debe usar ninguna)
CASOS: list[tuple[str, list[str]]] = [
    ("¿cuánto es el 15% de 200?", ["calcular"]),
    ("¿qué hora es?", ["reloj"]),
    ("busca quién ganó el último mundial de fútbol", ["buscar_en_web"]),
    ("guarda en tu memoria que me llamo Mateo y me gustan los mates", ["guardar_recuerdo"]),
    ("¿hace frío ahí?", ["leer_temperatura"]),
    ("¿qué tan iluminado está alrededor?", ["leer_luz"]),
    ("¿estoy tocando los botones A o B?", ["leer_botones"]),
    ("¿cómo está orientada la plaquita ahora?", ["leer_movimiento"]),
    ("¿qué tan fuerte se escucha alrededor?", ["leer_sonido"]),
    ("¿qué voltaje de batería tiene la plaquita?", ["leer_bateria"]),
    ("lee esta página y contame qué dice: https://en.wikipedia.org/wiki/Micro_Bit", ["leer_url"]),
    ("decime al azar qué merendar: manzana, banana o yogurt", ["azar"]),
    ("¿estás conectada a la plaquita?", ["leer_estado"]),
    ("¿qué hora es y cuánto es el 20% de 500?", ["reloj", "calcular"]),   # multi-tool
    ("contame un chiste corto", []),                                       # anti-overcall
]


async def _un_caso(cliente: httpx.AsyncClient, mensaje: str) -> dict:
    tools: list[str] = []
    fin = None
    error = None
    async with cliente.stream(
        "POST", "/api/chat/stream",
        json={"personaje": "kira", "mensaje": mensaje, "historia": []},
    ) as r:
        async for linea in r.aiter_lines():
            if not linea.startswith("data: "):
                continue
            try:
                ev = json.loads(linea[6:])
            except (json.JSONDecodeError, ValueError):
                continue
            if ev.get("tipo") == "tool":
                tools.append(ev.get("nombre", "?"))
            elif ev.get("tipo") == "fin":
                fin = ev
            elif ev.get("tipo") == "error":
                error = ev.get("mensaje")
    return {"tools": tools, "fin": fin, "error": error}


def _preparar(ks_mod):
    """Aisla la memoria de Kira y desactiva TTS/memo/debug para la bateria."""
    tmp = tempfile.mkdtemp(prefix="kira_diag_tools_")
    ks_mod.MEMORIA_DIR = tmp

    async def sin_memo(*a, **k):
        return None

    async def sin_recuerdos(*a, **k):
        return ""

    original_pj = ks_mod.cargar_personaje

    def pj_sin_voz(nombre):
        p = original_pj(nombre)
        p["voice_id"] = "PENDIENTE"
        return p

    def turno_dummy(*a, **k):
        None

    ks_mod.memoria_memo_post_charla = sin_memo
    ks_mod.memoria_recuerdos_bloque = sin_recuerdos
    ks_mod.cargar_personaje = pj_sin_voz
    ks_mod.guardar_turno = turno_dummy
    return tmp


async def main(vueltas: int = 1):
    tmp = _preparar(ks)
    print(f"memoria aislada en {tmp} | modelo: {ks.config.MODELO_IA}")
    print("=" * 78)
    total_aciertos = total = 0
    for vuelta in range(1, vueltas + 1):
        if vueltas > 1:
            print(f"\n----- VUELTA {vuelta}/{vueltas} -----")
        transport = httpx.ASGITransport(app=ks.app)
        async with httpx.AsyncClient(transport=transport, base_url="http://test", timeout=180) as cliente:
            for mensaje, esperadas in CASOS:
                r = await _un_caso(cliente, mensaje)
                tools, fin, error = r["tools"], r["fin"], r["error"]
                ok = tools == esperadas
                total += 1
                total_aciertos += ok
                marca = "ACIERTO" if ok else "FALLA "
                print(f"[{marca}] pedidas={tools!s:45} esperadas={esperadas!s:25}")
                print(f"          pregunta: {mensaje[:70]}")
                if error:
                    print(f"          ERROR: {error}")
                elif not fin:
                    print("          ERROR: sin evento fin")
                else:
                    print(f"          rta: {(fin.get('message') or '')[:90]}")
    print("=" * 78)
    print(f"SELECCION DE TOOLS: {total_aciertos}/{total}")
    sys.exit(0 if total_aciertos == total else 1)


if __name__ == "__main__":
    vueltas = 1
    if "--vueltas" in sys.argv:
        vueltas = int(sys.argv[sys.argv.index("--vueltas") + 1])
    asyncio.run(main(vueltas))
