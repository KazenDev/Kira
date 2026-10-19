"""
test_herramientas.py — el sistema de tools de la IA: multi-tool, dedup, tope.

Correr:  .venv/bin/python test_herramientas.py   (desde Cerebro/Backend)
         .venv/bin/python test_herramientas.py --solo-unitarios   (sin stubs de endpoint)

Que cubre:
  1. extraer_tools(): normaliza tool dict | lista | string | null (multi-tool).
  2. clave_tool() / es_tool_placa(): dedup y qué va en cadena (serial) vs paralelo.
  3. ejecutar_herramienta: dispatch de las tools nuevas (guardar_recuerdo, azar,
     leer_url errores accionables) + desconocida.
  4. ENDPOINT multi-tool: la IA pide VARIAS tools en una lista -> se ejecutan
     todas y vuelve UN solo bloque [RESULTADO DE HERRAMIENTA] consolidado.
  5. ENDPOINT tope: la IA pide tool sin parar -> sólo MAX_RONDAS_HERR
     ejecuciones y después una ronda forzada; cierra con 'fin', nunca loop.
  6. ENDPOINT dedup: la misma llamada repetida en rondas consecutivas NO se
     re-ejecuta (cache) y se le avisa al modelo.
  Sin red: el stream de la IA se stubbea (patrón de test_stream.py).
"""
import asyncio
import json
import os
import sys
import tempfile
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
    print("\n== 1) extraer_tools: dict | lista | string | basura ==")
    check("dict clasico -> 1 pedido",
          ks.extraer_tools({"nombre": "reloj"}) == [{"nombre": "reloj"}],
          str(ks.extraer_tools({"nombre": "reloj"})))
    lista = ks.extraer_tools([{"nombre": "reloj"}, {"nombre": "calcular", "expresion": "2+2"}])
    check("lista (multi-tool) -> 2 pedidos en orden", len(lista) == 2 and lista[1]["expresion"] == "2+2",
          str(lista))
    check("nombre suelto en string -> pedido", ks.extraer_tools("reloj") == [{"nombre": "reloj"}],
          str(ks.extraer_tools("reloj")))
    check("alias 'name' (tolerancia) -> pedido", ks.extraer_tools({"name": "reloj"})[0]["nombre"] == "reloj",
          str(ks.extraer_tools({"name": "reloj"})))
    check("null -> sin pedidos", ks.extraer_tools(None) == [])
    check("string vacio -> sin pedidos", ks.extraer_tools("") == [])
    check("basura -> sin pedidos (no revienta)",
          ks.extraer_tools([None, 42, {"sin_nombre": 1}]) == [],
          str(ks.extraer_tools([None, 42, {"sin_nombre": 1}])))

    print("\n== 2) clave_tool (dedup) y es_tool_placa (cadena vs paralelo) ==")
    a = ks.clave_tool({"nombre": "calcular", "expresion": "2+2"})
    b = ks.clave_tool({"nombre": "calcular", "expresion": "2+2"})
    c = ks.clave_tool({"nombre": "calcular", "expresion": "3+3"})
    check("mismos args -> misma clave (dedup)", a == b)
    check("args distintos -> clave distinta", a != c)
    check("el orden de los args no importa",
          ks.clave_tool({"nombre": "x", "a": 1, "b": 2}) == ks.clave_tool({"nombre": "x", "b": 2, "a": 1}))
    seriales = [
        "leer_temperatura", "leer_luz", "leer_botones", "leer_movimiento",
        "leer_sonido", "leer_bateria", "leer_gesto", "leer_brujula",
        "leer_toque", "controlar_metronomo",
    ]
    check("tools de la placa -> cadena (serial)", all(ks.es_tool_placa(n) for n in seriales),
          str([n for n in seriales if not ks.es_tool_placa(n)]))
    paralelas = ["reloj", "buscar_en_web", "calcular", "leer_url", "guardar_recuerdo", "azar", "leer_estado"]
    check("tools web/local -> paralelas", not any(ks.es_tool_placa(n) for n in paralelas),
          str([n for n in paralelas if ks.es_tool_placa(n)]))
    check("desconocida no revienta", ks.es_tool_placa("no_existe") is False)

    print("\n== 3) dispatch de las tools nuevas ==")
    r = ks.ejecutar_herramienta("no_existe_tal_tool")
    check("desconocida -> error claro", r["ok"] is False and "no_existe_tal_tool" in r["resultado"],
          r["resultado"])

    tmp = tempfile.mkdtemp(prefix="kira_test_mem_")
    original_mem_dir = ks.MEMORIA_DIR
    ks.MEMORIA_DIR = tmp
    try:
        r = ks.ejecutar_herramienta("guardar_recuerdo", {"texto": "Mateo odia los lunes", "personaje": "kira", "explicit_user": True})
        check("guardar_recuerdo ok", r["ok"] is True, r["resultado"])
        r = ks.ejecutar_herramienta("guardar_recuerdo", {"texto": "No guardar esto", "personaje": "kira"})
        check("guardar_recuerdo exige pedido explícito", r["ok"] is False, r["resultado"])
        ruta = os.path.join(tmp, "kira.jsonl")
        check("guardar_recuerdo escribio en la memoria", os.path.exists(ruta) and "Mateo odia los lunes" in open(ruta, encoding="utf-8").read())
        r = ks.ejecutar_herramienta("guardar_recuerdo", {"personaje": "kira"})
        check("guardar_recuerdo sin texto -> error accionable", r["ok"] is False and "texto" in r["resultado"],
              r["resultado"])
    finally:
        ks.MEMORIA_DIR = original_mem_dir

    r = ks.ejecutar_herramienta("azar", {"opciones": ["cara", "seca"]})
    check("azar elige de la lista", r["ok"] is True and r["resultado"].split(": ")[-1] in ("cara", "seca"),
          r["resultado"])
    r = ks.ejecutar_herramienta("azar", {"opciones": "manzana, banana, yogurt"})
    check("azar acepta string con comas", r["ok"] is True and r["resultado"].split(": ")[-1] in ("manzana", "banana", "yogurt"),
          r["resultado"])
    r = ks.ejecutar_herramienta("azar", {})
    check("azar sin opciones -> error accionable", r["ok"] is False and "opciones" in r["resultado"],
          r["resultado"])

    r = ks.ejecutar_herramienta("leer_url", {"url": "ftp://no"})
    check("leer_url sin http -> error accionable", r["ok"] is False and "http" in r["resultado"],
          r["resultado"])

    r = ks.ejecutar_herramienta("leer_estado", {"personaje": "kira"})
    check("leer_estado responde emocion + placa + hora",
          r["ok"] is True and "emocion" in r["resultado"] and "micro:bit" in r["resultado"],
          r["resultado"])

    r = ks.ejecutar_herramienta("calcular", {"expresion": "15% de 200"})
    check("calcular sigue intacta", r["ok"] is True and "30" in r["resultado"], r["resultado"])

    print("\n== 4) catalogo de herramientas (lo que viaja al modelo) ==")
    # Con la key de Exa se ofrecen todas; sin ella, la web se oculta (fallaria
    # siempre con 401) y el catalogo lo dice para que Kira no prometa buscar.
    key_real = ks.config.EXA_API_KEY
    ks.config.EXA_API_KEY = ""
    sin_web = ks.catalogo_herramientas()
    check("sin key de Exa: NO ofrece buscar_en_web ni leer_url",
          "- buscar_en_web:" not in sin_web and "- leer_url:" not in sin_web)
    check("sin key de Exa: avisa que hoy no hay busqueda", "NO HAY BUSQUEDA" in sin_web)
    check("sin key de Exa: el clima sigue (ver_clima)", "- ver_clima:" in sin_web)
    ks.config.EXA_API_KEY = "clave-de-prueba"
    cat = ks.catalogo_herramientas()
    ks.config.EXA_API_KEY = key_real
    check("lista TODAS las tools", all(f"- {n}:" in cat for n in ks.HERRAMIENTAS),
          str([n for n in ks.HERRAMIENTAS if f"- {n}:" not in cat]))
    check("dice cuando NO usar", "NO la uses" in cat, cat[:120])
    check("explica el formato lista (multi-tool)", "LISTA" in cat, cat[:120])
    check("habla del dedup", "no la repitas" in cat.lower(), cat[:120])

    print("\n== 4b) sensores onboard: registro, formato y dispatch ==")
    esperados = {
        "leer_temperatura": ("SENSOR:TEMP", "TEMP:"),
        "leer_luz": ("SENSOR:LUZ", "LUZ:"),
        "leer_botones": ("SENSOR:BOTON", "BOTON:"),
        "leer_movimiento": ("SENSOR:ACCEL", "ACCEL:"),
        "leer_sonido": ("SENSOR:MIC", "MIC:"),
        "leer_bateria": ("SENSOR:BAT", "BAT:"),
        "leer_gesto": ("SENSOR:GESTO", "GESTO:"),
        "leer_brujula": ("SENSOR:BRUJULA", "BRUJULA:"),
        "leer_toque": ("SENSOR:TOQUE", "TOUCH:"),
    }
    check(
        "los nueve sensores tienen comando y prefijo",
        all(
            ks.HERRAMIENTAS.get(n, {}).get("comando") == c
            and ks.HERRAMIENTAS.get(n, {}).get("prefijo") == p
            for n, (c, p) in esperados.items()
        ),
        str({n: ks.HERRAMIENTAS.get(n) for n in esperados}),
    )
    check(
        "los cuatro sensores anteriores tambien formatean",
        all([
            "23 grados" in ks.formatear_sensor("leer_temperatura", "TEMP:23"),
            "120/255" in ks.formatear_sensor("leer_luz", "LUZ:120"),
            "A=presionado" in ks.formatear_sensor("leer_botones", "BOTON:1:0"),
            "pitch=3" in ks.formatear_sensor("leer_movimiento", "ACCEL:0:1000:5:3:0"),
        ]),
        "formateo de sensores existentes",
    )
    check(
        "el formato de sonido explica que es relativo",
        "nivel relativo" in ks.formatear_sensor("leer_sonido", "MIC:42:40:20:10:5:2:1"),
        ks.formatear_sensor("leer_sonido", "MIC:42:40:20:10:5:2:1"),
    )
    check(
        "el formato de bateria identifica la fuente",
        "batería" in ks.formatear_sensor("leer_bateria", "BAT:2980:3000:2"),
        ks.formatear_sensor("leer_bateria", "BAT:2980:3000:2"),
    )
    check(
        "el formato de gesto traduce el codigo CODAL y la magnitud",
        "sacudida" in ks.formatear_sensor("leer_gesto", "GESTO:11:1450")
        and "1450 mili-g" in ks.formatear_sensor("leer_gesto", "GESTO:11:1450"),
        ks.formatear_sensor("leer_gesto", "GESTO:11:1450"),
    )
    check(
        "el formato de brújula avisa cuando falta calibración",
        "NO está calibrada" in ks.formatear_sensor("leer_brujula", "BRUJULA:-1:4200:0")
        and "sureste (135 grados" in ks.formatear_sensor("leer_brujula", "BRUJULA:135:4200:1"),
        ks.formatear_sensor("leer_brujula", "BRUJULA:-1:4200:0"),
    )
    check(
        "el formato táctil distingue logo tocado de suelto",
        "tocado" in ks.formatear_sensor("leer_toque", "TOUCH:1:38")
        and "no tocado" in ks.formatear_sensor("leer_toque", "TOUCH:0:4"),
        ks.formatear_sensor("leer_toque", "TOUCH:1:38"),
    )

    original_leer_sensor = ks.serial_mgr.leer_sensor
    llamadas = []

    def sensor_falso(comando, prefijo, timeout=4.0):
        llamadas.append((comando, prefijo))
        return {
            "SENSOR:TEMP": "TEMP:23",
            "SENSOR:LUZ": "LUZ:120",
            "SENSOR:BOTON": "BOTON:1:0",
            "SENSOR:ACCEL": "ACCEL:0:1000:5:3:0",
            "SENSOR:MIC": "MIC:42:40:20:10:5:2:1",
            "SENSOR:BAT": "BAT:2980:3000:2",
            "SENSOR:GESTO": "GESTO:11:1450",
            "SENSOR:BRUJULA": "BRUJULA:135:4200:1",
            "SENSOR:TOQUE": "TOUCH:1:38",
        }.get(comando)

    try:
        ks.serial_mgr.leer_sensor = sensor_falso
        resultados = {
            nombre: ks.ejecutar_herramienta(nombre)
            for nombre in esperados
        }
    finally:
        ks.serial_mgr.leer_sensor = original_leer_sensor
    check(
        "los nueve sensores llegan por el mismo camino serial/BLE",
        llamadas == [(c, p) for c, p in esperados.values()],
        str(llamadas),
    )
    check("el resultado de sonido llega a la IA", resultados["leer_sonido"]["ok"] and "espectro" in resultados["leer_sonido"]["resultado"], resultados["leer_sonido"])
    check("el resultado de bateria llega a la IA", resultados["leer_bateria"]["ok"] and "2980 mV" in resultados["leer_bateria"]["resultado"], resultados["leer_bateria"])
    check("el resultado de gesto llega a la IA", resultados["leer_gesto"]["ok"] and "sacudida" in resultados["leer_gesto"]["resultado"], resultados["leer_gesto"])
    check("el resultado de brújula llega a la IA", resultados["leer_brujula"]["ok"] and "135 grados" in resultados["leer_brujula"]["resultado"], resultados["leer_brujula"])
    check("el resultado de toque llega a la IA", resultados["leer_toque"]["ok"] and "tocado" in resultados["leer_toque"]["resultado"], resultados["leer_toque"])

    def sensor_no_soportado(comando, prefijo, timeout=4.0):
        return "SENSOR:?"

    try:
        ks.serial_mgr.leer_sensor = sensor_no_soportado
        r_viejo = ks.ejecutar_herramienta("leer_sonido")
    finally:
        ks.serial_mgr.leer_sensor = original_leer_sensor
    check(
        "firmware viejo devuelve una explicacion, no un falso OK",
        r_viejo["ok"] is False and "flasheá" in r_viejo["resultado"],
        r_viejo,
    )


