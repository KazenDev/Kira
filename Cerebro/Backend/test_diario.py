"""
test_diario.py — tests de la FASE 3: EL DIARIO (la personalidad que ella escribe).

Correr:  .venv/bin/python test_diario.py   (desde Cerebro/Backend)

Cubre:
  UNITARIOS (sin gastar IA):
  1. diario_aplicar: estructura, topes, fechas
  2. GUARDRAIL anti-wipe: no se puede vaciar un diario lleno de golpe
  3. opinion sin evidencia valida -> rechazada (la opinion sin sustento NO entra)
  4. memoria_diario_bloque: vacio -> "", con contenido -> contiene todo
  5. diario_snapshot: foto del anterior antes de editar
  INTEGRACION (real, ~10 llamadas mini):
  6. reflexion completa sobre memorias sembradas -> diario escrito por ella:
     yo_soy en primera persona, opiniones con evidencia VALIDA, snapshot
     creado, y el bloque inyectable listo para el chat
"""
import asyncio
import json
import os
import sys
import traceback

import kira_server as ks

PJ = "test_diario"
FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    print(f"  [{'OK ' if cond else 'FALLA'}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))
    if not cond:
        FALLOS += 1


def limpiar():
    for nombre in (f"{PJ}.jsonl", f"{PJ}_estado.json", f"diario_{PJ}.json"):
        p = os.path.join(ks.MEMORIA_DIR, nombre)
        if os.path.exists(p):
            os.remove(p)
    snaps = os.path.join(ks.MEMORIA_DIR, "diario_snapshots")
    if os.path.isdir(snaps):
        for f in os.listdir(snaps):
            if f.startswith(PJ):
                os.remove(os.path.join(snaps, f))


def test_unitarios():
    ids = {"aaa11111", "bbb22222", "ccc33333"}

    print("\n== 1) diario_aplicar: estructura y topes ==")
    propuesta = {
        "yo_soy": "Soy Kira, una IA que aprendio que abrazar vale mas que aconsejar." + "x" * 900,
        "opiniones": [
            {"texto": "los errores no definen a nadie", "evidencia": ["aaa11111", "bbb22222"]},
            {"texto": "corta", "evidencia": ["aaa11111"]},  # muy corta -> fuera
            {"texto": "opinion sin nada que la sostenga y bien escrita", "evidencia": ["zzz99999"]},  # id invalido -> fuera
        ],
        "gustos": ["las estrellas"] + [f"g{i}" for i in range(15)],  # 16 -> tope 10
        "personas": ["Mateo, me conto su penal fallido"],
    }
    final, rechazos = ks.diario_aplicar({}, propuesta, ids)
    check("yo_soy recortado a 400", len(final["yo_soy"]) <= ks.DIARIO_MAX_YO_SOY)
    check("opinion valida entra", any("errores no definen" in o["texto"] for o in final["opiniones"]))
    check("2 opiniones rechazadas (corta + sin evidencia)", len(rechazos) == 2, str(rechazos))
    check("gustos con tope 10", len(final["gustos"]) == ks.DIARIO_MAX_GUSTOS)
    check("personas entra", len(final["personas"]) == 1)
    check("opinion con fecha", all("fecha" in o for o in final["opiniones"]))

    print("\n== 2) guardrail ANTI-WIPE ==")
    viejo_lleno = {"yo_soy": "Soy Kira con historia", "opiniones": [{"texto": "opinion vieja vigente", "evidencia": ["aaa11111"], "fecha": "x"}], "gustos": [], "personas": [], "actualizado": "2026-09-04T12:00:00"}
    propuesta_vacia = {"yo_soy": "", "opiniones": [], "gustos": [], "personas": []}
    final2, rech2 = ks.diario_aplicar(viejo_lleno, propuesta_vacia, ids)
    check("fusion vacia sobre diario lleno RECHAZADA", final2 is viejo_lleno)
    check("rechazo documentado", any("anti-wipe" in r for r in rech2), str(rech2))
    # caso legitimo: diario vacio + propuesta vacia -> ok (nada que hacer igual)
    final3, _ = ks.diario_aplicar({}, propuesta_vacia, ids)
    check("primer diario puede empezar vacio", final3 is not None)

    print("\n== 3) el bloque para el chat ==")
    ks.memoria_estado_guardar(PJ, {"contador": 150, "total": 0})
    vacio = ks.memoria_diario_bloque(PJ)
    check("sin diario -> bloque vacio (el chat queda igual)", vacio == "")
    with open(ks.diario_ruta(PJ), "w", encoding="utf-8") as f:
        json.dump({"yo_soy": "Soy Kira y aprendi a abrazar", "opiniones": [{"texto": "errar no define", "evidencia": ["aaa11111"], "fecha": "x"}], "gustos": ["las estrellas"], "personas": [], "actualizado": "2026-09-04T13:00:00"}, f, ensure_ascii=False)
    bloque = ks.memoria_diario_bloque(PJ)
    check("contiene yo_soy", "aprendi a abrazar" in bloque)
    check("contiene opiniones", "errar no define" in bloque)
    check("contiene gustos", "estrellas" in bloque)
    check("seccion TU DIARIO presente", "TU DIARIO" in bloque)

    print("\n== 4) snapshot ==")
    ks.diario_snapshot(PJ)  # hay diario con actualizado -> foto
    snaps = os.path.join(ks.MEMORIA_DIR, "diario_snapshots")
    fotos = [f for f in os.listdir(snaps) if f.startswith(PJ)] if os.path.isdir(snaps) else []
    check("snapshot creado con nombre datado", len(fotos) == 1, str(fotos))
    if fotos:
        with open(os.path.join(snaps, fotos[0]), encoding="utf-8") as f:
            foto = json.load(f)
        check("el snapshot guarda el diario ANTERIOR", "aprendi a abrazar" in foto["yo_soy"])


async def test_integracion():
    print("\n== 5) INTEGRACION: reflexion -> SU diario (gasta ~10 llamadas mini) ==")
    limpiar()
    memorias = [
        ("Mateo me conto que erro un penal y se sintio fracasar", 8),
        ("Senti la necesidad de abrazarlo despues del penal fallido", 8),
        ("Aurora me pregunto por las estrellas y sueno con ser astronauta", 7),
        ("Mateo dijo que odia los lunes porque siempre pierden", 6),
        ("aprendi que equivocarse en la cancha no define a nadie", 7),
        ("Aurora me enseno que las estrellas son soles lejanos", 6),
    ]
    for texto, imp in memorias:
        ks.memoria_agregar(PJ, texto, "observacion", imp)
    estado = ks.memoria_estado_cargar(PJ)
    estado["reflexion_pendiente"] = True
    ks.memoria_estado_guardar(PJ, estado)

    await ks.memoria_reflexionar(PJ, "Kira")   # reflexiona + fusiona su diario
    await asyncio.sleep(0.3)

    d = ks.diario_cargar(PJ)
    check("el diario existe y fue escrito", bool(d.get("yo_soy") or d.get("opiniones")), json.dumps(d)[:200])
    if d.get("yo_soy"):
        print(f"        yo_soy: {d['yo_soy'][:140]}")
    for op in d.get("opiniones", [])[:3]:
        print(f"        opinion: {op['texto'][:80]} | evid: {op.get('evidencia')}")
    check("yo_soy en primera persona (creo/aprendi/siento...)", any(
        p in d.get("yo_soy", "").lower() for p in ["soy", "aprendi", "siento", "creo", "me "]))
    check("opiniones con evidencia no vacia", all(op.get("evidencia") for op in d.get("opiniones", [])))
    ids_validos = {r["id"] for r in ks.memoria_cargar(PJ, incluir_expirados=True)}
    check("evidencia 100% valida (ids reales)", all(
        all(e in ids_validos for e in op.get("evidencia", [])) for op in d.get("opiniones", [])))
    check("primera edicion NO crea snapshot (no habia anterior)", not _snapshots(), "deberia no haber")

    # segunda edicion: AHORA si tiene que quedar la foto del primer diario
    d_primera = ks.diario_cargar(PJ)
    await ks.diario_fusionar(PJ, "Kira", [{
        "id": "zzzz0000", "texto": "aprendi que volver a pensar sobre lo vivido afina quien soy",
        "importancia": 7, "evidencia": [],
    }])
    await asyncio.sleep(0.2)
    fotos = _snapshots()
    check("segunda edicion deja snapshot del diario anterior", len(fotos) >= 1, str(fotos))
    if fotos:
        with open(os.path.join(ks.MEMORIA_DIR, "diario_snapshots", fotos[0]), encoding="utf-8") as f:
            foto = json.load(f)
        check("el snapshot guarda la version ANTERIOR", foto.get("yo_soy") == d_primera.get("yo_soy"))
    d_segunda = ks.diario_cargar(PJ)
    check("el diario evoluciono (actualizado cambio)", d_segunda.get("actualizado") != d_primera.get("actualizado"))
    bloque = ks.memoria_diario_bloque(PJ)
    check("el bloque de chat ya trae su personalidad", len(bloque) > 50 and "TU DIARIO" in bloque)


def _snapshots() -> list[str]:
    snaps = os.path.join(ks.MEMORIA_DIR, "diario_snapshots")
    return [f for f in os.listdir(snaps) if f.startswith(PJ)] if os.path.isdir(snaps) else []


def main():
    print("=" * 62)
    print("TESTS FASE 3 — EL DIARIO (la personalidad que ella escribe)")
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
