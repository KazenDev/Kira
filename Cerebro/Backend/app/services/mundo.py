"""mundo.py - Lo que Kira puede SABER del mundo real (gratis, sin keys).

Fuentes elegidas por ser utiles en una feria de ciencia en Pitalito y por no
necesitar registro (probadas el 18-oct-2026 desde la PC de la feria):

  - Open-Meteo forecast: pronostico por horas, viento con direccion, UV,
    amanecer/atardecer y fase de la luna.
  - Open-Meteo air quality: calidad del aire (modelo, no estacion local).
  - USGS: sismos recientes cerca (el Huila es zona sismica).
  - Wikipedia en espanol: resumen de un tema y "un dia como hoy" (sirve de
    reemplazo de la busqueda web cuando no hay key de Exa).
  - wheretheiss.at: donde esta la Estacion Espacial Internacional.

Cada funcion devuelve {"ok": bool, "resultado": str} en lenguaje natural,
listo para la IA, y NUNCA lanza: si una API se cae, Kira lo dice simple.
"""
from __future__ import annotations

import math
import time
from datetime import datetime, timezone

import httpx
from urllib.parse import quote

UA = {"User-Agent": "KiraFeriaCiencias/1.0 (micro:bit; Pitalito, Huila)"}
_cache: dict[str, tuple[float, object]] = {}


def _get_json(url: str, ttl: float, timeout: float = 5.0):
    ahora = time.time()
    hit = _cache.get(url)
    if hit and ahora - hit[0] < ttl:
        return hit[1]
    r = httpx.get(url, timeout=timeout, headers=UA, follow_redirects=True)
    r.raise_for_status()
    data = r.json()
    _cache[url] = (ahora, data)
    return data


def _falla(que: str) -> dict:
    return {"ok": False, "resultado": f"No pude consultar {que} ahora (sin conexión con el servicio). Decilo simple, no lo inventes."}


# --- Puntos cardinales y luna ------------------------------------------------

_CARDINALES = ["norte", "noreste", "este", "sureste", "sur", "suroeste", "oeste", "noroeste"]