# ---------------------------------------------------------------------
#  ENDPOINT (stubs de IA: sin red)
# ---------------------------------------------------------------------
class _Stub:
    """Stub del stream de la IA: respuestas predeterminadas por ronda."""
    def __init__(self, respuestas: list[dict]):
        self.respuestas = respuestas
        self.rondas: list[list[str]] = []

    async def stream(self, messages, caja, temperatura):
        self.rondas.append([str(m.get("content", "")) for m in messages])
        i = min(len(self.rondas) - 1, len(self.respuestas) - 1)
        cuerpo = self.respuestas[i]
        caja["buffer"] = json.dumps(cuerpo, ensure_ascii=False)
        caja["salida"] = cuerpo
        return
        yield  # (hace que sea un async generator)


async def _correr(stub: _Stub, ejecutar=None, pj_extra=None, mensaje="hola"):
    """Corre el endpoint /api/chat/stream con stubs. Devuelve el crudo SSE."""
    originals = {
        "stream": ks._stream_ia,
        "recuerdos": ks.memoria_recuerdos_bloque,
        "memo": ks.memoria_memo_post_charla,
        "pj": ks.cargar_personaje,
        "ejecutar": ks.ejecutar_herramienta,
    }
    original_pj = originals["pj"]

    async def sin_recuerdos(*a, **k):
        return ""

    async def memo_sin_efecto(*a, **k):
        return None

    def pj_sin_voz(nombre):
        p = original_pj(nombre)
        p["voice_id"] = "PENDIENTE"   # sin TTS (no queremos red)
        return p

    ks._stream_ia = stub.stream
    ks.memoria_recuerdos_bloque = sin_recuerdos
    ks.memoria_memo_post_charla = memo_sin_efecto
    ks.cargar_personaje = pj_sin_voz
    if ejecutar is not None:
        ks.ejecutar_herramienta = ejecutar
    try:
        transport = httpx.ASGITransport(app=ks.app)
        async with httpx.AsyncClient(transport=transport, base_url="http://test") as cliente:
            async with cliente.stream(
                "POST", "/api/chat/stream",
                json={"personaje": "kira", "mensaje": mensaje, "historia": []},
            ) as r:
                return "".join([parte async for parte in r.aiter_text()])
    finally:
        ks._stream_ia = originals["stream"]
        ks.memoria_recuerdos_bloque = originals["recuerdos"]
        ks.memoria_memo_post_charla = originals["memo"]
        ks.cargar_personaje = originals["pj"]
        ks.ejecutar_herramienta = originals["ejecutar"]


