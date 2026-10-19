"""
test_rastro.py - Que se pueda auditar de donde salio CADA cosa que dice Kira

El bug que este archivo previene: la UI le mostraba las fuentes web al usuario
(el bloque "Fuentes" de la burbuja) pero a KIRA no le quedaban. El historial lo
arma el frontend y lo manda como body["historia"] = [{rol, contenido}], y ese
historial solo tiene la respuesta HABLANTE. Los resultados de herramientas se
descartaban al cerrar el turno.

O sea: a vos te salen los links, a ella no. Y si le repreguntas de donde lo
saco, mira su historial, no tiene nada, y te cuenta cualquier cosa. Kira tenia
amnesia de sus propias busquedas.

Estos checks son de los dos lados del viaje:
  - el backend arma el rastro y lo devuelve
  - el texto que se le inyecta a Kira es utilizable por ella
Y el mas importante: que la categoria "criada" exista, porque sin ella la
ausencia de fuente es ambigua.
"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from app.services import rastro as R

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


print("\n== 1) cada herramienta tiene NOMBRE HUMANO ==")
for h in ("leer_temperatura", "leer_luz", "leer_sonido", "leer_botones",
          "leer_brujula", "leer_bateria", "leer_gesto", "leer_toque",
          "leer_estado", "leer_movimiento", "buscar_en_web", "leer_url",
          "calcular", "reloj", "azar", "controlar_metronomo",
          "guardar_recuerdo", "buscar_recuerdos", "olvidar_recuerdo"):
    check(f"{h} tiene etiqueta", h in R.ETIQUETAS)
check("la etiqueta NO es el nombre crudo (si no, no le sirve a un humano)",
      R.ETIQUETAS["leer_brujula"][1] != "leer_brujula",
      f"-> {R.ETIQUETAS['leer_brujula'][1]!r}")

print("\n== 2) el rastro de cada tipo ==")
web = R.item_para_herramienta("buscar_en_web", {"ok": True, "resultado": "x", "args": {"query": "clima pitalito"}})
check("buscar_en_web es tipo web", web["tipo"] == "web", f"-> {web['tipo']}")
check("y guarda el QUERY (para poder volver a buscarlo)", "clima pitalito" in web["detalle"], f"-> {web['detalle']!r}")

sensor = R.item_para_herramienta("leer_luz", {"ok": True, "resultado": "luz: 120/255"})
check("leer_luz es tipo sensor", sensor["tipo"] == "sensor", f"-> {sensor['tipo']}")
check("y el valor recortado queda", "120" in sensor["valor"], f"-> {sensor['valor']!r}")

temp = R.item_para_herramienta("leer_temperatura", {"ok": True, "resultado": "silicio 25"})
check("leer_temperatura es sensor", temp["tipo"] == "sensor")
check("y declara la SEGUNDA fuente (el clima, no el chip)",
      R.ORIGEN_CLIMA in temp["detalle"], f"-> {temp['detalle']!r}")

calc = R.item_para_herramienta("calcular", {"ok": True, "resultado": "45", "args": {"expresion": "15% de 300"}})
check("calcular es tipo calculo", calc["tipo"] == "calculo")
check("y guarda la expresion (se puede auditar la cuenta)",
      "15% de 300" in calc["detalle"], f"-> {calc['detalle']!r}")

print("\n== 3) 'criada' EXISTE (lo mas importante) ==")
check("sin_fuentes devuelve la categoria criada",
      R.sin_fuentes([])["tipo"] == "criada", f"-> {R.sin_fuentes([])}")
check("y dice explicitamente que no consulto nada",
      "no consult" in R.sin_fuentes([])["detalle"].lower())
# La razon: una lista VACIA es ambigua (¿no busco? ¿me fallo?). "criada" responde.
check("no devuelve lista vacia sino un item",
      isinstance(R.sin_fuentes([]), dict))

print("\n== 4) el texto que se le inyecta a Kira ==")
t = R.para_historial_ia([web, sensor])
check("menciona las dos fuentes", "buscó" in t and "chip" in t.lower(), f"-> {t[:120]!r}")
check("le dice que lo diga SOLO si preguntan", "Si te preguntan" in t)
check("no se lo dictate: no dice 'use una herramienta' en el mensaje",
      "usaste una herramienta" not in t.lower())
check("lista vacia -> texto vacio (no inyecta ruido)",
      R.para_historial_ia([]) == "")

print("\n== 5) el rastro no crece sin limite ==")
largo = R.item_para_herramienta("buscar_en_web", {"ok": True, "resultado": "x" * 5000})
check("el valor se recorta", len(largo["valor"]) <= 165, f"-> {len(largo['valor'])} chars")

print(f"\n{ok} OK, {fail} fallas")
sys.exit(1 if fail else 0)
