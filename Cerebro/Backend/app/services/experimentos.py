"""experimentos.py - Experimentos de feria CON el publico (puro: sin placa).

La idea viene de los robots guia de museo: lo que mas engancha no es que el
robot explique, es que proponga un RETO y la gente lo haga ("creating a
puzzle", Yamazaki et al. 2012). Kira propone, la persona hace (tapar sus
LEDs, gritar, sacudirla), la placa MIDE de verdad y queda un RECORD del dia
para que el siguiente lo quiera superar.

Aca vive solo la parte pura (que tipo es, como se lee cada linea, el
resumen con la ciencia y los records). La medicion con la placa esta en
kira_server._experimento.
"""
from __future__ import annotations

import json
import math
import os
from datetime import date

# tipo -> (comando, prefijo, icono en los LEDs mientras mide)
TIPOS = {
    "luz": ("SENSOR:LUZ", "LUZ:", "SOL"),
    "sonido": ("SENSOR:MIC", "MIC:", "MUSICA"),
    "fuerza": ("SENSOR:ACCEL", "ACCEL:", "RAYO"),
}

_ALIAS = {
    "luz": "luz", "tapar": "luz", "oscuridad": "luz", "sombra": "luz", "taparme": "luz",
    "sonido": "sonido", "grito": "sonido", "gritar": "sonido", "ruido": "sonido",
    "aplauso": "sonido", "aplaudir": "sonido", "soplar": "sonido", "voz": "sonido",
    "fuerza": "fuerza", "sacudir": "fuerza", "sacudida": "fuerza", "movimiento": "fuerza",
    "agitar": "fuerza", "temblor": "fuerza",
}

# lo que la persona tiene que hacer (va en el resultado, por si Kira lo repite)
INSTRUCCION = {
    "luz": "taparle los LEDs con la mano",
    "sonido": "hacer el ruido más fuerte que pueda (gritar, aplaudir, soplarle)",
    "fuerza": "sacudirla lo más fuerte que pueda (¡sin soltarla!)",
    "linterna": "alumbrarle los LEDs con la linterna del celular (estaba muy oscuro para taparla)",
}

# por debajo de esto no hay luz que tapar: el reto pasa a ser la linterna
LUZ_OSCURO = 25

# la ciencia de cada uno, para que Kira la cuente CORTO y a su manera
CIENCIA = {
    "luz": ("tus LEDs funcionan al revés: además de prender, la luz que les llega genera "
            "una corriente chiquitita, y midiendo esa corriente sabés cuánta luz hay"),
    "sonido": ("tu micrófono tiene una membrana diminuta que vibra con el aire; esa "
               "vibración se vuelve electricidad y de ahí sale el número"),
    "fuerza": ("tu acelerómetro mide la aceleración en g: quieta sentís 1 g, que es la "
               "gravedad; sacudirte suma aceleración en las tres direcciones"),
}
CIENCIA["linterna"] = CIENCIA["luz"]


def normalizar_tipo(texto: str) -> str | None:
    t = (texto or "").strip().lower()
    for palabra in t.replace(",", " ").split():
        if palabra in _ALIAS:
            return _ALIAS[palabra]
    return _ALIAS.get(t)


def valor_de(tipo: str, linea: str | None) -> float | None:
    """Un numero por lectura: luz 0-255, sonido 0-100, fuerza en mili-g
    (la magnitud del vector: quieta da ~1000)."""
    if not linea:
        return None
    _, prefijo, _ = TIPOS[tipo]
    if not linea.startswith(prefijo):
        return None
    partes = linea[len(prefijo):].split(":")
    try:
        if tipo == "fuerza":
            x, y, z = (float(p) for p in partes[:3])
            return math.sqrt(x * x + y * y + z * z)
        return float(partes[0])
    except (ValueError, IndexError):
        return None


def leer_records(ruta: str) -> dict:
    try:
        with open(ruta, encoding="utf-8") as f:
            datos = json.load(f)
        return datos if isinstance(datos, dict) else {}
    except (OSError, ValueError):
        return {}


def guardar_records(ruta: str, records: dict) -> None:
    os.makedirs(os.path.dirname(ruta) or ".", exist_ok=True)
    tmp = ruta + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(records, f, ensure_ascii=False, indent=1)
    os.replace(tmp, ruta)


def _puntaje(tipo: str, base: float, muestras: list[float]) -> float:
    """Lo que compite: cuanto LOGRO la persona (mas es mejor en los tres)."""
    if tipo == "linterna":
        return max(muestras) - base
    if tipo == "luz":
        # porcentaje de luz que le sacaron
        return 0.0 if base <= 0 else max(0.0, (base - min(muestras)) / base * 100)
    if tipo == "sonido":
        # el SEGUNDO más alto: un grito dura varias lecturas y un "tac" suelto
        # (un golpe en la mesa, un pico del micrófono) no gana el récord
        orden = sorted(muestras)
        return orden[-2] if len(orden) >= 2 else orden[-1]
    # fuerza: la intensidad PROMEDIO de la sacudida (el pico satura en ~2 g por
    # eje y daria empates; sacudir fuerte todo el rato es lo que gana)
    return sum(abs(m - 1000) for m in muestras) / len(muestras) / 1000


