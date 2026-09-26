#!/usr/bin/env python3
"""
sim_rayo.py - ¿LLEGA el retumbo del trueno a verse alguna vez?

PREGUNTA CONCRETA. Lluvia::rayo() son 4 pasos: flash de cielo, rayo en zigzag,
fade, y retumbo (3 ecos del trueno). El retumbo es el unico que tiene
uBit.sleep(120). El loading corre dentro de un LOTE de 250 ms (LoadingBase.h:
LOTE_MS) y frameRastro() devuelve true en cuanto se agota, con lo que el
patron hace su `return` de siempre.

El rayo NO esta protegido contra el corte del lote. Y como proximoRayo ya se
resetea ANTES de llamar a rayo(), un rayo cortado a medias no se reintenta.

O sea que hay DOS preguntas, y la segunda es la que duele:
  1) ¿se ve el retumbo?
  2) ¿el sleep(120) se ejecuta? -> 120 ms de puerto sordo

POR QUE UNA SIMULACION Y NO MEDIR EN LA PLACA. Es aritmetica de timers, se
puede simular exacta y sale en un segundo. Y medido en la placa NO se puede
aislar: el pico del retumbo (pantalla entera a 55) y el flash del cielo
(pantalla entera a 170) se ven IGUALES por el replica LED, que manda 1 bit
por pixel. Dos eventos que la placa no distingue y la simulacion si.

EL LFSR. uBit.random() es codal::random() (CodalCompat.cpp:109): un LFSR de
32 bits con static random_value GLOBAL COMPARTIDO, semilla 0xC0DA1. Esta
implementado tal cual abajo. Importa porque LLUVIA es la unica cosa que
todavia lo usa (las emociones ya van con pseudo()), o sea que su estado se
comparte con todo lo demas. Consecuencia: la fase del rayo respecto del lote
NO se puede predecir leyendo el codigo, porque depende de cuantas veces haya
llamado random() todo el resto del programa. Por eso se corre de dos maneras:

  lfsr  : el LFSR real, semilla 0xC0DA1, con TODAS las llamadas que hace
          Lluvia por frame. Es una realizacion particular.
  ideal : random(300) uniforme e independiente. Es la ESPERANZA sobre todas
          las historias posibles del LFSR global.

Si los dos coinciden, la conclusion no depende de la realizacion.
"""
import sys

FRAME_MS = 16
LOTE_MS = 250
FRAMES_CICLO = 400
N_GOTAS = 5
PASOS_ANTES = 2 + 2 + 3          # flash + rayo + fade, antes del retumbo
FRAMES_TRUENO = 24              # la cola nueva: 55*0,87^24 = 1,9


# --------------------------------------------------------------------------
# El LFSR de codal::random(), copiado de CodalCompat.cpp:109
# --------------------------------------------------------------------------
class LFSR:
    def __init__(self, seed=0xC0DA1):
        self.v = seed & 0xFFFFFFFF

    def rnd(self, mx):
        """codal::random(mx)"""
        if mx <= 0:
            return 0
        if self.v == 0:
            self.v = 0xC0DA1
        mx -= 1
        while True:
            m = mx
            res = 0
            while True:
                r = self.v
                bit = (((r >> 31) ^ (r >> 6) ^ (r >> 4) ^ (r >> 2)
                        ^ (r >> 1) ^ r) & 1)
                r = ((bit << 31) | (r >> 1)) & 0xFFFFFFFF
                self.v = r
                res = ((res << 1) | (r & 1)) & 0xFFFFFFFF
                m >>= 1
                if m == 0:
                    break
            if res <= mx:
                return res


class Ideal:
    """random(300) uniforme: la esperanza sobre todas las historias del LFSR."""
    def __init__(self, seed):
        import random
        self._r = random.Random(seed)

    def rnd(self, mx):
        return self._r.randrange(mx) if mx > 0 else 0


# --------------------------------------------------------------------------
# El estado de la lluvia (lo que hace actualizarGotas, solo para avanzar el LFSR)
# --------------------------------------------------------------------------
class Gotas:
    def __init__(self, R, iniciada=False):
        self.R = R
        self.fase = [0] * N_GOTAS
        self.timer = [0] * N_GOTAS
        if not iniciada:
            for i in range(N_GOTAS):
                self.timer[i] = 20 + i * 45 + R.rnd(70)
                R.rnd(5)

    def frame(self):
        for i in range(N_GOTAS):
            f = self.fase[i]
            if f == 0:
                if self.timer[i] > 0:
                    self.timer[i] -= 1
                    continue
                self.fase[i] = 1
                self.R.rnd(5)                       # gx
                self.R.rnd(50)                      # gv
                if self.R.rnd(7) == 0:              # gota gorda
                    pass
            elif f == 2:
                if self.timer[i] > 0:
                    self.timer[i] -= 1
                    continue
                self.fase[i] = 0
                self.timer[i] = 25 + self.R.rnd(120)
            else:
                pass
            # la fase 1 puede caer al piso (fase 2, timer 3), sin random


