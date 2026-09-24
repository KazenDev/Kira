"""
test_reflexion.py — tests de la FASE 2 de personalidad emergente (LA REFLEXION).

Correr:  .venv/bin/python test_reflexion.py   (desde Cerebro/Backend)

Cubre:
  UNITARIOS:
  1. memoria_cargar filtra recuerdos expirados
  2. _normalizar (min-max, caso constante)
  3. _tokens (tildes fuera, stopwords fuera, cortas fuera)
  4. memoria_recuperar: la memoria relevante+importante entra al top;
     la irrelevativa+vieja queda fuera; respeta top_n
  5. memoria_extraer_pensamiento: parsea evidencia "(por los recuerdos N)";
     mapea a ids reales; SIN evidencia -> None (el guardrail)
  6. candado: reflexiones simultaneas imposibles (reflexionando)
  INTEGRACION (real, ~7 llamadas mini):
  7. una reflexion completa sobre memorias sembradas -> pensamientos con
     evidencia, importancia y estado limpio al final
"""
import asyncio
import json
import os
import sys
import traceback
from datetime import datetime, timedelta

import kira_server as ks

PJ = "test_reflex"
FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    print(f"  [{'OK ' if cond else 'FALLA'}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))
    if not cond:
        FALLOS += 1


def limpiar():
    for nombre in (f"{PJ}.jsonl", f"{PJ}_estado.json"):
        ruta = os.path.join(ks.MEMORIA_DIR, nombre)
        if os.path.exists(ruta):
            os.remove(ruta)


def sembrar(recuerdos: list[tuple[str, int, str]], expirada: tuple[str, int] | None = None):
    """Carga memorias directas (sin gastar IA): (texto, importancia, tipo)."""
    limpiar()
    for texto, imp, tipo in recuerdos:
        ks.memoria_agregar(PJ, texto, tipo, imp)
    if expirada:
        # escribir a mano una memoria vencida (fecha vieja, expiracion vieja)
        vieja = {
            "id": "viejaviej", "fecha": "2020-01-01T00:00:00", "tipo": "observacion",
            "texto": expirada[0], "importancia": expirada[1],
            "expira": "2020-02-01T00:00:00",
        }
        with open(os.path.join(ks.MEMORIA_DIR, f"{PJ}.jsonl"), "a", encoding="utf-8") as f:
            f.write(json.dumps(vieja, ensure_ascii=False) + "\n")


def test_unitarios():
    print("\n== 1) memoria_cargar filtra expirados ==")
    sembrar([("me gusta la musica", 5, "observacion")], expirada=("recuerdo olvidado", 9))
    carga = ks.memoria_cargar(PJ)
    check("carga la vigente", any(r["texto"] == "me gusta la musica" for r in carga))
    check("filtra la expirada", all(r["texto"] != "recuerdo olvidado" for r in carga))
    todo = ks.memoria_cargar(PJ, incluir_expirados=True)
    check("con incluir_expirados=True la trae", any(r["texto"] == "recuerdo olvidado" for r in todo))

    print("\n== 2) _normalizar ==")
    n = ks._normalizar({"a": 1.0, "b": 3.0, "c": 5.0})
    check("min-max a [0,1]", abs(n["a"]) < 1e-9 and abs(n["c"] - 1) < 1e-9 and abs(n["b"] - 0.5) < 1e-9, str(n))
    cte = ks._normalizar({"x": 7.0, "y": 7.0})
    check("caso constante -> 0.5", cte["x"] == 0.5 and cte["y"] == 0.5, str(cte))

    print("\n== 3) _tokens ==")
    t = ks._tokens("A mí me GUSTA la música electrónica, pero la música")
    check("minusculas sin tildes", "musica" in t and "electronica" in t, str(t))
    check("stopwords fuera", "pero" not in t and "gusta" in t, str(t))
    check("palabras cortas fuera", all(len(p) >= 4 for p in t), str(t))

    print("\n== 4) memoria_recuperar (pesos Stanford 0.5/3/2) ==")
    sembrar([
        ("Mateo me conto que odia los lunes porque pierde siempre", 7, "observacion"),
        ("aprendi que la musica calma la tristeza de la gente", 9, "observacion"),
        ("hoy comi algo rico", 2, "observacion"),
        ("me contaron del clima en bogota", 2, "observacion"),
    ])
    recuerdos = ks.memoria_cargar(PJ)
    top = ks.memoria_recuperar(recuerdos, "¿Que siento por los lunes y por que la gente los odia?")
    textos = [r["texto"] for r in top]
    check("la relevante (lunes) entra", any("lunes" in t for t in textos), str(textos))
    top_clima = ks.memoria_recuperar(recuerdos, "¿Como esta el clima en bogota hoy?")
    check("el clima es relevante para la pregunta climatica", any("clima" in r["texto"] for r in top_clima))
    solo2 = ks.memoria_recuperar(recuerdos, "musica", top_n=2)
    check("respeta top_n", len(solo2) <= 2, f"{len(solo2)}")

    print("\n== 5) memoria_extraer_pensamiento ==")
    rel = [{"id": "aaa11111", "texto": "r1"}, {"id": "bbb22222", "texto": "r2"}, {"id": "ccc33333", "texto": "r3"}]
    par = ks.memoria_extraer_pensamiento("Los lunes duelen porque la gente que quiero sufre (por los recuerdos 1, 3)", rel)
    check("parsea texto+evidencia", par is not None and par[0].startswith("Los lunes"), str(par))
    check("mapea a ids reales", par and set(par[1]) == {"aaa11111", "ccc33333"}, str(par))
    check("sin evidencia -> None (guardrail)", ks.memoria_extraer_pensamiento("creo que los lunes son malos sin mas", rel) is None)
    check("formato con parentesis desnudo", ks.memoria_extraer_pensamiento("conclusion valida y larga (1, 2)", rel) is not None)
    check("linea numerada heredada", ks.memoria_extraer_pensamiento("2. otra conclusion con sustento largo (2)", rel) is not None)
    check("linea corta -> None", ks.memoria_extraer_pensamiento("corta (1)", rel) is None)

    print("\n== 6) candado anti-simultaneas ==")
    estado = {"contador": 150, "total": 0, "reflexion_pendiente": True, "reflexionando": True}
    ks.memoria_estado_guardar(PJ, estado)
    # con el candado puesto, reflexionar no debe hacer nada (sale temprano)
    asyncio.run(ks.memoria_reflexionar(PJ, "Kira"))
    check("con candado no consume la pendiente", ks.memoria_estado_cargar(PJ).get("reflexionando") is True)
    limpiar()


async def test_integracion():
    print("\n== 7) INTEGRACION: reflexion completa (gasta ~7 llamadas mini) ==")
    sembrar([
        ("Mateo me conto que erro un penal y se sintio fracasar", 8, "observacion"),
        ("Sentí la necesidad de abrazarlo despues del penal fallido", 8, "observacion"),
        ("Aurora me pregunto por las estrellas y le contaron que son soles", 7, "observacion"),
        ("Mateo dijo que odia los lunes porque siempre pierden", 6, "observacion"),
        ("aprendi que equivocarse en la cancha no define a nadie", 7, "pensamiento"),
        ("Aurora sonaba con ser astronauta alguna vez", 6, "observacion"),
    ])
    estado = ks.memoria_estado_cargar(PJ)
    estado["reflexion_pendiente"] = True
    ks.memoria_estado_guardar(PJ, estado)

    await ks.memoria_reflexionar(PJ, "Kira")
    await asyncio.sleep(0.3)

    recuerdos = ks.memoria_cargar(PJ)
    pensamientos = [r for r in recuerdos if r["tipo"] == "pensamiento" and r["texto"] not in
                    {"aprendi que equivocarse en la cancha no define a nadie"}]
    check("genero pensamientos nuevos", len(pensamientos) >= 1, f"{len(pensamientos)}")
    if pensamientos:
        for p in pensamientos[:3]:
            print(f"        pensamiento ({p['importancia']}/10): {p['texto'][:90]}")
            print(f"          evidencia: {p.get('evidencia', [])}")
        check("todos citan evidencia", all(p.get("evidencia") for p in pensamientos))
        check("importancias en rango", all(1 <= p["importancia"] <= 10 for p in pensamientos))
    estado = ks.memoria_estado_cargar(PJ)
    check("reflexion_pendiente limpiada", estado.get("reflexion_pendiente") is False, str(estado))
    check("candado liberado", estado.get("reflexionando") is False, str(estado))
    check("cuenta la reflexion hecha", estado.get("reflexiones_hechas", 0) == 1, str(estado))


def main():
    print("=" * 62)
    print("TESTS FASE 2 — LA REFLEXION (Stanford Generative Agents)")
    print("=" * 62)
    try:
        test_unitarios()
        if "--solo-unitarios" not in sys.argv:
            asyncio.run(test_integracion())
        else:
            print("\n(integracion salteada)")
    except Exception:
        traceback.print_exc()
        global FALLOS
        FALLOS += 1
    finally:
        limpiar()
    print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"{FALLOS} FALLAS ❌"))
    sys.exit(0 if FALLOS == 0 else 1)


if __name__ == "__main__":
    main()
