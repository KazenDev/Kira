"""Pure sentence segmentation used by the streaming TTS scheduler."""

from __future__ import annotations

import re


def dividir_frases(texto: str, max_frases: int = 6) -> list[str]:
    text = (texto or "").strip()
    if not text:
        return []
    parts = [part.strip() for part in re.split(r"(?<=[.?!…])\s+", text) if part.strip()]
    phrases: list[str] = []
    current = ""
    for part in parts:
        current = (current + " " + part).strip() if current else part
        if len(current) >= 20:
            phrases.append(current)
            current = ""
    if current:
        phrases.append(current)
    if len(phrases) > max_frases:
        phrases = phrases[: max_frases - 1] + [" ".join(phrases[max_frases - 1 :])]
    return [phrase for phrase in phrases if phrase]


def _termina_en_punto(frase: str) -> bool:
    value = frase.rstrip("\"»”)' ").strip()
    return bool(value) and value[-1] in ".?!…"


# --- Audio tags de ElevenLabs ------------------------------------------------
#
# Kira puede escribir audio tags de Eleven v4 en el medio del texto
# ("jaja [laughs] no te creo", "[sighs] bueno..."). La VOZ los necesita; la
# PANTALLA, la memoria y el historial no: ahi serian ruido ("[laughs]" escrito
# en la burbuja). Solo se sacan corchetes con forma de tag: palabras cortas en
# minuscula, opcionalmente separadas por coma ("[whispers, scared]").
_TAG = re.compile(r"\s*\[[a-záéíóúñ][a-záéíóúñ ,'-]{1,40}\]", re.IGNORECASE)


# Un tag que todavia esta llegando ("A ver [gigg"): mientras el texto se
# escribe en vivo, el pedazo sin cerrar tampoco se muestra
_TAG_ABIERTO = re.compile(r"\s*\[[a-záéíóúñ ,'-]{0,40}$", re.IGNORECASE)


def quitar_tags_audio(texto: str) -> str:
    limpio = _TAG.sub("", texto or "")
    limpio = _TAG_ABIERTO.sub("", limpio)
    limpio = re.sub(r"[ \t]{2,}", " ", limpio)
    limpio = re.sub(r"\s+([,.;:!?…])", r"\1", limpio)
    return limpio.strip()


# --- Segmentador INCREMENTAL para la voz por frases --------------------------
#
# POR QUE NO ALCANZA dividir_frases. El texto de la IA llega en pedazos y antes
# se volvia a dividir TODO el texto en cada pedazo, deduplicando por frase
# identica. Pero una frase corta se pega con la siguiente al llegar mas texto,
# y "Hola." + "Hola. ¿Cómo estás?" no son identicas: se mandaban LAS DOS a la
# voz (Kira repetia). Y los "..." llegan de a un punto, asi que salian
# "Bueno.", "Bueno.." y "Bueno..." como tres audios: la voz se cortaba y
# volvia, que es exactamente lo que se escuchaba en la web.
#
# Ahora hay un CURSOR: lo que ya se mando a la voz no se vuelve a mirar. Una
# frase se da por terminada solo cuando el signo va seguido de un espacio (asi
# un "..." a medio llegar nunca cuenta como final), y los puntos suspensivos
# NO cortan: son una pausa dentro de la frase. Solo si un pedazo se hace muy
# largo se acepta cortar en ellos.

_FIN_FUERTE = re.compile(r"[.?!]+[\"»”)']*\s+")
_FIN_SUSPENSIVO = re.compile(r"(?:\.{2,}|…)[\"»”)']*\s+")


class SegmentadorTTS:
    def __init__(self, primera_min: int = 12, min_chars: int = 28, largo_max: int = 140):
        self.cursor = 0
        self.primera_min = primera_min    # la primera sale antes: latencia
        self.min_chars = min_chars
        self.largo_max = largo_max
        self.enviadas: list[str] = []
        self.prefijo = ""

    def _minimo(self) -> int:
        return self.primera_min if not self.enviadas else self.min_chars

    def avanzar(self, texto: str) -> list[str]:
        """Frases NUEVAS y completas desde la ultima vez (puede ser ninguna)."""
        texto = texto or ""
        if len(texto) < self.cursor:
            # El texto se achico (no deberia pasar): no se repite nada
            return []
        nuevas: list[str] = []
        while True:
            pendiente = texto[self.cursor:]
            corte = None
            for m in _FIN_FUERTE.finditer(pendiente):
                if not _FIN_SUSPENSIVO.match(pendiente, m.start()) and m.end() >= self._minimo():
                    corte = m.end()
                    break
            if corte is None and len(pendiente) >= self.largo_max:
                # Pedazo muy largo sin punto: se acepta cortar en "..." o en coma
                for patron in (_FIN_SUSPENSIVO, re.compile(r",\s+")):
                    ms = [m for m in patron.finditer(pendiente) if m.end() >= self.min_chars]
                    if ms:
                        corte = ms[0].end()
                        break
            if corte is None:
                return nuevas
            frase = pendiente[:corte].strip()
            self.cursor += corte
            self.prefijo = texto[:self.cursor]
            if frase:
                self.enviadas.append(frase)
                nuevas.append(frase)

    def terminar(self, texto_final: str) -> list[str]:
        """Al final: lo que quede (aunque no termine en punto).

        Si el texto final NO continua lo que ya se dijo (la IA cambio el
        mensaje entre rondas), no se repite lo dicho: se manda solo lo nuevo
        si se puede ubicar, y si no, nada de lo ya enviado se vuelve a decir.
        """
        texto_final = texto_final or ""
        if self.cursor and not texto_final.startswith(self.prefijo):
            # El final no continua lo dicho (espacios, comillas o un mensaje
            # rearmado): se ubica la ultima frase dicha y se sigue desde ahi.
            ultima = self.enviadas[-1] if self.enviadas else ""
            pos = texto_final.find(ultima) if ultima else -1
            if pos < 0:
                return []          # no se puede ubicar: mejor no repetir nada
            self.cursor = pos + len(ultima)
            self.prefijo = texto_final[:self.cursor]
        nuevas = self.avanzar(texto_final)
        resto = texto_final[self.cursor:].strip()
        if resto:
            self.cursor = len(texto_final)
            self.prefijo = texto_final
            self.enviadas.append(resto)
            nuevas.append(resto)
        return nuevas

    def dicho(self) -> str:
        return " ".join(self.enviadas)
