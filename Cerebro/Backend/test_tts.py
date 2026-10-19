"""test_tts.py - la voz por frases: sin repeticiones, sin cortes en los "...".

Correr:  .venv/bin/python test_tts.py   (desde Cerebro/Backend)

El bug que cubre (se escuchaba en la web): el texto de la IA llega a pedazos
y antes se volvia a dividir TODO en cada pedazo. "Hola." y "Hola. ¿Cómo
estás?" salian como dos audios (Kira repetia) y los "..." llegaban de a un
punto: "Bueno.", "Bueno..", "Bueno..." -> la voz se cortaba y volvia.
"""
import sys

from app.domain.tts import SegmentadorTTS, quitar_tags_audio
from app.domain.streaming import quitar_muletillas

FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    print(f"  [{'OK ' if cond else 'FALLA'}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))
    if not cond:
        FALLOS += 1


def trocear(texto: str, paso: int) -> list[str]:
    """Simula el stream: el texto va creciendo de a 'paso' caracteres."""
    seg = SegmentadorTTS()
    frases: list[str] = []
    for i in range(paso, len(texto) + paso, paso):
        frases += seg.avanzar(texto[:i])
    frases += seg.terminar(texto)
    return frases


CASOS = [
    "Hola. ¿Cómo estás? Yo bien, gracias. Bueno... la verdad... no sé qué decirte, jaja.",
    "Mirá, mi chipcito marca veintisiete grados... pero ojo, eso es el calor de mis "
    "propios circuitos, no la pieza. Afuera en Pitalito están a diecinueve.",
    "¡¿Qué?! No te creo... [laughs] en serio, ¿vos hiciste eso? Contame todo.",
    "Sí.",
    "Uy... uy... uy... no sé... qué decirte... de verdad... ni idea... jaja... bueno... "
    "dejame pensar... un poquito... más... porque... no... se me ocurre... nada... todavía.",
]

print("== 1) nunca repite ni pierde texto (de a 1, 3 y 11 caracteres) ==")
for texto in CASOS:
    for paso in (1, 3, 11):
        frases = trocear(texto, paso)
        unido = "".join(" ".join(frases).split())
        check(f"paso {paso:2d}: '{texto[:28]}...'", unido == "".join(texto.split()),
              str(frases))

print("\n== 2) los '...' no cortan la frase ==")
frases = trocear(CASOS[0], 1)
check("ninguna frase termina en '..' o en un solo '.' de unos puntos suspensivos",
      not any(f.endswith("..") and not f.endswith("...") for f in frases), str(frases))
check("'Bueno... la verdad...' viaja entero en una frase",
      any("Bueno... la verdad..." in f for f in frases), str(frases))
frases = trocear(CASOS[4], 2)
check("un texto largo lleno de '...' igual se parte (no espera al final)", len(frases) >= 2, str(frases))

print("\n== 3) la primera frase sale pronto (latencia) ==")
seg = SegmentadorTTS()
primeras = seg.avanzar("¡Hola, Tomás! ")
check("'¡Hola, Tomás!' sale sola sin esperar mas texto", primeras == ["¡Hola, Tomás!"], str(primeras))

print("\n== 4) final que no continua exacto lo dicho ==")
seg = SegmentadorTTS()
seg.avanzar("Hola, ¿qué tal? Hoy estoy re contenta. ")
resto = seg.terminar("  Hola, ¿qué tal? Hoy estoy re contenta. Y vos, ¿qué contás?")
check("con espacios de mas: no repite el saludo", not any("Hola" in f for f in resto), str(resto))
check("y dice lo que faltaba", any("qué contás" in f for f in resto), str(resto))

print("\n== 5) audio tags: a la voz si, a la pantalla no ==")
limpio = quitar_tags_audio("¡¿Qué?! No te creo... [laughs] en serio, [whispers, excited] ¿vos?")
check("saca [laughs] y [whispers, excited]", "[" not in limpio, limpio)
check("deja el texto bien puntuado", limpio == "¡¿Qué?! No te creo... en serio, ¿vos?", limpio)
check("no toca corchetes que no son tags", quitar_tags_audio("vale [12] pesos") == "vale [12] pesos")
check("un tag que todavía está llegando no se muestra", quitar_tags_audio("A ver [gigg") == "A ver")

print("\n== 6) muletillas de asistente fuera ==")
check("'Ah, buena esa...' -> 'Ah, no te veo'", quitar_muletillas("Ah, buena esa... no te veo") == "Ah, no te veo")
check("'¡Qué buena pregunta!' al inicio se va", quitar_muletillas("¡Qué buena pregunta! El cielo es azul") == "El cielo es azul")
check("en medio de una frase NO se toca",
      quitar_muletillas("Es una buena pregunta para la profe") == "Es una buena pregunta para la profe")
check("con dos puntos tambien", quitar_muletillas("Hola. Buena pregunta: mirá") == "Hola. Mirá",
      quitar_muletillas("Hola. Buena pregunta: mirá"))
check("a medio llegar no muestra la muletilla", quitar_muletillas("Buena esa") == "")
check("'che' argentino fuera", quitar_muletillas("Tonta será, pero che... algo te causó gracia")
      == "Tonta será, pero... algo te causó gracia", quitar_muletillas("Tonta será, pero che... algo te causó gracia"))
check("'Che, ¿viste?' -> '¿viste?'", quitar_muletillas("Che, ¿viste eso?") == " ¿viste eso?".strip(),
      repr(quitar_muletillas("Che, ¿viste eso?")))
check("no toca 'noche' ni 'leche'", quitar_muletillas("De noche tomo leche") == "De noche tomo leche")

print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"{FALLOS} FALLAS ❌"))
sys.exit(1 if FALLOS else 0)