def test_tools_nuevas():
    print("\n== 9) tools nuevas: dias_hasta, hora_en, juegos y la placa ausente ==")
    from datetime import date
    from app.services import mundo
    hoy = date(2026, 10, 18)
    r = mundo.dias_hasta("navidad", hoy)
    check("navidad desde el 18-oct-2026: 68 dias, viernes", "68 días" in r["resultado"] and "viernes" in r["resultado"],
          r["resultado"])
    r = mundo.dias_hasta("1/1", hoy)
    check("'1/1' ya paso este año -> el que viene (2027)", "2027" in r["resultado"], r["resultado"])
    check("hoy -> '¡Es hoy!'", mundo.dias_hasta("18/10", hoy)["resultado"].startswith("¡Es hoy!"))
    check("fecha imposible -> ok False", mundo.dias_hasta("31/02", hoy)["ok"] is False)
    check("basura -> pide el formato", mundo.dias_hasta("el finde", hoy)["ok"] is False)
    r = mundo.hora_en("Bogotá", "America/Bogota")
    check("hora_en misma zona -> 'la misma hora'", "la misma hora" in r["resultado"], r["resultado"])
    r = mundo.hora_en("Tokio", "Asia/Tokyo")
    check("hora_en Tokio -> 14 horas más", "14 horas más" in r["resultado"], r["resultado"])
    check("zona inexistente -> ok False", mundo.hora_en("X", "Marte/Olimpo")["ok"] is False)

    for frase, esperado in [("¿a qué hora se pone el sol hoy?", "sol_y_luna"), ("¿qué hora es en Tokio?", "hora_en"),
                            ("¿cómo está el clima en Bogotá?", "clima_en"), ("¿cuánto falta para navidad?", "dias_hasta"),
                            ("juguemos piedra papel o tijera", "jugar_en_placa"), ("tocame feliz cumpleaños", "tocar_melodia"),
                            ("¿por qué el sol es amarillo?", None), ("¿qué es una alarma de incendio?", None),
                            ("el clima en Pitalito", None), ("me toca ir al baño", None)]:
        check(f"pedido '{frase}' -> {esperado}", ks.accion_pedida(frase) == esperado, str(ks.accion_pedida(frase)))

    original = ks._placa_viva
    enviados = []
    original_enviar = ks.serial_mgr.enviar
    ks.serial_mgr.enviar = lambda cmd: enviados.append(cmd)
    try:
        ks._placa_viva = lambda: False
        for nombre, args in [("tocar_melodia", {"cancion": "estrellita"}), ("temporizador", {"segundos": 60}),
                             ("mostrar_en_leds", {"icono": "sol"}), ("jugar_en_placa", {"juego": "dado"}),
                             ("calibrar_brujula", {})]:
            r = ks.ejecutar_herramienta(nombre, args)
            check(f"sin placa, {nombre} NO dice que lo hizo", r["ok"] is False and "no está conectada" in r["resultado"],
                  r["resultado"][:80])
        check("sin placa no se manda nada", enviados == [], str(enviados))
        ks._placa_viva = lambda: True
        r = ks.ejecutar_herramienta("jugar_en_placa", {"juego": "piedra, papel o tijera"})
        check("con placa: ppt -> JUEGO:2", r["ok"] and enviados[-1:] == ["JUEGO:2"], str(enviados))
        r = ks.ejecutar_herramienta("jugar_en_placa", {"juego": "ajedrez"})
        check("juego que no existe -> lista los que hay", not r["ok"] and "dado" in r["resultado"])
        r = ks.ejecutar_herramienta("calibrar_brujula", {})
        check("calibrar -> CALIBRAR:BRUJULA", r["ok"] and enviados[-1:] == ["CALIBRAR:BRUJULA"], str(enviados))
    finally:
        ks._placa_viva = original
        ks.serial_mgr.enviar = original_enviar


