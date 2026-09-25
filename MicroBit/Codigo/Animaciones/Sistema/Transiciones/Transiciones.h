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
 * TODO a 60 FPS reales: cada frame = uBit.sleep(16) (1/60 s). El display
 * refresca a 60 Hz (NRF52_LED_MATRIX_FREQUENCY), asi que 16 ms es el techo:
 * mas rapido no se ve, solo gasta.
 *
 * Cada transicion dura ~2,0 s (126 frames; Cortina son 132). OJO: antes de
 * que se les agregara el chequeo de serial por frame eran COMPLETAMENTE
 * SORDAS: 2,0 s sin mirar el puerto, con la IA mandando comandos que la
 * placa no leia. Medido: 1984 ms de peor latencia. Ahora es un frame (16 ms).
 *
 * NO SE ANIDAN: si un comando llega durante una transicion, el destino se
 * guarda y se aplica al terminar, en vez de llamar a otra transicion desde
 * adentro (Morfosis reserva 580 bytes de pila y la pila util es ~2 KB). Ver
 * el guard en Transiciones.cpp.
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