def resumir(tipo: str, base: float | None, muestras: list[float], records: dict,
            quien: str = "", hoy: date | None = None) -> tuple[dict, dict]:
    """(resultado para la IA, records actualizados)."""
    hoy = hoy or date.today()
    if not muestras or base is None:
        return ({"ok": False, "resultado": (
            "La placa no me devolvió lecturas a tiempo (¿se desconectó?). Decilo simple y "
            "proponé repetirlo.")}, records)

    if tipo == "luz" and base < LUZ_OSCURO:
        tipo = "linterna"
    puntaje = round(_puntaje(tipo, base, muestras), 2 if tipo == "fuerza" else 0)
    if tipo == "linterna":
        medicion = (f"estaba casi oscuro ({base:.0f}/255) y con la luz llegó a "
                    f"{max(muestras):.0f}/255: subió {puntaje:.0f} puntos")
        unidad = " puntos de luz"
    elif tipo == "luz":
        medicion = (f"antes había {base:.0f}/255 de luz y cuando te taparon bajó a "
                    f"{min(muestras):.0f}/255: te sacaron el {puntaje:.0f}% de la luz")
        unidad = "% de oscuridad"
    elif tipo == "sonido":
        medicion = (f"el silencio de base era {base:.0f}/100 y el grito sostenido llegó a "
                    f"{puntaje:.0f}/100")
        unidad = "/100 de ruido"
    else:
        medicion = (f"quieta sentís 1 g; la sacudida llegó a un pico de "
                    f"{max(muestras) / 1000:.1f} g, con una intensidad promedio de "
                    f"{puntaje:.2f} g por encima de la gravedad")
        unidad = " g de intensidad"

    hubo_cambio = (tipo == "luz" and puntaje >= 30) or (tipo == "linterna" and puntaje >= 40) or (tipo == "sonido" and puntaje - base >= 20) \
        or (tipo == "fuerza" and puntaje >= 0.15)
    if not hubo_cambio:
        # un intento en el que no pasó nada no compite (ni pisa el récord)
        return ({"ok": True, "puntaje": puntaje, "resultado": (
            f"Experimento de {tipo} ({INSTRUCCION[tipo]}): {medicion}. Casi no cambió nada: "
            "capaz no lo hicieron a tiempo o muy suave. Decilo con humor y proponé repetirlo.")},
                records)

    # records POR DIA: cada jornada de feria arranca de cero
    dia = hoy.isoformat()
    previo = records.get(tipo) if isinstance(records.get(tipo), dict) else None
    if previo and previo.get("fecha") != dia:
        previo = None
    nuevos = dict(records)
    if previo is None:
        aviso = "es el PRIMER intento de hoy: ese número queda como récord del día"
        nuevos[tipo] = {"valor": puntaje, "fecha": dia, "quien": quien}
    elif puntaje > float(previo.get("valor", 0)):
        antes = previo.get("quien") or "alguien"
        aviso = (f"¡NUEVO RÉCORD del día! Le ganó a {antes}, que tenía "
                 f"{previo['valor']}{unidad}")
        nuevos[tipo] = {"valor": puntaje, "fecha": dia, "quien": quien}
    else:
        dueno = previo.get("quien") or "alguien"
        aviso = (f"no alcanzó el récord del día: {previo['valor']}{unidad}, de {dueno}. "
                 "Retalo a intentarlo otra vez")

    resultado = (f"Experimento de {tipo} ({INSTRUCCION[tipo]}): {medicion}. {aviso}. "
                 f"La ciencia, para contarla corto y a tu manera: {CIENCIA[tipo]}.")
    return {"ok": True, "resultado": resultado, "puntaje": puntaje}, nuevos


_NOMBRE_RECORD = {"luz": "oscuridad (taparte)", "linterna": "linterna", "sonido": "grito",
                  "fuerza": "sacudida"}
_UNIDAD_RECORD = {"luz": "%", "linterna": " puntos de luz", "sonido": "/100", "fuerza": " g"}


def records_del_dia(records: dict, hoy: date | None = None) -> str:
    """Los récords de HOY en una línea, para que Kira los nombre con el dato
    real (y no invente "nadie me lo rompe todavía")."""
    dia = (hoy or date.today()).isoformat()
    partes = []
    for tipo, nombre in _NOMBRE_RECORD.items():
        r = records.get(tipo)
        if isinstance(r, dict) and r.get("fecha") == dia:
            quien = f", de {r['quien']}" if r.get("quien") else ""
            partes.append(f"{nombre} {r.get('valor')}{_UNIDAD_RECORD[tipo]}{quien}")
    if not partes:
        return "hoy todavía nadie hizo un experimento: el primero que lo intente pone el récord"
    return "; ".join(partes)
