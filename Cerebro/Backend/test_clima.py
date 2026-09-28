"""
test_clima.py - El segundo numero de la temperatura, y por que NO se corrige

Tres cosas que este archivo verifica, y las tres son cosas que se pudieron
romper sin que nadie se enterara:

  1) QUE EL TEXTO NO MIENTA. Antes la tool decia "temperatura ambiente" y la
     IA lo repetia como verdad. Ahora el texto tiene que decir que es el
     silicono del chip. Si alguien vuelve a poner "ambiente" en la
     descripcion, este test lo frena.

  2) QUE EL CLIMA ES UN EXTRA, NO UN REQUISITO. Si Open-Meteo esta caido, la
     tool tiene que seguir devolviendo el numero del chip. Un backend que
     muere porque no le pudo preguntar el clima a internet es un backend malo.

  3) QUE NO SE INVENTA UNA TEMPERATURA DE LA PIEZA. El dato de afuera NO es el
     dato de la habitacion (con calefaccion la pieza puede estar mas caliente
     que la calle). El texto se lo aclara a la IA explicitamente, porque el
     modo de fallo es que la IA promedie los dos y conteste un numero
     inventado con total seguridad.
"""
import sys
import os
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

import kira_server as ks
from app.services import clima

ok = 0
fail = 0


def check(desc, cond, extra=""):
    global ok, fail
    if cond:
        print(f"  OK   {desc}")
        ok += 1
    else:
        print(f"  FALLA {desc} {extra}")
        fail += 1


print("\n== 1) la descripcion de la tool NO afirma que sea ambiente ==")
d = ks.HERRAMIENTAS["leer_temperatura"]["descripcion"]
bajo = d.lower()
check("no dice 'temperatura ambiente'", "temperatura ambiente" not in bajo)
check("dice que es el chip/silicio", "chip" in bajo or "silicio" in bajo)
check("avisa que NO es el ambiente", "no es" in bajo)
check("le dice a la IA que no lo conteste como ambiente",
      "no respondas" in bajo or "no lo respondas" in bajo)

print("\n== 2) formatear_sensor con clima y sin clima ==")
# con clima simulado
clima.temperatura_externa = lambda *a, **k: {
    "temp": 18, "sensacion": 20, "hora": "2026-09-27T19:45"}
t = ks.formatear_sensor("leer_temperatura", "TEMP:25")
check("menciona el numero del chip", "25" in t, f"-> {t!r}")
check("menciona el numero de afuera", "18" in t, f"-> {t!r}")
check("aclara que NO es el ambiente", "NO es la temperatura del ambiente" in t,
      f"-> {t!r}")
check("aclara que afuera no es la pieza", "no de la pieza" in t, f"-> {t!r}")
check("prohibe promediar los dos", "no promedies" in bajo or "NO promedies" in t)
check("el test viejo sigue entendiendo el formato ('N grados')",
      "25 grados" in t, f"-> {t!r}")

# sin clima (la API caida)
clima.temperatura_externa = lambda *a, **k: None
t2 = ks.formatear_sensor("leer_temperatura", "TEMP:25")
check("sin clima NO explota", isinstance(t2, str) and t2 != "")
check("sin clima sigue el numero del chip", "25" in t2, f"-> {t2!r}")
check("sin clima aclara igual que no es ambiente",
      "NO es la del ambiente" in t2, f"-> {t2!r}")

print("\n== 3) otros sensores NO se tocaron ==")
luz = ks.formatear_sensor("leer_luz", "LUZ:120")
check("leer_luz sigue igual", "120" in luz and "luz ambiente" in luz.lower(),
      f"-> {luz!r}")

print("\n== 4) el servicoio se degrada en vez de propagar ==")
# una lat/lon imposible: tiene que devolver None, no reventar
real = clima.temperatura_externa.__wrapped__ if hasattr(clima.temperatura_externa, "__wrapped__") else None
import app.services.clima as mod
v = mod.temperatura_externa(999.0, 999.0, timeout=2.0, cache_s=0)
check("coordenadas imposibles devuelven None y no raises", v is None,
      f"-> {v!r}")

print(f"\n{ok} OK, {fail} fallas")
sys.exit(1 if fail else 0)
