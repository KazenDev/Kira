"""test_rag_charla.py - ¿Kira se ACUERDA cuando hace falta? (memoria en una charla)

Correr:  .venv/bin/python test_rag_charla.py   (desde Cerebro/Backend)

Mide el bloque de recuerdos que de verdad le llega a la IA en un turno
(memoria_recuerdos_bloque), con recuerdos SEMBRADOS en un indice temporal: no
toca la memoria real de nadie y no llama a la IA (el resumen se intercepta).

Cada caso es una frase que una persona le diria a Kira y lo que el bloque
tiene que traer. Los casos de "¿te acordás...?" son los que mas fallaban:
esas palabras no aparecen en ningun recuerdo, asi que una busqueda literal
no encuentra nada aunque Kira sepa un monton de esa persona.
"""
from __future__ import annotations

import asyncio
import os
import sys
import tempfile
from datetime import datetime, timedelta

os.environ.setdefault("KIRA_SIN_SERIAL", "1")

import kira_server as ks  # noqa: E402
from app.intelligence.rag_store import RagStore  # noqa: E402

FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    print(f"  [{'OK ' if cond else 'FALLA'}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))
    if not cond:
        FALLOS += 1


def recuerdo(i: int, texto: str, dias: float, tipo="observacion", importancia=6, **extra) -> dict:
    fecha = datetime.now() - timedelta(days=dias)
    return {"id": f"r{i:03d}", "fecha": fecha.isoformat(timespec="seconds"), "tipo": tipo,
            "texto": texto, "importancia": importancia,
            "expira": (fecha + timedelta(days=30)).isoformat(timespec="seconds"), **extra}


DURABLE = {"source": "llm_memory_extraction", "durable": True, "memory_class": "semantic"}
EPISODICO = {"source": "episodic_memo", "durable": False}

SEMBRADOS = [
    recuerdo(1, "Tomás tiene un perro que se llama Rocky.", 12, importancia=7, **DURABLE),
    recuerdo(2, "A Tomás le encanta el fútbol y es hincha de Millonarios.", 12, importancia=7, **DURABLE),
    recuerdo(3, "Tomás está en noveno grado en el colegio.", 11, importancia=6, **DURABLE),
    recuerdo(4, "Hoy Tomás me contó que perdió un partido tres a uno y estaba bajoneado.", 3, importancia=7, **EPISODICO),
    recuerdo(5, "Ana me mostró un truco de magia con una moneda y me reí un montón.", 6, importancia=7, **EPISODICO),
    recuerdo(6, "Alguien me preguntó si yo era la impostora y casi me delato.", 9, importancia=6, **EPISODICO),
    recuerdo(7, "Creo que me gusta más charlar de fútbol que de política.", 5, tipo="pensamiento", importancia=7),
    # Distracciones: cosas de otra gente y de otros temas
    recuerdo(20, "Una profe me preguntó cuántas luces tengo y le dije veinticinco.", 15),
    recuerdo(21, "Un nene me pidió que le cante una canción de cumpleaños.", 14),
    recuerdo(22, "Me pusieron a prueba con una cuenta difícil y la saqué con la calculadora.", 13),
    recuerdo(23, "Una chica me contó que le dan miedo las arañas.", 10),
    recuerdo(24, "Me preguntaron la hora tres veces seguidas en la feria.", 8),
    recuerdo(25, "Un señor me dijo que los robots nunca van a tener sentimientos.", 7),
    recuerdo(26, "Hablamos de los planetas y Saturno es el que más me gusta.", 7),
    recuerdo(27, "Alguien me tapó el sensor de luz para ver si me asustaba.", 6),
    recuerdo(28, "Me contaron un chiste de un pollito y no lo entendí.", 5),
    recuerdo(29, "Un grupo de chicos me hizo bailar con el metrónomo.", 4),
    recuerdo(30, "Una nena me dijo que mi color rojo es su favorito.", 4),
    recuerdo(31, "Me preguntaron si sé cocinar y dije que solo arroz digital.", 2),
    recuerdo(32, "Discutimos si los gatos son mejores que los perros.", 2),
    recuerdo(33, "Un chico de once me preguntó cómo se programa una micro:bit.", 1.5),
    recuerdo(34, "Me sacudieron fuerte y me mareé un montón.", 1.2),
]

# (frase, historial, alguna de estas palabras TIENE que estar en el bloque)
CASOS = [
    ("¿cómo se llama mi perro?", [], ["Rocky"]),
    ("¿de qué equipo soy hincha?", [], ["Millonarios"]),
    ("¿en qué grado estoy?", [], ["noveno"]),
    ("¡hoy ganamos el partido!", [], ["partido"]),
    ("¿qué truco te mostró Ana?", [], ["moneda"]),
    ("¿de qué te gusta charlar a vos?", [], ["fútbol"]),
    # Las de memoria vaga: no comparten palabras con ningun recuerdo
    ("¿te acordás de lo que te conté la otra vez?", [], ["partido", "Rocky", "Millonarios", "noveno"]),
    ("contame algo que recuerdes de mí", [], ["partido", "Rocky", "Millonarios", "noveno"]),
    ("¿sabés algo de mí?", [], ["partido", "Rocky", "Millonarios", "noveno"]),
    # Con historial: la pregunta corta se entiende por lo que se venia hablando
    ("¿y cómo se llamaba?", [{"rol": "user", "contenido": "acordate de mi perro"},
                             {"rol": "assistant", "contenido": "¡Claro! ¿Qué pasa con él?"}], ["Rocky"]),
]


async def correr() -> dict:
    tmp = tempfile.mkdtemp(prefix="rag_charla_")
    store = RagStore(os.path.join(tmp, "index.sqlite3"), ks.rag_embeddings,
                     rrf_k=ks.config.RAG_RRF_K)
    original_store, original_cargar, original_texto = ks.rag_store, ks.memoria_cargar, ks.texto_ia
    llamadas_resumen = []

    async def resumen_falso(sistema, usuario, **kw):
        # El resumen real es una llamada a la IA: aca se devuelve el listado
        # tal cual, para medir solo lo que la RECUPERACION trajo.
        llamadas_resumen.append(usuario)
        return usuario.split("[Recuerdos que me vinieron]")[-1].strip()

    ks.rag_store = store
    ks.memoria_cargar = lambda pj, incluir_expirados=False: list(SEMBRADOS)
    ks.texto_ia = resumen_falso
    resultados = {}
    try:
        if ks.rag_embeddings.available():
            store.sync_memories("kira", SEMBRADOS, embed=True)
        for frase, hist, esperadas in CASOS:
            bloque = await ks.memoria_recuerdos_bloque("kira", "Kira", frase, hist)
            ok = any(e.lower() in bloque.lower() for e in esperadas)
            resultados[frase] = (ok, bloque)
        control = await ks.memoria_recuerdos_bloque("kira", "Kira", "¿cuál es la capital de Francia?", [])
        resultados["__control__"] = (True, control)
    finally:
        ks.rag_store, ks.memoria_cargar, ks.texto_ia = original_store, original_cargar, original_texto
    resultados["__resumenes__"] = (True, str(len(llamadas_resumen)))
    return resultados


def main() -> int:
    print("== memoria en una charla (embeddings:",
          "SI" if ks.rag_embeddings.available() else "NO, solo palabras", ") ==")
    res = asyncio.run(correr())
    for frase, _, _ in CASOS:
        ok, bloque = res[frase]
        check(f"'{frase}'", ok, "bloque: " + (bloque.replace("\n", " | ")[:160] or "(vacío)"))
    _, control = res["__control__"]
    print(f"  (control 'capital de Francia': {'vacío' if not control else control.replace(chr(10), ' | ')[:120]})")
    print(f"  (llamadas extra a la IA para resumir: {res['__resumenes__'][1]})")
    aciertos = sum(1 for f, _, _ in CASOS if res[f][0])
    print(f"\nRECUPERA {aciertos}/{len(CASOS)}")
    return 1 if FALLOS else 0


if __name__ == "__main__":
    sys.exit(main())
