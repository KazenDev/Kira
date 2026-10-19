"""
rastro.py - De donde salio CADA cosa que dijo Kira, para que se pueda auditar.

EL PROBLEMA QUE RESUELVE
========================

La UI le mostra las fuentes web al usuario (el bloque "Fuentes" de la burbuja),
pero a KIRA no le quedaban. El historial de conversacion lo arma el FRONTEND y
lo manda al backend como body["historia"] = [{rol, contenido}]. Ese historial
solo tiene la respuesta HABLANTE: los resultados de herramientas y las fuentes
se descartan al cerrar el turno.

O sea: a vos te salen los links, a ella no. Y si le repreguntas "de donde
sacaste eso", mira su historial, no tiene nada, y te contesta cualquier cosa.
Kira tiene amnesia de sus propias busquedas.

ADEMAS, el bloque de fuentes solo cubria buscar_en_web. Si decia "siento 24
grados" no habia ninguna fuente que mostrar, ni para vos ni para ella, que es
justo donde mas miente (el sensor es el silicio del chip, no el ambiente).

QUE HACE ESTE MODULO
====================

Arma una lista de "de donde salio esto" para cada turno, con TRES clases de
origen, que es la distincion util:

  web     - consulto internet. Con link. Verificable por el usuario.
  sensor  - se lo dijo su propio cuerpo (el micro:bit). No es un dato del
            mundo: es una medicion del hardware, y hay que decirlo asi.
  calculo - lo calculo ella con la herramienta exacta.
  memoria - lo saco de lo que recorda (diario/memoria). No es fuente: es ella.
  criada  - no uso nada. Sale de su conocimiento de entrenamiento. NO tiene
            fuente, y por eso NO se le inventa una.

Y la ultima clase es la mas importante: deja la AUSENCIA de fuente visible. Si
la UI solo muestra el bloque cuando hay fuentes, entonces no se puede
distinguir "no busca porque no hace falta" de "busco y le fallo". Con la
categoria "criada" se ve que no consulto nada.

Y las dos mitades:

  1) Se le ENVIA a Kira en el historial de los turnos siguientes, para que
     pueda citar sus propias fuentes cuando le preguntan de donde saco algo.
  2) Se le MANDA a la UI, para que el usuario pueda auditar.
"""
from __future__ import annotations

# Como se le dice CADA herramienta a una persona. Sin esto, el rastro dice
# "leer_brujula" y el usuario no entiende nada. Con esto dice "su brújula".
ETIQUETAS: dict[str, tuple[str, str]] = {
    # (tipo, nombre humano)
    "buscar_en_web": ("web", "buscó en internet"),
    "leer_url": ("web", "leyó una página"),
    "leer_temperatura": ("sensor", "su propio chip (temperatura)"),
    "leer_luz": ("sensor", "su propio chip (luz)"),
    "leer_sonido": ("sensor", "su micrófono (sonido)"),
    "leer_botones": ("sensor", "sus botones"),
    "leer_movimiento": ("sensor", "su acelerómetro"),
    "leer_gesto": ("sensor", "su acelerómetro (gesto)"),
    "leer_brujula": ("sensor", "su brújula"),
    "leer_toque": ("sensor", "su logo táctil"),
    "leer_bateria": ("sensor", "su batería"),
    "leer_estado": ("estado", "cómo se siente"),
    "reloj": ("sistema", "el reloj del sistema"),
    "ver_clima": ("clima", "el clima de Pitalito (Open-Meteo)"),
    "calcular": ("calculo", "lo calculó"),
    "azar": ("calculo", "lo sortió al azar"),
    "controlar_metronomo": ("accion", "su metrónomo"),
    "guardar_recuerdo": ("memoria", "lo guardó en su memoria"),
    "buscar_recuerdos": ("memoria", "lo buscó en su memoria"),
    "actualizar_recuerdo": ("memoria", "actualizó un recuerdo"),
    "olvidar_recuerdo": ("memoria", "olvidó un recuerdo"),
}

# Cuando el clima entra por leer_temperatura, la fuente REAL no es el chip: es
# Open-Meteo. Sin esto, el rastro de la temperatura seria "su propio chip" y
# se perderia el segundo dato que se le pasa a la IA.
ORIGEN_CLIMA = "el clima de Pitalito (Open-Meteo)"


def _recorta(texto: str, n: int = 160) -> str:
    t = " ".join(str(texto).split())
    return t if len(t) <= n else t[: n - 1] + "…"


def item_para_herramienta(nombre: str, resultado: dict) -> dict:
    """Arma UN item del rastro a partir del nombre de la tool y su resultado."""
    tipo, etiqueta = ETIQUETAS.get(nombre, ("otro", nombre))
    detalle = ""
    valor = _recorta(resultado.get("resultado", "")) if resultado else ""

    if nombre == "buscar_en_web":
        # El query es lo que le permitio volver a encontrarlo.
        args = resultado.get("args") if resultado else None
        if isinstance(args, dict) and args.get("query"):
            detalle = f'buscó "{args["query"]}"'
        else:
            detalle = "buscó en internet"
    elif nombre == "leer_url":
        args = resultado.get("args") if resultado else None
        if isinstance(args, dict) and args.get("url"):
            detalle = _recorta(str(args["url"]), 70)
    elif nombre == "calcular":
        args = resultado.get("args") if resultado else None
        if isinstance(args, dict) and args.get("expresion"):
            detalle = _recorta(str(args["expresion"]), 70)
    elif nombre == "leer_temperatura":
        # El segundo numero: el clima. Va aparte porque es OTRA fuente.
        detalle = ORIGEN_CLIMA

    return {
        "tipo": tipo,
        "herramienta": nombre,
        "etiqueta": etiqueta,
        "detalle": detalle,
        "valor": valor,
        "ok": bool(resultado.get("ok")) if resultado else False,
    }


def sin_fuentes(herramientas: list[dict]) -> dict:
    """El rastro de un turno en el que NO consulto nada.

    Se devuelve igual, con la categoria "criada", en vez de una lista vacia. La
    diferencia importa: vacio es ambiguo (¿no busco o me fallo?), "criada" dice
    que no consulto nada y el numero sale de su conocimiento, que no tiene
    fuente. Es la categoria que mas honestidad le da al usuario.
    """
    return {
        "tipo": "criada",
        "herramienta": "",
        "etiqueta": "de su propio conocimiento",
        "detalle": "no consultó ninguna fuente en este turno",
        "valor": "",
        "ok": True,
    }


def para_historial_ia(rastro: list[dict]) -> str:
    """El texto que se le inyecta a Kira para que sepa de donde salio cada cosa.

    Va como un turno propio del historial, NO mezclado con su respuesta. Motivo:
    si se lo mezclamos, ella lo va a leer en voz alta o lo va a citar siempre, y
    el prompt le pide expresamente no decir "use una herramienta". Como turno
    aparte, lo tiene disponible para cuando le pregunten de donde lo saco, y
    nocontamination su conversacion normal.
    """
    if not rastro:
        return ""
    lineas = ["[De donde salio lo que acaba de decir]"]
    for it in rastro:
        partes = [f"- {it['etiqueta']}"]
        if it.get("detalle"):
            partes.append(it["detalle"])
        if it.get("valor"):
            partes.append(f"-> {it['valor']}")
        lineas.append(" ".join(partes))
    lineas.append(
        "Si te preguntan de donde sacaste esto, decilo con esto. No lo digas "
        "en cada mensaje: solo cuando te lo pregunten."
    )
    return "\n".join(lineas)