async def test_multi_tool():
    print("\n== 5) ENDPOINT multi-tool: lista de 2 tools -> UN bloque consolidado ==")
    stub = _Stub([
        {"emotion": "neutral", "message": "",
         "tool": [{"nombre": "reloj"}, {"nombre": "calcular", "expresion": "2+2"}]},
        {"emotion": "happy", "message": "son las horas y cuatro en tu cuenta", "tool": None},
    ])
    crudo = await _correr(stub)   # tools REALES (reloj y calcular: sin red ni placa)

    check("ejecuto la ronda 1 con las 2 pedidas", len(stub.rondas) == 2, f"rondas={len(stub.rondas)}")
    check("se emitieron los 2 eventos tool",
          crudo.count('"tipo": "tool"') == 2, f"tools={crudo.count(chr(34) + 'tipo' + chr(34))}")
    bloque = stub.rondas[1][-1] if len(stub.rondas) > 1 else ""
    check("ronda 2 = UN bloque [RESULTADO DE HERRAMIENTA]", bloque.startswith("[RESULTADO DE HERRAMIENTA]"),
          bloque[:80])
    check("el bloque trae el resultado de reloj", "hora y fecha actual" in bloque, bloque[:200])
    check("el bloque trae el resultado de calcular", "2+2 = 4" in bloque, bloque[:300])
    check("avisa que llegaron juntas (no volver a pedir)",
          "VARIAS herramientas" in bloque and "NO vuelvas a pedir" in bloque, bloque[-200:])
    check("el system prompt trae el catálogo de tools",
          len(stub.rondas) > 0 and "Catálogo de herramientas" in stub.rondas[0][0],
          stub.rondas[0][0][-120:] if stub.rondas else "(sin rondas)")
    fin = [l for l in crudo.splitlines() if '"tipo": "fin"' in l]
    check("cerro con 'fin'", len(fin) == 1, crudo[:200])
    check("sin error", '"tipo": "error"' not in crudo, crudo[:300])


