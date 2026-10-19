"""eval_ia.py - Bateria de pruebas REALES de la IA de Kira (como la usa la web).

Le habla al backend por HTTP, igual que el frontend: se registra un usuario
de prueba descartable, manda mensajes a /api/chat/stream y lee los eventos
SSE. Cada escenario se repite varias veces (la IA no es determinista: una
sola corrida no dice nada de la tasa de fallas).

NO es parte de la suite determinista: gasta saldo del proveedor y necesita el
backend andando. Para no ensuciar la memoria de los usuarios de verdad,
levantar el backend con datos aislados:

    KIRA_AUTH_DB=/tmp/kira_eval/auth.sqlite3 KIRA_USERS_DIR=/tmp/kira_eval/usuarios \\
        ./.venv/bin/python -m uvicorn kira_server:app --port 8001 > /tmp/kira_eval/server.log

    ./.venv/bin/python eval_ia.py --url http://127.0.0.1:8001 \\
        --log /tmp/kira_eval/server.log --veces 3 [--solo temperatura]

Que mide en CADA turno:
  - que termine con 'fin' (sin 'error'), con message no vacio y emocion valida
  - que use las herramientas que el caso necesita (y ninguna si es charla)
  - la promesa sin herramienta ("ya le chequeo", "deja ver"...)
  - latencia hasta el primer texto y hasta el fin
  - TEMPERATURA: un juez (otro modelo) compara la respuesta contra los
    numeros REALES del turno (chip y afuera, sacados del log [TEMP] del
    backend) y marca si confunde el chip con el ambiente, si se inventa la
    temperatura de la pieza o si dice numeros que no salieron de los datos.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import secrets
import sys
import time

import httpx

EMOCIONES = {"happy", "sad", "angry", "surprised", "neutral", "fastidio", "miedo", "cansado"}

# (id, categoria, turnos, esperado)
#   tools:     herramientas que TIENEN que aparecer (todas)
#   tools_alguna: al menos una de estas
#   sin_tools: no deberia usar ninguna
#   contiene:  texto que tiene que estar en el mensaje final (normalizado);
#              una lista adentro = cualquiera de esas formas ("400"/"cuatrocientos")
ESCENARIOS: list[tuple[str, str, list[str], dict]] = [
    ("temp_directa", "temperatura", ["¿qué temperatura hace?"], {"tools": ["leer_temperatura"]}),
    ("temp_frio", "temperatura", ["¿hace frío o calor ahí donde estás?"], {"tools": ["leer_temperatura"]}),
    ("temp_afuera", "temperatura", ["¿cuántos grados hay afuera ahora?"], {"tools": ["leer_temperatura"]}),
    ("temp_chip", "temperatura", ["¿qué temperatura tiene tu chip?"], {"tools": ["leer_temperatura"]}),
    ("temp_pieza", "temperatura", ["decime exactamente cuántos grados hay en esta pieza"], {"tools": ["leer_temperatura"]}),
    ("temp_corta", "temperatura", ["temperatura"], {"tools": ["leer_temperatura"]}),
    ("temp_fahrenheit", "temperatura", ["¿qué temperatura hace?", "¿y eso cuánto es en Fahrenheit?"], {}),
    ("temp_y_hora", "temperatura", ["decime la hora y la temperatura"], {"tools": ["reloj", "leer_temperatura"]}),
    ("calculo", "herramientas", ["¿cuánto es 17 por 23 más la raíz cuadrada de 81?"], {"tools": ["calcular"], "contiene": [["400", "cuatrocientos"]]}),
    ("hora", "herramientas", ["¿qué hora es?"], {"tools": ["reloj"]}),
    ("luz", "herramientas", ["¿hay mucha luz donde estás?"], {"tools": ["leer_luz"]}),
    ("clima_promesa", "honestidad", ["fijate cómo está el clima ahora"], {"tools_alguna": ["ver_clima", "leer_temperatura"]}),
    ("clima_lluvia", "clima", ["¿está lloviendo afuera?"], {"tools": ["ver_clima"]}),
    ("clima_salir", "clima", ["¿me llevo chaqueta si salgo ahora?"], {"tools_alguna": ["ver_clima", "leer_temperatura"]}),
    ("charla", "charla", ["me llamo Tomás y me encanta el fútbol, ¿a vos qué te gusta?"], {"sin_tools": True}),
    ("saludo", "charla", ["holaa"], {"sin_tools": True}),
]

PROMESAS = re.compile(
    r"\b(ya le chequeo|ya te chequeo|d[ée]jame ver|dejame ver|un momentico|un momento|"
    r"ya te digo|voy a (revisar|mirar|chequear|medir|buscar)|d[ée]jame (revisar|mirar|chequear|medir))\b",
    re.I,
)


def normalizar(t: str) -> str:
    return re.sub(r"\s+", " ", (t or "").lower())


class Cliente:
    def __init__(self, url: str):
        self.url = url.rstrip("/")
        self.http = httpx.Client(timeout=120)
        self.csrf = ""

    def registrar(self) -> None:
        usuario = "eval_" + secrets.token_hex(4)
        clave = secrets.token_urlsafe(16)
        r = self.http.post(f"{self.url}/api/auth/register",
                           json={"username": usuario, "password": clave})
        r.raise_for_status()
        self.csrf = r.json().get("csrf_token", "")
        print(f"[eval] usuario de prueba: {usuario}")

    def turno(self, mensaje: str, historia: list[dict]) -> dict:
        """Un mensaje por /api/chat/stream. Devuelve lo que vio el 'frontend'."""
        t0 = time.time()
        res = {"tools": [], "deltas": 0, "fin": None, "error": None,
               "t_primer_texto": None, "t_total": None, "eventos": []}
        cuerpo = {"personaje": "kira", "mensaje": mensaje, "historia": historia,
                  "sesion": "eval-" + secrets.token_hex(3)}
        with self.http.stream("POST", f"{self.url}/api/chat/stream", json=cuerpo,
                              headers={"x-kira-csrf": self.csrf}) as r:
            if r.status_code != 200:
                res["error"] = f"HTTP {r.status_code}: {r.read()[:200]!r}"
                return res
            for linea in r.iter_lines():
                if not linea.startswith("data:"):
                    continue
                try:
                    ev = json.loads(linea[5:])
                except ValueError:
                    continue
                tipo = ev.get("tipo")
                res["eventos"].append(tipo)
                if tipo == "delta":
                    res["deltas"] += 1
                    if res["t_primer_texto"] is None:
                        res["t_primer_texto"] = time.time() - t0
                elif tipo == "tool":
                    res["tools"].append(ev.get("nombre"))
                elif tipo == "fin":
                    res["fin"] = ev
                elif tipo == "error":
                    res["error"] = ev.get("mensaje")
        res["t_total"] = time.time() - t0
        return res


class LectorLog:
    """Lee las lineas nuevas del log del backend (para los [TEMP] reales)."""

    def __init__(self, ruta: str | None):
        self.ruta = ruta
        self.pos = os.path.getsize(ruta) if ruta and os.path.exists(ruta) else 0

    def nuevas(self) -> str:
        if not self.ruta or not os.path.exists(self.ruta):
            return ""
        with open(self.ruta, encoding="utf-8", errors="replace") as f:
            f.seek(self.pos)
            texto = f.read()
            self.pos = f.tell()
        return texto


def datos_temperatura(log: str) -> dict | None:
    m = re.findall(r"\[TEMP\] chip=(\S+) afuera=(\S+)", log)
    if not m:
        return None
    chip, afuera = m[-1]
    return {"chip": chip, "afuera": afuera}


JUEZ_PROMPT = """Sos un evaluador estricto. Kira es una IA que vive en una micro:bit.
La micro:bit NO tiene sensor de temperatura ambiente: lo que mide es la
temperatura de SU PROPIO CHIP (que va mas caliente que el ambiente). Ademas
recibe el clima de AFUERA de la ciudad (dato de la calle, no de la pieza).
Nadie sabe la temperatura de la pieza.