def cardinal(grados: float) -> str:
    return _CARDINALES[int(((grados % 360) + 22.5) // 45) % 8]


def viento_relativo(viento_desde: float, rumbo_kira: float) -> str:
    """De donde le viene el viento a Kira, segun hacia donde mira ella."""
    rel = (viento_desde - rumbo_kira) % 360
    if rel < 45 or rel >= 315:
        return "de frente"
    if rel < 135:
        return "de tu derecha"
    if rel < 225:
        return "de atrás"
    return "de tu izquierda"


def fase_luna(f: float) -> str:
    # moon_phase de Open-Meteo: 0 nueva, 0,25 cuarto creciente, 0,5 llena,
    # 0,75 cuarto menguante
    f = f % 1.0
    if f < 0.03 or f > 0.97:
        return "luna nueva"
    if f < 0.22:
        return "luna creciente"
    if f < 0.28:
        return "cuarto creciente"
    if f < 0.47:
        return "luna gibosa creciente"
    if f < 0.53:
        return "luna llena"
    if f < 0.72:
        return "luna gibosa menguante"
    if f < 0.78:
        return "cuarto menguante"
    return "luna menguante"


def _hora(iso: str) -> str:
    try:
        return datetime.fromisoformat(iso).strftime("%H:%M")
    except (TypeError, ValueError):
        return str(iso)


# --- Pronostico ----------------------------------------------------------------

def pronostico(lat: float, lon: float, lugar: str, rumbo_kira: float | None = None) -> dict:
    url = (
        "https://api.open-meteo.com/v1/forecast"
        f"?latitude={lat}&longitude={lon}&timezone=auto"
        "&current=wind_direction_10m,wind_speed_10m,wind_gusts_10m,uv_index,is_day"
        "&hourly=precipitation_probability,temperature_2m&forecast_hours=6"
        "&daily=sunrise,sunset,uv_index_max,precipitation_probability_max,moon_phase"
        "&forecast_days=1"
    )
    try:
        d = _get_json(url, ttl=900)
    except Exception:
        return _falla("el pronóstico")
    partes = []
    cur = d.get("current") or {}
    if cur.get("wind_direction_10m") is not None:
        desde = float(cur["wind_direction_10m"])
        viento = (f"viento del {cardinal(desde)} a {round(cur.get('wind_speed_10m') or 0)} km/h"
                  f" (ráfagas de {round(cur.get('wind_gusts_10m') or 0)})")
        if rumbo_kira is not None:
            viento += f": a vos te viene {viento_relativo(desde, rumbo_kira)}"
        partes.append(viento)
    if cur.get("uv_index") is not None and cur.get("is_day"):
        partes.append(f"índice UV ahora {round(cur['uv_index'], 1)}")
    hor = d.get("hourly") or {}
    probs = hor.get("precipitation_probability") or []
    temps = hor.get("temperature_2m") or []
    if probs:
        maxp = max(probs)
        hora_max = _hora((hor.get("time") or [""])[probs.index(maxp)])
        partes.append(f"probabilidad de lluvia en las próximas 6 horas: hasta {maxp}% (cerca de las {hora_max})")
    if temps:
        partes.append(f"temperatura las próximas horas entre {round(min(temps))} y {round(max(temps))} grados")
    dia = d.get("daily") or {}
    if dia.get("sunrise"):
        partes.append(f"hoy el sol sale a las {_hora(dia['sunrise'][0])} y se pone a las {_hora(dia['sunset'][0])}")
    if dia.get("uv_index_max"):
        partes.append(f"UV máximo del día {round(dia['uv_index_max'][0], 1)}")
    if dia.get("moon_phase"):
        partes.append(f"la luna está en {fase_luna(float(dia['moon_phase'][0]))}")
    if not partes:
        return _falla("el pronóstico")
    return {"ok": True, "resultado": f"Pronóstico para {lugar}: " + "; ".join(partes) + "."}


def calidad_aire(lat: float, lon: float, lugar: str) -> dict:
    url = ("https://air-quality-api.open-meteo.com/v1/air-quality"
           f"?latitude={lat}&longitude={lon}&current=pm2_5,us_aqi&timezone=auto")
    try:
        d = _get_json(url, ttl=1800)
        cur = d.get("current") or {}
        aqi = cur.get("us_aqi")
        pm = cur.get("pm2_5")
    except Exception:
        return _falla("la calidad del aire")
    if aqi is None:
        return _falla("la calidad del aire")
    nivel = ("buena" if aqi <= 50 else "moderada" if aqi <= 100 else
             "mala para personas sensibles" if aqi <= 150 else "mala")
    return {"ok": True, "resultado": (
        f"Calidad del aire en {lugar}: {nivel} (índice {round(aqi)}, partículas finas "
        f"{round(pm or 0, 1)} microgramos por metro cúbico). Es un modelo, no un sensor local.")}


# --- Sismos ----------------------------------------------------------------------

def sismos_cercanos(lat: float, lon: float, lugar: str, radio_km: int = 300) -> dict:
    url = ("https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson"
           f"&latitude={lat}&longitude={lon}&maxradiuskm={radio_km}"
           "&minmagnitude=2.5&limit=3&orderby=time")
    try:
        d = _get_json(url, ttl=300, timeout=6.0)
    except Exception:
        return _falla("los sismos")
    feats = d.get("features") or []
    if not feats:
        return {"ok": True, "resultado": f"No hubo sismos registrados de magnitud 2,5 o más a menos de {radio_km} km de {lugar} últimamente."}
    lineas = []
    ahora = datetime.now(timezone.utc)
    for f in feats:
        p = f.get("properties") or {}
        coords = (f.get("geometry") or {}).get("coordinates") or [None, None, None]
        cuando = datetime.fromtimestamp((p.get("time") or 0) / 1000, tz=timezone.utc)
        horas = (ahora - cuando).total_seconds() / 3600
        hace = f"hace {round(horas)} horas" if horas < 48 else f"hace {round(horas / 24)} días"
        dist = _haversine(lat, lon, coords[1], coords[0]) if coords[1] is not None else None
        donde = f"a unos {round(dist)} km" if dist is not None else ""
        prof = ""
        if coords[2] is not None:
            # dicho en criollo: el modelo confunde "10 km" con "hondo"
            tipo = "superficial" if coords[2] < 70 else "intermedio" if coords[2] < 300 else "profundo"
            prof = f", {tipo} ({round(coords[2])} km de profundidad)"
        lineas.append(f"magnitud {p.get('mag')} {hace} {donde}{prof} ({p.get('place', '')})")
    return {"ok": True, "resultado": (
        f"Sismos recientes cerca de {lugar} (USGS): " + "; ".join(lineas)
        + ". Contalo con calma, sin asustar: los sismos chicos son comunes en el Huila.")}


def _haversine(lat1, lon1, lat2, lon2) -> float:
    r = 6371.0
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dp, dl = math.radians(lat2 - lat1), math.radians(lon2 - lon1)
    a = math.sin(dp / 2) ** 2 + math.cos(p1) * math.cos(p2) * math.sin(dl / 2) ** 2
    return 2 * r * math.asin(math.sqrt(a))


def rumbo_hacia(lat1, lon1, lat2, lon2) -> float:
    """Rumbo (grados desde el norte) para ir de 1 a 2."""
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dl = math.radians(lon2 - lon1)
    x = math.sin(dl) * math.cos(p2)
    y = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
    return (math.degrees(math.atan2(x, y)) + 360) % 360


# --- Wikipedia ---------------------------------------------------------------------

def wikipedia(tema: str) -> dict:
    tema = (tema or "").strip()
    if not tema:
        return {"ok": False, "resultado": "Falta el tema a buscar (arg 'tema')."}
    titulo = tema.replace(" ", "_")
    try:
        d = _get_json(f"https://es.wikipedia.org/api/rest_v1/page/summary/{quote(titulo)}",
                      ttl=86400)
    except Exception:
        # El titulo exacto no existe: se busca el mas parecido
        try:
            s = _get_json("https://es.wikipedia.org/w/api.php?action=opensearch&limit=1&format=json"
                          f"&search={quote(tema)}", ttl=86400)
            if not s or not s[1]:
                return {"ok": False, "resultado": f"No encontré nada sobre '{tema}' en Wikipedia."}
            d = _get_json("https://es.wikipedia.org/api/rest_v1/page/summary/"
                          + quote(s[1][0].replace(" ", "_")), ttl=86400)
        except Exception:
            return _falla("Wikipedia")
    extracto = (d.get("extract") or "").strip()
    if not extracto:
        return {"ok": False, "resultado": f"No encontré un resumen de '{tema}' en Wikipedia."}
    if len(extracto) > 900:
        extracto = extracto[:900].rsplit(". ", 1)[0] + "."
    return {"ok": True, "resultado": f"Wikipedia, '{d.get('title', tema)}': {extracto}",
            "fuente": f"https://es.wikipedia.org/wiki/{(d.get('title') or tema).replace(' ', '_')}"}


def un_dia_como_hoy(cuantos: int = 3) -> dict:
    hoy = datetime.now()
    url = f"https://api.wikimedia.org/feed/v1/wikipedia/es/onthisday/selected/{hoy.month:02d}/{hoy.day:02d}"
    try:
        d = _get_json(url, ttl=21600)
    except Exception:
        return _falla("las efemérides")
    eventos = d.get("selected") or []
    if not eventos:
        return _falla("las efemérides")
    elegidos = eventos[:cuantos]
    lineas = [f"en {e.get('year')}: {e.get('text', '').strip().rstrip('.')}" for e in elegidos]
    return {"ok": True, "resultado": "Un día como hoy (Wikipedia): " + "; ".join(lineas) + "."}


# --- Estacion Espacial ---------------------------------------------------------------

def estacion_espacial(lat: float, lon: float, lugar: str) -> dict:
    try:
        d = _get_json("https://api.wheretheiss.at/v1/satellites/25544", ttl=20)
    except Exception:
        return _falla("la Estación Espacial")
    dist = _haversine(lat, lon, d["latitude"], d["longitude"])
    rumbo = rumbo_hacia(lat, lon, d["latitude"], d["longitude"])
    return {"ok": True, "resultado": (
        f"La Estación Espacial Internacional está ahora a {round(d.get('altitude', 0))} km de "
        f"altura, volando a {round(d.get('velocity', 0))} km/h, sobre un punto a unos "
        f"{round(dist)} km de {lugar} hacia el {cardinal(rumbo)}. "
        f"Ahora le da {'el sol' if d.get('visibility') == 'daylight' else 'la sombra de la Tierra'}.")}


# --- Lugares (geocodificacion) -----------------------------------------------------

def ubicar(lugar: str, cerca: tuple[float, float] | None = None) -> dict | None:
    """Coordenadas de un lugar por nombre (Open-Meteo geocoding, gratis).

    Primero busca EN COLOMBIA y se queda con el mas poblado (la capital le gana
    a los pueblitos que se llaman igual); si en Colombia no hay nada, el mas
    poblado del mundo. Sin el filtro, "San Agustin" daba Florida (el parque
    arqueologico del Huila ni aparecia entre los 10 primeros) y "el mas
    cercano" elegia un caserio llamado Bogota en el Huila.
    """
    lugar = (lugar or "").strip()
    if not lugar:
        return None
    base = ("https://geocoding-api.open-meteo.com/v1/search?count=10&language=es&format=json"
            f"&name={quote(lugar)}")
    def buscar(url):
        try:
            return (_get_json(url, ttl=86400) or {}).get("results") or []
        except Exception:
            return []

    def peso(x):
        pob = x.get("population") or 0
        dist = _haversine(cerca[0], cerca[1], x["latitude"], x["longitude"]) if cerca else 0
        return (pob, -dist)

    en_colombia = buscar(base + "&countryCode=CO")
    en_el_mundo = buscar(base)
    if not en_colombia and not en_el_mundo:
        return None
    r = max(en_colombia, key=peso) if en_colombia else max(en_el_mundo, key=peso)
    if en_colombia and en_el_mundo:
        # "Paris" es Francia aunque haya un caserio Paris en Antioquia: gana el
        # del mundo solo si es MUCHISIMO mas grande (50x)
        famoso = max(en_el_mundo, key=peso)
        if (famoso.get("population") or 0) > 50 * max(r.get("population") or 0, 1000):
            r = famoso
    nombre = ", ".join(x for x in (r.get("name"), r.get("admin1"), r.get("country")) if x)
    return {"lat": r["latitude"], "lon": r["longitude"], "nombre": nombre}


def donde_queda(lat: float, lon: float, lugar: str, rumbo_kira: float | None = None) -> dict:
    destino = ubicar(lugar, cerca=(lat, lon))
    if not destino:
        return {"ok": False, "resultado": f"No encontré dónde queda '{lugar}'."}
    dist = _haversine(lat, lon, destino["lat"], destino["lon"])
    rumbo = rumbo_hacia(lat, lon, destino["lat"], destino["lon"])
    texto = (f"{destino['nombre']} queda a unos {round(dist)} km en línea recta, hacia el "
             f"{cardinal(rumbo)} ({round(rumbo)} grados desde el norte).")
    if rumbo_kira is not None:
        rel = (rumbo - rumbo_kira) % 360
        lado = ("justo adelante tuyo" if rel < 23 or rel >= 337 else
                "adelante a tu derecha" if rel < 68 else "a tu derecha" if rel < 113 else
                "atrás a tu derecha" if rel < 158 else "detrás tuyo" if rel < 203 else
                "atrás a tu izquierda" if rel < 248 else "a tu izquierda" if rel < 293 else
                "adelante a tu izquierda")
        texto += f" Según tu brújula, te queda {lado}."
    return {"ok": True, "resultado": texto, "rumbo": rumbo}