async def test_tope():
    print("\n== 6) ENDPOINT tope: la IA pide tool SIN PARAR -> corta en MAX_RONDAS_HERR ==")
    pide = lambda i: {"emotion": "neutral", "message": "",
                      "tool": {"nombre": "calcular", "expresion": f"2+{i}"}}  # distinta cada vez
    stub = _Stub([pide(i) for i in range(20)])   # siempre pide otra (caso patologico)
    llamadas: list[str] = []

    def tool_falsa(nombre, args=None):
        llamadas.append(nombre)
        return {"ok": True, "resultado": f"{args.get('expresion')} = 4"}
    crudo = await _correr(stub, ejecutar=tool_falsa)
    check(f"ejecuto exactamente {ks.MAX_RONDAS_HERR} veces (no infinito)",
          len(llamadas) == ks.MAX_RONDAS_HERR, f"llamadas={len(llamadas)}")
    check("hubo ronda forzada extra (1 + 3 + 1)", len(stub.rondas) == ks.MAX_RONDAS_HERR + 2,
          f"rondas={len(stub.rondas)}")
    check("la ronda forzada exige la respuesta y prohíbe otra tool",
          any("PROHIBIDO pedir otra herramienta" in m for m in stub.rondas[-1]),
          stub.rondas[-1][-200:] if stub.rondas else "")
    fin = [l for l in crudo.splitlines() if '"tipo": "fin"' in l]
    check("cerro con 'fin' (nunca loop ni mudo)", len(fin) == 1, f"fines={len(fin)}")
    check("el mensaje final es el de respaldo con el dato",
          bool(fin) and "Según mi cuenta" in fin[0], fin[0][:200] if fin else "(sin fin)")


