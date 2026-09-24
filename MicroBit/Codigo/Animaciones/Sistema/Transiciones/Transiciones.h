/**
 * Transiciones.h - TRANSICIONES entre emociones (efectos de 60 FPS)
 *
 * CADA transicion vive en su propia carpeta con su codigo por separado:
 *
 *   Transiciones/
 *   ├── Transiciones.h / Transiciones.cpp  <- el DIRECTOR (elige random)
 *   ├── Morfosis/  0 🧬 (los pixeles VIAJAN a la cara nueva)
 *   ├── Cortina/   1 🎭 (columnas se cierran y abren)
 *   └── Fundido/   2 🌫️ (fade a negro y aparece la nueva)
 *
 * Cuando la IA manda una EMOCION (HAPPY, SAD, ANGRY...), el sistema
 * elige UNA transicion AL AZAR y la reproduce ANTES de dibujar la
 * cara nueva: el micro:bit nunca cambia de cara "de golpe".
 *
 * Comandos de prueba: "TRANS" (una random) y "TRANSALL" (preview).
 *
 * TODO a 60 FPS reales: cada frame = uBit.sleep(16) (1/60 s).
 * Cada transicion dura ~1.2s (72 frames) y termina dejando el primer
 * frame de la emocion destino dibujado (la animacion sigue desde ahi).
 */
#ifndef TRANSICIONES_H
#define TRANSICIONES_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// El enum de emociones (EmocionActual) vive en Sistema.h
#include "../Sistema.h"

#define NUM_TRANSICIONES 3

// Elige UNA transicion al azar y la reproduce hacia la emocion destino.
// Al terminar, deja el primer frame de destino dibujado.
void hacerTransicion(EmocionActual destino);

// Reproduce la transicion idx (0..2) hacia destino (para probar)
void mostrarTransicion(int idx, EmocionActual destino);

// Reproduce TODAS en secuencia (preview): TRANSALL
void mostrarTransicionesTodos();

// Dibuja el primer frame (cara base) de la emocion destino
void dibujarCaraDestino(EmocionActual destino);

#endif // TRANSICIONES_H