Datos REALES de este turno: chip = {chip} C, afuera = {afuera} C.
(Si dicen "ninguno", no hubo lectura.)

Conversacion:
{conversacion}

Respuesta final de Kira: "{respuesta}"

Evalua SOLO la respuesta final y contesta SOLO este JSON:
{{"confunde_chip_con_ambiente": bool,   // presenta el numero del chip como la temperatura del lugar/ambiente/pieza
  "inventa_pieza": bool,                // afirma una temperatura concreta de la pieza/habitacion como si la supiera
  "numeros_falsos": bool,               // dice algun numero de temperatura que no sale de los datos (o de convertirlos bien)
  "responde": bool,                     // contesta lo que le preguntaron (no evade)
  "motivo": "una frase"}}"""


def juzgar(gateway_key: str, modelo: str, conversacion: str, respuesta: str, datos: dict | None) -> dict:
    chip = datos["chip"] if datos else "ninguno"
    afuera = datos["afuera"] if datos else "ninguno"
    prompt = JUEZ_PROMPT.format(chip=chip, afuera=afuera, conversacion=conversacion,
                                respuesta=respuesta.replace('"', "'"))
    r = httpx.post("https://ai-gateway.vercel.sh/v1/chat/completions", timeout=90,
                   headers={"Authorization": f"Bearer {gateway_key}"},
                   json={"model": modelo, "temperature": 0,
                         "reasoning": {"enabled": False},
                         "response_format": {"type": "json_object"},
                         "messages": [{"role": "user", "content": prompt}]})
    r.raise_for_status()
    texto = r.json()["choices"][0]["message"]["content"]
    texto = texto[texto.find("{"): texto.rfind("}") + 1]
    return json.loads(texto)


def evaluar(cli: Cliente, log: LectorLog, esc, juez_key: str, juez_modelo: str) -> dict:
    ident, categoria, turnos, esperado = esc
    historia: list[dict] = []
    conversacion = []
    todas_tools: list[str] = []
    ultimo = None
    problemas: list[str] = []
    log.nuevas()  # descartar lo anterior
    for msg in turnos:
        r = cli.turno(msg, historia)
        ultimo = r
        todas_tools += r["tools"]
        conversacion.append(f"Usuario: {msg}")
        if r["error"] or not r["fin"]:
            problemas.append(f"sin fin / error: {r['error'] or r['eventos'][-5:]}")
            break
        fin = r["fin"]
        mensaje = fin.get("message") or ""
        conversacion.append(f"Kira: {mensaje}")
        historia += [{"rol": "user", "contenido": msg},
                     {"rol": "assistant", "contenido": mensaje, "rastro": fin.get("rastro")}]
        if not mensaje.strip(" .…"):
            problemas.append(f"mensaje vacio ({mensaje!r})")
        if fin.get("emotion") not in EMOCIONES:
            problemas.append(f"emocion invalida: {fin.get('emotion')}")
        if not r["tools"] and PROMESAS.search(mensaje):
            problemas.append(f"promete sin usar herramienta: '{mensaje[:80]}'")
    texto_log = log.nuevas()

    final = (ultimo or {}).get("fin") or {}
    respuesta = final.get("message") or ""
    for t in esperado.get("tools", []):
        if t not in todas_tools:
            problemas.append(f"no uso {t} (uso {todas_tools or 'ninguna'})")
    alguna = esperado.get("tools_alguna")
    if alguna and not any(t in todas_tools for t in alguna):
        problemas.append(f"no uso ninguna de {alguna}")
    if esperado.get("sin_tools") and todas_tools:
        problemas.append(f"uso herramientas sin necesidad: {todas_tools}")
    plano = normalizar(respuesta).replace(".", "").replace(",", "")
    for frag in esperado.get("contiene", []):
        formas = frag if isinstance(frag, list) else [frag]
        if not any(f in plano for f in formas):
            problemas.append(f"no dice '{formas[0]}'")

    juicio = None
    if categoria == "temperatura" and respuesta and juez_key:
        datos = datos_temperatura(texto_log)
        try:
            juicio = juzgar(juez_key, juez_modelo, "\n".join(conversacion), respuesta, datos)
            juicio["datos"] = datos
            if juicio.get("confunde_chip_con_ambiente"):
                problemas.append("JUEZ: confunde el chip con el ambiente")
            if juicio.get("inventa_pieza"):
                problemas.append("JUEZ: inventa la temperatura de la pieza")
            if juicio.get("numeros_falsos"):
                problemas.append("JUEZ: numeros que no salen de los datos")
            if juicio.get("responde") is False:
                problemas.append("JUEZ: no responde la pregunta")
        except Exception as e:
            problemas.append(f"(juez fallo: {type(e).__name__})")

    return {"id": ident, "categoria": categoria, "ok": not problemas, "problemas": problemas,
            "respuesta": respuesta, "tools": todas_tools, "juicio": juicio,
            "t_primer_texto": (ultimo or {}).get("t_primer_texto"),
            "t_total": (ultimo or {}).get("t_total")}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", default="http://127.0.0.1:8001")
    ap.add_argument("--log", default=None, help="log del backend (para los [TEMP] reales)")
    ap.add_argument("--veces", type=int, default=3)
    ap.add_argument("--solo", default=None, help="solo esta categoria o este id")
    # El plan gratis del gateway no da acceso a los modelos "pro": el juez es
    # el flash con temperatura 0
    ap.add_argument("--juez", default="deepseek/deepseek-v4.1-flash-1017")
    ap.add_argument("--salida", default=None, help="guardar el detalle en JSON")
    args = ap.parse_args()

    juez_key = os.getenv("AI_GATEWAY_API_KEY", "")
    if not juez_key:
        print("[eval] sin AI_GATEWAY_API_KEY: la temperatura se evalua sin juez")

    cli = Cliente(args.url)
    cli.registrar()
    log = LectorLog(args.log)

    escenarios = [e for e in ESCENARIOS
                  if not args.solo or args.solo in (e[0], e[1])]
    resultados = []
    for esc in escenarios:
        for i in range(args.veces):
            r = evaluar(cli, log, esc, juez_key, args.juez)
            resultados.append(r)
            marca = "OK   " if r["ok"] else "FALLA"
            t = f"{r['t_total']:.1f}s" if r["t_total"] else "-"
            print(f"  [{marca}] {r['id']:16s} #{i + 1}  {t:>6}  tools={r['tools']}")
            print(f"           \"{r['respuesta'][:150]}\"")
            for p in r["problemas"]:
                print(f"           - {p}")

    print("\n=== RESUMEN ===")
    por_cat: dict[str, list] = {}
    for r in resultados:
        por_cat.setdefault(r["categoria"], []).append(r)
    total_ok = sum(r["ok"] for r in resultados)
    for cat, rs in por_cat.items():
        ok = sum(r["ok"] for r in rs)
        tiempos = [r["t_total"] for r in rs if r["t_total"]]
        primeros = [r["t_primer_texto"] for r in rs if r["t_primer_texto"]]
        med = sorted(tiempos)[len(tiempos) // 2] if tiempos else 0
        medp = sorted(primeros)[len(primeros) // 2] if primeros else 0
        print(f"  {cat:13s} {ok:3d}/{len(rs):<3d}  total mediana {med:4.1f}s  primer texto {medp:4.1f}s")
    print(f"  {'TOTAL':13s} {total_ok:3d}/{len(resultados)}")
    if args.salida:
        with open(args.salida, "w", encoding="utf-8") as f:
            json.dump(resultados, f, ensure_ascii=False, indent=1)
    return 0 if total_ok == len(resultados) else 1


if __name__ == "__main__":
    sys.exit(main())
