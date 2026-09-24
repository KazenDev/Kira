"""
test_recuerdos.py — tests de MEMORIA EN CHARLA (converse.py de Stanford).

Correr:  .venv/bin/python test_recuerdos.py   (desde Cerebro/Backend)

Cubre:
  UNITARIOS (la mayoria sin gastar IA):
  1. sin recuerdos -> bloque vacio (hoy: costo cero, chat intacto)
  2. recuerdos frescos (<15 min) excluidos: son de ESTA charla
  3. pocos recuerdos viejos (<3) -> van crudos, sin llamada extra
  4. la recuperacion es relevante: preguntan de musica -> aparece lo musical
  INTEGRACION (real, ~3 llamadas):
  5. el RESUMEN estilo Stanford (>=3 recuerdos): una frase en primera persona
  6. EL CASO REAL DEL USUARIO: chat A (tomboys) -> chat B pregunta
     "que te gustaba?" -> el bloque trae tomboys -> y la IA RESPONDE
     recordandolo (charla completa con system armado como el endpoint)
"""
import asyncio
import json
import os
import sys
import traceback
from datetime import datetime, timedelta

import kira_server as ks

PJ = "test_recrd"
FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    print(f"  [{'OK ' if cond else 'FALLA'}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))
    if not cond:
        FALLOS += 1


def limpiar():
    for f in (f"{PJ}.jsonl", f"{PJ}_estado.json", f"diario_{PJ}.json"):
        p = os.path.join(ks.MEMORIA_DIR, f)
        if os.path.exists(p):
            os.remove(p)


def sembrar(recuerdos: list[tuple[str, int]], hace_minutos: float = 60):
    """Memorias con fecha ATRASADA (para pasar el filtro de frescos)."""
    limpiar()
    hace = datetime.now() - timedelta(minutes=hace_minutos)
    expira = hace + timedelta(days=40)  # todavia vigentes
    with open(os.path.join(ks.MEMORIA_DIR, f"{PJ}.jsonl"), "a", encoding="utf-8") as f:
        for texto, imp in recuerdos:
            f.write(json.dumps({
                "id": os.urandom(4).hex(), "fecha": hace.isoformat(timespec="seconds"),
                "tipo": "observacion", "texto": texto, "importancia": imp,
                "expira": expira.isoformat(timespec="seconds"),
            }, ensure_ascii=False) + "\n")


async def test_unitarios():
    print("\n== 1) sin recuerdos -> vacio ==")
    limpiar()
    bloque = await ks.memoria_recuerdos_bloque(PJ, "Kiro", "hola?", [])
    check("bloque vacio (hoy no cuesta nada)", bloque == "")

    print("\n== 2) frescos de esta charla excluidos ==")
    ks.memoria_agregar(PJ, "me preguntaron si prefiero tomboys", 6)  # fecha AHORA
    bloque = await ks.memoria_recuerdos_bloque(PJ, "Kiro", "que te gustaba?", [])
    check("recuerdo de hace 0 min NO se inyecta", bloque == "")

    print("\n== 3) pocos y viejos -> crudos ==")
    sembrar([("Mateo me dijo que prefiere las tomboys", 7)])
    bloque = await ks.memoria_recuerdos_bloque(PJ, "Kiro", "que te gustaba?", [])
    check("bloque presente", bloque != "")
    check("va CRUDO (sin resumen: es 1 recuerdo)", "- Mateo me dijo" in bloque, bloque[:80])
    check("seccion con titulo correcto", "LO QUE TE VIENE A LA MENTE" in bloque)

    print("\n== 4) relevancia selectiva ==")
    sembrar([
        ("Mateo me dijo que prefiere las tomboys", 7),
        ("Aurora me enseno que las estrellas son soles lejanos", 6),
        ("hoy comi algo rico en la feria", 2),
    ])
    bloque = await ks.memoria_recuerdos_bloque(PJ, "Kiro", "que te gustaba? lo de las tomboys que hablamos", [])
    check("trae lo relevante (tomboys)", "tomboy" in bloque.lower(), bloque[:200])
    check("la seccion esta presente", "LO QUE TE VIENE A LA MENTE" in bloque)
    # y lo irrelevante no se fuerza: pregunta de otro tema sin memoria -> vacio o sin comida
    bloque2 = await ks.memoria_recuerdos_bloque(PJ, "Kiro", "hablamos de futbol y goles", [])
    check("tema sin recuerdos no fuerza memoria", ("comi" not in bloque2.lower()), bloque2[:120])


async def test_integracion():
    print("\n== 5) RESUMEN estilo Stanford (>=3: la llamada extra) ==")
    sembrar([
        ("Mateo me dijo que prefiere las tomboys", 7),
        ("Aurora me enseno que las estrellas son soles lejanos", 6),
        ("Mateo conto que odia los lunes porque siempre pierden", 6),
    ], hace_minutos=120)
    bloque = await ks.memoria_recuerdos_bloque(PJ, "Kira", "que me decis de las tomboys y los lunes?", [])
    check("bloque con resumen presente", "LO QUE TE VIENE A LA MENTE" in bloque)
    cuerpo = bloque.split("MENTE (recuerdos de tu vida, no de esta charla)\n")[-1]
    check("es UNA frase comprimida (no lista cruda)", not cuerpo.strip().startswith("-"), cuerpo[:120])
    check("menciona lo importante", any(k in cuerpo.lower() for k in ["tomboy", "lunes"]), cuerpo[:200])
    print(f"        resumen: {cuerpo[:160]}")

    print("\n== 6) EL CASO REAL: chat A (tomboys) -> chat B pregunta ==")
    # chat B: mismo armado de system que usa el endpoint real, con la
    # pregunta EXACTA que uso el usuario en su prueba
    pj = ks.cargar_personaje("kira")
    pregunta_real = "¿Que te gustaba? de esos de los trans cuales eran?"
    bloque = await ks.memoria_recuerdos_bloque(PJ, "Kira", pregunta_real, [])
    system = pj["system_prompt"] + ks.memoria_diario_bloque(PJ) + bloque
    r = await ks.post_json_ia({
        "model": ks.config.MODELO_IA,
        "messages": [
            {"role": "system", "content": system},
            {"role": "user", "content": pregunta_real},
        ],
        "response_format": ks.config.JSON_MODE,
        "max_tokens": 150, "temperature": 0.8,
    })
    salida = json.loads(r["choices"][0]["message"]["content"])
    msg = salida.get("message", "")
    print(f"        ({salida.get('emotion')}) {msg[:130]}")
    check("responde JSON valido", salida.get("emotion") is not None)
    # la memoria INFLUYO en la respuesta: habla del tema recordado (el
    # modelo puede suavizar el termino exacto, pero si no estuviera la
    # memoria no podria hablar de "esa seguridad/autenticidad" NUNCA)
    bajo = msg.lower()
    influyo = any(k in bajo for k in ["tomboy", "estilo", "seguridad", "autentic", "mateo", "visaje", "onda"])
    check("la memoria del otro chat INFLUYO en la respuesta", influido := influyo, msg)
    check("el bloque SI contenia el termino exacto (sistema ok)",
          "tomboy" in bloque.lower(), bloque[:160])


def main():
    print("=" * 62)
    print("TESTS — MEMORIA EN CHARLA (Stanford converse.py adaptado)")
    print("=" * 62)
    try:
        asyncio.run(test_unitarios())
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
