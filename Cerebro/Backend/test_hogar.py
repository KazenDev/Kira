"""
test_hogar.py — tests de la FASE 4: EL HOGAR (personalidad 100% emergente).

Correr:  .venv/bin/python test_hogar.py   (desde Cerebro/Backend)

Cubre:
  CONTRATO (que no se rompió NADA del pipeline):
  1. Kira (única personalidad) carga, es FEMENINA y el JSON sigue parseando
  2. contrato intacto: 8 emociones, herramientas, formato de salida JSON
  3. la personalidad escrita por humanos FUE JUBILADA
  4. el hogar está presente: piso de valores + intocable
  5. la voz opinionada está presente (anti robot diplomático)
  INTEGRACION (real, ~2 llamadas):
  6. charla real con Kira recién nacida (sin diario): responde JSON
     válido Y con opinión propia (sin "en mi opinión" ni "como IA")
"""
import asyncio
import json
import sys
import traceback

import kira_server as ks

FALLOS = 0
EMOCIONES = {"happy", "sad", "angry", "surprised", "neutral", "fastidio", "miedo", "cansado"}


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    print(f"  [{'OK ' if cond else 'FALLA'}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))
    if not cond:
        FALLOS += 1


def test_contrato():
    print("\n== 1) Kira (única personalidad) carga ==")
    kira = json.load(open("../Personaje/kira.json", encoding="utf-8"))
    check("kira.json parsea", "system_prompt" in kira)
    check("Kira es FEMENINA", kira.get("genero") == "femenina", str(kira.get("genero")))
    check("solo queda UN personaje (se fue Kiro)", ks.config.PERSONAJES == ["kira"],
          str(ks.config.PERSONAJES))
    check("el JSON de Kiro ya no existe",
          not __import__("os").path.exists("../Personaje/kiro.json"))
    check("backup de las originales existe",
          __import__("os").path.exists("../Personaje/original_escrito/kira.json"))

    sp = kira["system_prompt"]
    print("\n== 2) el contrato técnico INTACTO ==")
    check("las 8 emociones", all(e in sp for e in EMOCIONES))
    check("formato de salida JSON", '"emotion"' in sp and '"tool"' in sp and "# Salida" in sp)
    check("herramientas presentes", "leer_temperatura" in sp and "controlar_metronomo" in sp)
    check("reglas de voz TTS", "# Voz (TTS)" in sp)

    print("\n== 3) la personalidad escrita por humanos JUBILADA ==")
    for fuera in ("Sentimental y empática", "pocas bromas", "la sentimental del dúo", "Vos sos su opuesta"):
        check(f"kira sin: '{fuera[:30]}'", fuera not in sp)

    print("\n== 4) el HOGAR presente ==")
    check("piso de valores", "bondad" in sp.lower() and "intocable" in sp.lower())
    check("protección infantil", "niños" in sp and "adulto de confianza" in sp)
    check("vale más que el diario", "diario" in sp and "no lo cambia" in sp)

    print("\n== 5) la voz opinionada ==")
    check("anti robot diplomático", "PROHIBIDO" in sp and "en mi opinión" in sp)
    check("tomar postura", "Tomá postura" in sp and "discrepar" in sp)
    check("carácter desde TU DIARIO", "TU DIARIO" in sp and "LO ESCRIBÍS VOS" in sp)
    check("temperamento de cuna mínimo", "Temperamento de cuna" in sp)
    check("rol en cría", "en cría" in kira["rol"])
    check("sin rastro del hermano en el prompt", "Kiro" not in sp)


async def test_integracion():
    print("\n== 6) INTEGRACION: charla real con Kira recién nacida (2 llamadas) ==")
    pj = ks.cargar_personaje("kira")
    bloque = ks.memoria_diario_bloque("kira")
    check("su diario aún está vacío (recién nacida)", bloque == "", "debería estar vacía")

    # pregunta provocadora de opiniones
    for pregunta in ("kira, qué pensás del reggaetón? decime tu opinión honesta",
                     "cual es mejor: los perros o los gatos? no me des respuesta diplomática"):
        r = await ks.post_json_ia({
            "model": ks.config.MODELO_IA,
            "messages": [
                {"role": "system", "content": pj["system_prompt"] + bloque},
                {"role": "user", "content": pregunta},
            ],
            "response_format": ks.config.JSON_MODE,
            "max_tokens": 200,
            "temperature": 0.85,
        })
        salida = json.loads(r["choices"][0]["message"]["content"])
        msg = salida.get("message", "")
        print(f"        ({salida.get('emotion')}) {msg[:110]}")
        check("responde JSON válido con emotion", salida.get("emotion") in EMOCIONES)
        check("mensaje no vacío", len(msg) > 15)
        bajo = msg.lower()
        check("SIN 'en mi opinión'", "en mi opinión" not in bajo and "en mi opinion" not in bajo)
        check("SIN 'como IA no...'", "como ia no" not in bajo and "como inteligencia artificial no" not in bajo)
        check("SIN 'depende/uno no puede'", "depende de cada" not in bajo and "no puedo tener opinión" not in bajo)
        check("TOMA postura (dice lo que le parece)", any(
            w in bajo for w in ["me ", "odio", "amo", "me gusta", "me parece", "adoro", "no me", "prefiero"]))


def main():
    print("=" * 62)
    print("TESTS FASE 4 — EL HOGAR (personalidad 100% emergente)")
    print("=" * 62)
    try:
        test_contrato()
        if "--solo-unitarios" not in sys.argv:
            asyncio.run(test_integracion())
        else:
            print("\n(integracion salteada)")
    except Exception:
        traceback.print_exc()
        global FALLOS
        FALLOS += 1
    print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"{FALLOS} FALLAS ❌"))
    sys.exit(0 if FALLOS == 0 else 1)


if __name__ == "__main__":
    main()
