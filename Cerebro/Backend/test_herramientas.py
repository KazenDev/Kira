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
    seriales = ["leer_temperatura", "leer_luz", "leer_botones", "leer_movimiento", "controlar_metronomo"]
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
    cat = ks.catalogo_herramientas()
    check("lista TODAS las tools", all(f"- {n}:" in cat for n in ks.HERRAMIENTAS),
          str([n for n in ks.HERRAMIENTAS if f"- {n}:" not in cat]))
    check("dice cuando NO usar", "NO la uses" in cat, cat[:120])
    check("explica el formato lista (multi-tool)", "LISTA" in cat, cat[:120])
    check("habla del dedup", "no la repitas" in cat.lower(), cat[:120])


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


async def _correr(stub: _Stub, ejecutar=None, pj_extra=None):
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
                json={"personaje": "kira", "mensaje": "hola", "historia": []},
            ) as r:
                return "".join([parte async for parte in r.aiter_text()])
    finally:
        ks._stream_ia = originals["stream"]
        ks.memoria_recuerdos_bloque = originals["recuerdos"]
        ks.memoria_memo_post_charla = originals["memo"]
        ks.cargar_personaje = originals["pj"]
        ks.ejecutar_herramienta = originals["ejecutar"]


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


def main():
    global FALLOS
    solo_unitarios = "--solo-unitarios" in sys.argv
    print("=" * 62)
    print("TESTS DE HERRAMIENTAS — multi-tool, dedup, tope, catálogo")
    print("=" * 62)
    try:
        test_unitarios()
        if not solo_unitarios:
            asyncio.run(test_multi_tool())
            asyncio.run(test_tope())
            asyncio.run(test_dedup())
        else:
            print("\n(integracion de endpoint salteada)")
    except Exception:
        traceback.print_exc()
        FALLOS += 1
    print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"FALLAS: {FALLOS} ❌"))
    sys.exit(1 if FALLOS else 0)


if __name__ == "__main__":
    main()