async def test_dedup():
    print("\n== 7) ENDPOINT dedup: la misma llamada repetida NO se re-ejecuta ==")
    stub = _Stub([
        {"emotion": "neutral", "message": "", "tool": {"nombre": "reloj"}},
        {"emotion": "neutral", "message": "", "tool": {"nombre": "reloj"}},   # repite IDENTICA
        {"emotion": "happy", "message": "ya te di la hora", "tool": None},
    ])
    llamadas: list[str] = []

    def tool_falsa(nombre, args=None):
        llamadas.append(nombre)
        return {"ok": True, "resultado": "hora y fecha actual: 12:00 del lunes 1 de enero de 2026"}

    crudo = await _correr(stub, ejecutar=tool_falsa)
    check("la herramienta se ejecuto UNA sola vez", llamadas == ["reloj"], f"llamadas={llamadas}")
    check("igual avanzo de ronda (no se cuelga)", len(stub.rondas) == 3, f"rondas={len(stub.rondas)}")
    bloque = stub.rondas[2][-1] if len(stub.rondas) > 2 else ""
    check("el bloque le avisa que no la repita",
          "NO la repitas" in bloque, bloque[:250])
    check("y le re-muestra el resultado cacheado", "hora y fecha actual" in bloque, bloque[:250])
    fin = [l for l in crudo.splitlines() if '"tipo": "fin"' in l]
    check("cerro con 'fin'", len(fin) == 1, crudo[:200])


