"""
test_memoria.py — tests de la FASE 1 de personalidad emergente.

Correr:  .venv/bin/python test_memoria.py   (desde Cerebro/Backend)

Cubre:
  1. Estado fresco de un personaje (contador = 150)
  2. memoria_agregar: linea JSONL bien formada + contador que resta
  3. Gatillo estilo Stanford: al cruzar 0 -> reflexion_pendiente + re-armado
  4. Expiracion de 30 dias en cada memoria
  5. INTEGRACION (real, gasta ~2 llamadas chiquitas de IA): el memo
     post-charla escribe una memoria real en primera persona
     -> se puede saltear con: python test_memoria.py --solo-unitarios
"""
import asyncio
import json
import os
import shutil
import sys
import traceback
from datetime import datetime, timedelta

import kira_server as ks

PJ = "test_pj"
FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    estado = "OK " if cond else "FALLA"
    if not cond:
        FALLOS += 1
    print(f"  [{estado}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))


def limpiar():
    for nombre in (f"{PJ}.jsonl", f"{PJ}_estado.json"):
        ruta = os.path.join(ks.MEMORIA_DIR, nombre)
        if os.path.exists(ruta):
            os.remove(ruta)


def test_unitarios():
    print("\n== 1) estado fresco ==")
    limpiar()
    estado = ks.memoria_estado_cargar(PJ)
    check("contador arranca en 150 (Stanford)", estado["contador"] == 150, str(estado))
    check("total arranca en 0", estado.get("total") == 0, str(estado))

    print("\n== 2) agregar memoria ==")
    recuerdo = ks.memoria_agregar(PJ, "Mateo me conto que odia los lunes", "observacion", 4)
    ruta = os.path.join(ks.MEMORIA_DIR, f"{PJ}.jsonl")
    check("jsonl existe", os.path.exists(ruta))
    with open(ruta, encoding="utf-8") as f:
        linea = json.loads(f.readline())
    check("campos completos", all(k in linea for k in ("id", "fecha", "tipo", "texto", "importancia", "expira")), str(linea.keys()))
    check("texto identico", linea["texto"] == "Mateo me conto que odia los lunes")
    check("importancia respetada", linea["importancia"] == 4)
    estado = ks.memoria_estado_cargar(PJ)
    check("contador resto: 150-4=146", estado["contador"] == 146, str(estado))

    print("\n== 3) gatillo de reflexion estilo Stanford ==")
    # escenario deterministico: personaje fresco, 15 memorias de 10 = 150
    # -> el disparo cae EXACTO con la ultima y el contador se re-arma
    limpiar()
    for _ in range(15):
        ks.memoria_agregar(PJ, "memoria intensa de prueba", importancia=10)
    estado = ks.memoria_estado_cargar(PJ)
    check("reflexion_pendiente quedo marcada", estado.get("reflexion_pendiente") is True, str(estado))
    check("contador re-armado a 150 exacto", estado["contador"] == 150, str(estado["contador"]))
    check("cuenta las reflexiones disparadas", estado.get("reflexiones", 0) == 1, str(estado))
    check("total de memorias correcto", estado.get("total") == 15, str(estado))

    # importancia clamp: fuera de rango se ajusta
    r2 = ks.memoria_agregar(PJ, "prueba de limites", importancia=99)
    check("importancia se recorta a 10", r2["importancia"] == 10)

    print("\n== 4) expiracion ==")
    expira = datetime.fromisoformat(linea["expira"])
    esperado = datetime.fromisoformat(linea["fecha"]) + timedelta(days=30)
    check("expira a 30 dias (Stanford)", abs((expira - esperado).total_seconds()) < 2, f"{expira} vs {esperado}")


async def test_integracion():
    print("\n== 5) INTEGRACION: memo post-charla real (gasta 2 llamadas mini) ==")
    limpiar()
    turnos = [
        {"rol": "user", "contenido": "kira, estoy triste porque perdí el partido de futbol"},
        {"rol": "assistant", "contenido": "ay mi amor, que pena... contame que paso"},
        {"rol": "user", "contenido": "perdimos 3-0 y yo erre un penal"},
        {"rol": "assistant", "contenido": "errar un penal no te hace mal jugador"},
    ]
    await ks.memoria_memo_post_charla("test_pj", "Kira", turnos)
    # dar un instante por si el print llega tarde
    await asyncio.sleep(0.3)
    ruta = os.path.join(ks.MEMORIA_DIR, f"{PJ}.jsonl")
    if not os.path.exists(ruta):
        check("el memo escribio una memoria", False, "jsonl no existe")
        return
    with open(ruta, encoding="utf-8") as f:
        lineas = [json.loads(l) for l in f if l.strip()]
    check("hay al menos una memoria", len(lineas) >= 1, f"{len(lineas)} lineas")
    if lineas:
        memo = lineas[0]
        print(f"        memo: {memo['texto'][:100]}")
        print(f"        importancia: {memo['importancia']}/10")
        check("memo en primera persona (tiene 'me'/'mi' o similar)", any(p in memo["texto"].lower() for p in ["me ", "mi ", "me,", "quien", "contaron", "contó", "sentí"]), memo["texto"])
        check("importancia en rango 1-10", 1 <= memo["importancia"] <= 10)
        # una tristeza deportiva deberia pesar mas que hablar del clima
        check("importancia emocional razonable (>=3)", memo["importancia"] >= 3, str(memo["importancia"]))


def main():
    print("=" * 62)
    print("TESTS FASE 1 — personalidad emergente (memoria + memo)")
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
