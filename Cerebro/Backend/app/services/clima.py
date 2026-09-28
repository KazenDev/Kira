"""
clima.py - La temperatura DE AFUERA, para que la de la micro:bit tenga con qué compararse.

POR QUE EXISTE ESTE SERVICIO
=============================

La micro:bit NO tiene sensor de temperatura ambiente. Lo que tiene es el
termometro interno del SoC nRF52833, que mide el SILICIO del chip, no el aire
(el datasheet dice textualmente "the temperature reading is not representative
of the ambient temperature, but rather the temperature relative to the surface
temperature of the chip", con una precision de +/-5 C SIN trimear).

Medido en la placa: el termometro es estable, no deriva (25 C clavados, tanto
en reposo como con la lluvia a 60 fps). El error es un offset FIJO de esa
placa. Peromedir cual es el offset exige un termometro de referencia, que
nadie tiene en la mesa.

ESTO NO CALIBRA. Solo le da a la IA el segundo numero, el de afuera, para que
pueda razonar sobre la diferencia en vez de afirmar con seguridad un numero del
que no sabe. Antes la tool decia "Lee la temperatura AMBIENTE" y la IA
respondia "hace 25 grados" creyendolo, y Kira lo escribia en su diario como
si fuera verdad. Ahora la IA sabe que 25 es su propio cuerpo de silicio y que
afuera hay otra cosa.

Y de paso deja datos: cada par (chip, afuera) queda en el log, que con una
semana de muestras YA SI permite calcular el offset real con evidencia, en
lugar de con una intuicion. Ese es el paso siguiente, y es gratis.

POR QUE NO SE CORRIGE EL NUMERO ACA
===================================
Porque "corregirlo" necesita saber el error, y el error sale de medir la
temperatura de la PIEZA, que no es la de afuera. Si la pieza esta mas
caliente que la calle (calefaccion encendida, que en Pitalito a las 8 de la
noche es lo normal), un "promedio" entre chip y calle se equivoca por varios
grados. Y un error inventado, peor que un error medido: queda escrito en el
firmware para siempre.

Open-Meteo: gratis, sin API key, sin registro. Cacheado porque el clima no
cambia cada minuto y la tool se puede llamar seguido.
"""
from __future__ import annotations

import logging
import time

import httpx

log = logging.getLogger(__name__)

# Duracion del cache. El clima no cambia cada minuto y la tool se puede
# llamar varias veces seguidas (la IA reintenta si no le cierra el formato).
CACHE_SEGUNDOS = 900  # 15 min

# Origen de la muestra: de donde sale el numero, para que la IA no lo
# presente como si estuviera midiendo la pieza.
ORIGEN = "dato del clima de Pitalito, Huila; NO es la temperatura de la pieza"

_cache: dict = {"valor": None, "vence": 0.0}


def _redondea(v: float) -> int:
    return int(round(v))


def temperatura_externa(
    lat: float,
    lon: float,
    timeout: float = 4.0,
    cache_s: int = CACHE_SEGUNDOS,
) -> dict | None:
    """
    Temperatura del aire AFUERA del punto indicado.

    Devuelve {"temp": int, "sensacion": int, "hora": str} o None si la API no
    respondio. None es un caso NORMAL, no una excepcion: el backend tiene que
    poder seguir operando sin clima, y la IA simplemente no recibe el segundo
    numero (que es como venia funcionando todo este tiempo).

    El cache es por proceso. No hay redis ni disco para esto: son 15 minutos
    de un dato que pesa 40 bytes.
    """
    ahora = time.time()
    if _cache["valor"] is not None and ahora < _cache["vence"]:
        return _cache["valor"]

    url = (
        "https://api.open-meteo.com/v1/forecast"
        f"?latitude={lat}&longitude={lon}"
        "&current=temperature_2m,apparent_temperature"
        "&timezone=auto"
    )
    try:
        r = httpx.get(url, timeout=timeout)
        r.raise_for_status()
        actual = r.json().get("current") or {}
        temp = actual.get("temperature_2m")
        if temp is None:
            return None
        sens = actual.get("apparent_temperature")
        valor = {
            "temp": _redondea(float(temp)),
            "sensacion": _redondea(float(sens)) if sens is not None else None,
            "hora": str(actual.get("time", "")),
        }
    except Exception as exc:  # httpx, json, timeout... cualquier cosa
        # No se propaga: el clima es un extra, no un requisito. Si Open-Meteo
        # se cae, Kira sigue funcionando con el numero de siempre.
        log.info("[CLIMA] no se pudo leer el clima: %s", exc)
        return None

    _cache["valor"] = valor
    _cache["vence"] = ahora + cache_s
    return valor