# --------------------------------------------------------------------------
# Lluvia::rayo() -- version VIEJA (con sleep(120) y sin exencion de lote)
# --------------------------------------------------------------------------
def rayo_viejo(t, loteHasta, R):
    """Devuelve (t_final, nivel, pintó_pico, durmio120)."""
    # el rayo en zigzag: 5 llamadas a random
    R.rnd(5)
    for _ in range(4):
        R.rnd(3)

    for _ in range(PASOS_ANTES):                     # pasos 1-3
        if t >= loteHasta:
            return t, 0, False, False
        t += FRAME_MS

    # paso 4: retumbo
    if t >= loteHasta:
        return t, 1, False, False                   # llego, pero sin pico
    t += FRAME_MS                                    # pinta el pico y duerme
    pico = True
    durmio = False
    for p in range(3):                               # 3 ecos
        if t >= loteHasta:
            return t, 2 + p, pico, durmio
        t += FRAME_MS
        durmio = True          # el sleep(120) YA ocurrio, corte o no corte
        t += 120
    return t, 5, pico, True


# --------------------------------------------------------------------------
# Lluvia::rayoInterno() -- version NUEVA (exenta del lote, cola por el rastro)
# --------------------------------------------------------------------------
# El pico dura 1 frame y la cola son 24 frames de decaerRastro(87). 55*0,87^24
# = 1,9, o sea hasta apagarse. frameRayo() sigue cortando por comando serial
# (eso NO se exento), asi que un comando externo tambien lo puede cortar.
def rayo_nuevo(t, R):
    """El pico dura 1 frame y la cola son 24 frames de decaerRastro(87).
    El lote NO lo corta (loteExento), asi que llega siempre al final. El
    serial SIEMPRE lo corta: eso no se exento."""
    R.rnd(5)
    for _ in range(4):
        R.rnd(3)
    for _ in range(PASOS_ANTES):                     # pasos 1-3
        t += FRAME_MS
    t += FRAME_MS                                    # el pico
    for f in range(FRAMES_TRUENO):                   # la cola
        t += FRAME_MS
    return t, 6, True, False                         # 6 = llego al final


def correr(R, n_rayos, nuevo):
    t = 0
    gotas = Gotas(R)
    proximo = 420 + R.rnd(300)
    corte = [0] * 7
    pico = dormi = hechos = 0

    while hechos < n_rayos:
        loteHasta = t + LOTE_MS
        for _ in range(FRAMES_CICLO):
            if t >= loteHasta:          # frameRastro(78) corta el ciclo
                break
            gotas.frame()
            if proximo <= 0:
                proximo = 420 + R.rnd(300)   # resetea ANTES de rayo()
                if nuevo:
                    t, nivel, p, d = rayo_nuevo(t, R)
                else:
                    t, nivel, p, d = rayo_viejo(t, loteHasta, R)
                corte[nivel] += 1
                hechos += 1
                pico += p
                dormi += d
            else:
                proximo -= 1
            t += FRAME_MS
    return corte, pico, dormi, hechos


ETIQUETAS = [
    "corte en flash / rayo / fade  (no se ve NADA del rayo)",
    "llego al retumbo pero se corto justo ANTES del pico",
    "pico + 0 de 3 ecos",
    "pico + 1 de 3 ecos",
    "pico + 2 de 3 ecos",
    "retumbo completo (3 de 3)",
    "llego hasta el final",
]


def mostrar(nombre, corte, pico, dormi, total, completo):
    print(f"--- {nombre} ---")
    for i in range(7):
        if corte[i]:
            print(f"  {corte[i]:4d} ({100*corte[i]/total:5.1f}%)  {ETIQUETAS[i]}")
    print(f"  {total:4d} (100.0%)  TOTAL")
    print()
    print(f"  el rayo llega entero en   : {completo}/{total} ({100*completo/total:.0f}%)")
    print(f"  el pico del retumbo se ve : {pico}/{total} ({100*pico/total:.0f}%)")
    print(f"  el sleep(120) se ejecuta  : {dormi}/{total} ({100*dormi/total:.0f}%)")
    print()


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 400
    print(f"rayos por corrida: {n}\n")
    for nombre, fabrica in [("LFSR real (semilla 0xC0DA1)", lambda: LFSR(0xC0DA1)),
                            ("random(300) uniforme", lambda: Ideal(12345))]:
        print(f"===== {nombre} =====\n")
        c, p, d, tot = correr(fabrica(), n, nuevo=False)
        mostrar("ANTES (con corte de lote y sleep(120))", c, p, d, tot, c[5])
        c, p, d, tot = correr(fabrica(), n, nuevo=True)
        mostrar("DESPUES (exento del lote, cola por el rastro)", c, p, d, tot, c[6])
    print("LEER: 'random(300) uniforme' es la esperanza sobre todas las")
    print("historias posibles del LFSR global. Si las dos corridas coinciden,")
    print("el numero no depende de la realizacion que este en juego.")


main()