async def test_promesa():
    print("\n== 8) ENDPOINT: dice 'ahi va' / 'ya te busco' SIN la herramienta -> ronda de aviso ==")
    llamadas: list[str] = []

    def tool_falsa(nombre, args=None):
        llamadas.append(nombre)
        return {"ok": True, "resultado": "Sonando 'cumpleaños' por tu parlante."}

    stub = _Stub([
        {"emotion": "happy", "message": "Ahí va la melodía.", "tool": None},
        {"emotion": "happy", "message": "", "tool": {"nombre": "tocar_melodia", "cual": "cumpleaños"}},
        {"emotion": "happy", "message": "¡Sonando!", "tool": None},
    ])
    await _correr(stub, ejecutar=tool_falsa, mensaje="tocame feliz cumpleaños")
    check("pedido de melodia sin tool: se le avisa y la toca", llamadas == ["tocar_melodia"],
          f"llamadas={llamadas} rondas={len(stub.rondas)}")
    aviso = stub.rondas[1][-1] if len(stub.rondas) > 1 else ""
    check("el aviso nombra la herramienta", "tocar_melodia" in aviso, aviso[:120])

    llamadas.clear()
    stub = _Stub([
        {"emotion": "neutral", "message": "Dame un segundo que te averiguo.", "tool": None},
        {"emotion": "neutral", "message": "Edison murió un día como hoy.", "tool": None},
    ])
    await _correr(stub, ejecutar=tool_falsa, mensaje="¿qué pasó un día como hoy?")
    check("promesa de buscar: hay ronda de aviso", len(stub.rondas) == 2, f"rondas={len(stub.rondas)}")

    stub = _Stub([{"emotion": "happy", "message": "¡Hola! ¿Cómo te llamás?", "tool": None}])
    await _correr(stub, ejecutar=tool_falsa, mensaje="hola")
    check("charla normal: UNA sola ronda", len(stub.rondas) == 1, f"rondas={len(stub.rondas)}")


def main():
    global FALLOS
    solo_unitarios = "--solo-unitarios" in sys.argv
    print("=" * 62)
    print("TESTS DE HERRAMIENTAS — multi-tool, dedup, tope, catálogo")
    print("=" * 62)
    try:
        test_unitarios()
        test_tools_nuevas()
        if not solo_unitarios:
            asyncio.run(test_multi_tool())
            asyncio.run(test_tope())
            asyncio.run(test_dedup())
            asyncio.run(test_promesa())
        else:
            print("\n(integracion de endpoint salteada)")
    except Exception:
        traceback.print_exc()
        FALLOS += 1
    print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"FALLAS: {FALLOS} ❌"))
    sys.exit(1 if FALLOS else 0)


if __name__ == "__main__":
    main()
