"""
test_stream.py — el endpoint de streaming NUNCA puede morir en silencio.

Correr:  .venv/bin/python test_stream.py   (desde Cerebro/Backend)
         .venv/bin/python test_stream.py --solo-unitarios   (sin gastar IA)

Que cubre:
  1. motivo_sin_salida(): explica la causa en los 4 casos reales (vacio,
     {"error": ...} del proveedor, JSON cortado, JSON sin el formato)
  2. El ENDPOINT con una IA que contesta HTTP 200 con {"error": ...} (el caso
     real del 17-sep con la key vencida de nano-gpt): antes terminaba mudo y la
     web mostraba "se corto la conexion"; ahora manda un evento error con el
     motivo real. Se prueba SIN red (stub del stream de la IA).
  3. Stream HTTP 200 SIN contenido (caso real 23-sep con DeepSeek): antes
     pasaba como "exito" y el usuario veia "la IA no respondio nada" sin
     reintento; ahora reintenta y, si sigue mudo, cierra con error claro.
  4. INTEGRACION real (1 llamada chiquita de IA): el chat de verdad emite
     eventos delta progresivos y cierra con fin + emotion + message.
"""
import asyncio
import json
import sys
import traceback

import httpx

import kira_server as ks

FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    estado = "OK " if cond else "FALLA"
    if not cond:
        FALLOS += 1
    print(f"  [{estado}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))


def test_unitarios():
    print("\n== 1) motivo_sin_salida: la causa real, no un mensaje generico ==")

    razon = ks.motivo_sin_salida({"buffer": ""})
    check("buffer vacio -> dice que la IA no respondio", "no respondió nada" in razon, razon)

    razon = ks.motivo_sin_salida({"buffer": '{"error": {"message": "Invalid session", "type": "invalid_api_key"}}'})
    check("error del proveedor -> muestra SU mensaje", "Invalid session" in razon, razon)

    razon = ks.motivo_sin_salida({"buffer": '{"emotion": "happy", "message": "che que'})
    check("JSON cortado -> dice que vino incompleto y muestra el pedazo", "incompleta" in razon and "che que" in razon, razon)

    razon = ks.motivo_sin_salida({"buffer": '{"otra_cosa": 1}'})
    check("JSON valido pero sin el formato de la app -> lo dice", "formato" in razon, razon)

    casos = [{}, {"buffer": None}, {"buffer": "no soy json"}, {"buffer": "[1,2,3]"}]
    check("nunca revienta ni devuelve un motivo vacio", all(ks.motivo_sin_salida(c).strip() for c in casos))


async def test_endpoint_no_muere_mudo():
    print("\n== 2) el endpoint con una IA que devuelve 200 + {'error': ...} ==")

    original_stream = ks._stream_ia
    original_recuerdos = ks.memoria_recuerdos_bloque
    original_memo = ks.memoria_memo_post_charla

    async def stream_roto(messages, caja, temperatura):
        # exactamente lo que hacia nano-gpt con la key vencida: HTTP 200 con
        # un cuerpo que NO es la respuesta de la app
        caja["buffer"] = '{"error": {"message": "Invalid session", "type": "invalid_api_key"}}'
        caja["salida"] = {}
        return
        yield  # (hace que sea un async generator)

    async def sin_recuerdos(*a, **k):
        return ""

    async def memo_sin_efecto(*a, **k):
        return None

    ks._stream_ia = stream_roto
    ks.memoria_recuerdos_bloque = sin_recuerdos
    ks.memoria_memo_post_charla = memo_sin_efecto
    try:
        transport = httpx.ASGITransport(app=ks.app)
        async with httpx.AsyncClient(transport=transport, base_url="http://test") as cliente:
            async with cliente.stream(
                "POST", "/api/chat/stream",
                json={"personaje": "kira", "mensaje": "hola", "historia": []},
            ) as r:
                cuerpo = "".join([parte async for parte in r.aiter_text()])
        check("manda un evento 'error' (no termina mudo)", '"tipo": "error"' in cuerpo, cuerpo[:220])
        check("el evento dice la CAUSA real", "Invalid session" in cuerpo, cuerpo[:220])
        check("no inventa un 'fin' con mensaje vacio", '"tipo": "fin"' not in cuerpo, cuerpo[:220])
    finally:
        ks._stream_ia = original_stream
        ks.memoria_recuerdos_bloque = original_recuerdos
        ks.memoria_memo_post_charla = original_memo


async def test_integracion():
    print("\n== 3) INTEGRACION real: el chat de verdad streamea y cierra con fin ==")

    original_memo = ks.memoria_memo_post_charla

    async def memo_sin_efecto(*a, **k):
        # no ensuciar la memoria/diario de Kira con una charla de test
        return None

    ks.memoria_memo_post_charla = memo_sin_efecto
    try:
        transport = httpx.ASGITransport(app=ks.app)
        async with httpx.AsyncClient(transport=transport, base_url="http://test", timeout=90) as cliente:
            async with cliente.stream(
                "POST", "/api/chat/stream",
                json={"personaje": "kira", "mensaje": "Contame en una frase corta como estás.", "historia": []},
            ) as r:
                eventos = [parte async for parte in r.aiter_text()]
        crudo = "".join(eventos)
        deltas = crudo.count('"tipo": "delta"')
        print(f"        eventos delta recibidos: {deltas}")
        check("streamed texto en vivo (delta >= 1)", deltas >= 1, f"deltas={deltas}")
        check("cerro con el evento 'fin'", '"tipo": "fin"' in crudo)
        check("el fin trae emotion + message", '"emotion"' in crudo and '"message"' in crudo)
        check("NO hubo error", '"tipo": "error"' not in crudo, crudo[:300])
        for linea in crudo.splitlines():
            if '"tipo": "fin"' in linea:
                print(f"        fin: {linea[:160]}")
    finally:
        ks.memoria_memo_post_charla = original_memo


async def test_herramienta_con_texto():
    print("\n== 2b) la IA pide una herramienta Y escribe texto: se ejecuta IGUAL ==")
    # Caso REAL del 17-sep: el usuario pidió la temperatura, la IA pidió
    # 'leer_temperatura' pero con texto en el mismo JSON, y el backend tiraba
    # la herramienta ("se IGNORA la herramienta") -> el usuario recibía solo la
    # promesa y el sensor nunca se leía. Ahora se ejecuta igual.
    original_stream = ks._stream_ia
    original_recuerdos = ks.memoria_recuerdos_bloque
    original_memo = ks.memoria_memo_post_charla
    original_tool = ks.ejecutar_herramienta
    original_pj = ks.cargar_personaje

    llamadas: list[str] = []
    rondas: list[list[str]] = []

    async def stream_dos_rondas(messages, caja, temperatura):
        rondas.append([str(m.get("content", "")) for m in messages])
        if len(rondas) == 1:
            # ronda 1: la IA rompe la regla (pide el sensor Y escribe texto)
            cuerpo = {"emotion": "surprised", "message": "uy, dejame sentir eso",
                      "tool": {"nombre": "leer_temperatura"}}
        else:
            # ronda 2: ya con el dato real, el mensaje final
            cuerpo = {"emotion": "happy", "message": "estamos a veintiun grados", "tool": None}
        caja["buffer"] = json.dumps(cuerpo, ensure_ascii=False)
        caja["salida"] = cuerpo
        return
        yield  # (hace que sea un async generator)

    def tool_falsa(nombre, args=None):
        llamadas.append(nombre)
        return {"ok": True, "resultado": "temperatura: 21 grados Celsius"}

    async def sin_recuerdos(*a, **k):
        return ""

    async def memo_sin_efecto(*a, **k):
        return None

    def pj_sin_voz(nombre):
        p = original_pj(nombre)
        p["voice_id"] = "PENDIENTE"   # sin TTS (no queremos red en el test)
        return p

    ks._stream_ia = stream_dos_rondas
    ks.memoria_recuerdos_bloque = sin_recuerdos
    ks.memoria_memo_post_charla = memo_sin_efecto
    ks.ejecutar_herramienta = tool_falsa
    ks.cargar_personaje = pj_sin_voz
    try:
        transport = httpx.ASGITransport(app=ks.app)
        async with httpx.AsyncClient(transport=transport, base_url="http://test") as cliente:
            async with cliente.stream(
                "POST", "/api/chat/stream",
                json={"personaje": "kira", "mensaje": "¿cuántos grados hay?", "historia": []},
            ) as r:
                crudo = "".join([parte async for parte in r.aiter_text()])

        check("ejecutó la herramienta (no la tiró por venir con texto)",
              llamadas == ["leer_temperatura"], f"llamadas={llamadas}")
        check("avisó al frontend que está midiendo", '"tipo": "tool"' in crudo, crudo[:200])
        check("hubo ronda 2 con el resultado REAL",
              len(rondas) == 2 and "[RESULTADO DE HERRAMIENTA]" in rondas[1][-1],
              f"rondas={len(rondas)}")
        check("la ronda 2 sabe que el texto anterior no tenía el dato",
              len(rondas) == 2 and "OJO:" in rondas[1][-1], f"rondas={len(rondas)}")
        fin = [l for l in crudo.splitlines() if '"tipo": "fin"' in l]
        check("cerró con 'fin'", len(fin) == 1, crudo[:200])
        check("el mensaje final usa el dato real (no la promesa de la ronda 1)",
              bool(fin) and "veintiun grados" in fin[0] and "dejame sentir" not in fin[0],
              fin[0][:200] if fin else "(sin fin)")
    finally:
        ks._stream_ia = original_stream
        ks.memoria_recuerdos_bloque = original_recuerdos
        ks.memoria_memo_post_charla = original_memo
        ks.ejecutar_herramienta = original_tool
        ks.cargar_personaje = original_pj


async def test_stream_vacio_reintenta():
    print("\n== 3) stream 200 VACIO (caso real 23-sep): reintenta, no entregar 'no respondio nada' a la primera ==")

    lineas_ok = [
        'data: {"choices":[{"delta":{"content":"{\\"emotion\\":\\"happy\\","}}]}',
        'data: {"choices":[{"delta":{"content":"\\"message\\":\\"hola\\"}"}}]}',
        "data: [DONE]",
    ]

    class _Resp:
        def __init__(self, lineas):
            self.status_code = 200
            self.headers = {}
            self._lineas = lineas

        def raise_for_status(self):
            pass

        async def aiter_lines(self):
            for linea in self._lineas:
                yield linea

    class _Ctx:
        def __init__(self, resp):
            self._resp = resp

        async def __aenter__(self):
            return self._resp

        async def __aexit__(self, *args):
            return False

    estado = {"vacias": 1, "llamadas": 0}

    class _Cliente:
        """Las primeras `vacias` pasadas cierran el stream mudo (200, cero
        deltas) — exactamente lo que hizo DeepSeek el 23-sep."""

        def __init__(self, **kwargs):
            pass

        async def __aenter__(self):
            return self

        async def __aexit__(self, *args):
            return False

        def stream(self, *args, **kwargs):
            estado["llamadas"] += 1
            mudo = estado["llamadas"] <= estado["vacias"]
            return _Ctx(_Resp([] if mudo else lineas_ok))

    async def no_dormir(*a, **k):
        return None

    original_client = ks.httpx.AsyncClient
    original_sleep = ks.esperar_reintento
    ks.httpx.AsyncClient = _Cliente
    ks.esperar_reintento = no_dormir
    try:
        # A) la 1a pasada llega muda, la 2a contesta: el usuario NUNCA ve el error
        caja: dict = {}
        eventos = [ev async for ev in ks._stream_ia([{"role": "user", "content": "hola"}], caja, 0.8)]
        tipos = [json.loads(e[5:]).get("tipo") for e in eventos if e.startswith("data:")]
        check("la pasada muda NO se entregó como error", "error" not in tipos, str(tipos))
        check("hubo más de una pasada (esperando/reintento)",
              tipos.count("esperando") >= 2, str(tipos))
        check("la 2a pasada llenó la caja con la respuesta real",
              caja.get("salida", {}).get("message") == "hola", str(caja.get("salida")))

        # B) SIEMPRE mudo: tras agotar intentos + respaldo, error CLARO (nunca mudo)
        estado.update({"vacias": 99, "llamadas": 0})
        caja2: dict = {}
        eventos2 = [ev async for ev in ks._stream_ia([{"role": "user", "content": "hola"}], caja2, 0.8)]
        tipos2 = [json.loads(e[5:]).get("tipo") for e in eventos2 if e.startswith("data:")]
        esperado = ks.MAX_INTENTOS_IA * len(ks.config.PROVEEDORES_IA)
        check("agotó todos los intentos de TODOS los proveedores",
              estado["llamadas"] == esperado, f'{estado["llamadas"]} llamadas != {esperado}')
        check("terminó con un error EXPLÍCITO (no mudo)", tipos2.count("error") == 1, str(tipos2))
        mensaje = ""
        for e in eventos2:
            pl = json.loads(e[5:]) if e.startswith("data:") else {}
            if pl.get("tipo") == "error":
                mensaje = str(pl.get("mensaje", ""))
        check("el error dice que contestó vacío (no un genérico)", "vacío" in mensaje, mensaje)
        check("la caja quedó marcada con error (el endpoint no inventa fin)",
              caja2.get("error") is True, str(caja2))
    finally:
        ks.httpx.AsyncClient = original_client
        ks.esperar_reintento = original_sleep


def main():
    print("=" * 62)
    print("TESTS — el stream nunca muere mudo")
    print("=" * 62)
    try:
        test_unitarios()
        asyncio.run(test_endpoint_no_muere_mudo())
        asyncio.run(test_stream_vacio_reintenta())
        asyncio.run(test_herramienta_con_texto())
        if "--solo-unitarios" not in sys.argv:
            asyncio.run(test_integracion())
        else:
            print("\n(integracion salteada)")
    except Exception:
        traceback.print_exc()
        global FALLOS
        FALLOS += 1
    print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"{FALLOS} FALLAS ❌"))


if __name__ == "__main__":
    main()
