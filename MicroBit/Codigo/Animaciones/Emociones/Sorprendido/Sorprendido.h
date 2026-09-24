/**
 * Sorprendido.h - Modulo de la emocion SORPRENDIDO 😲
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * La sorpresa es OJOS ABIERTOS y BOCA DE "o":
 *   - Los ojos se DILATAN: miran fijo, bien abiertos y brillantes
 *   - La boca "o" se ABRE en un diamante grande (como "WOW") y se cierra
 *   - Parpadea RARISIMO: casi nunca (el asombrado mira fijo)
 *   - De vez en cuando un PULSO DE ASOMBRO: todo el brillo sube de
 *     golpe y vuelve (el susto)
 */
#ifndef SORPRENDIDO_H
#define SORPRENDIDO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra una pasada de la animacion de sorpresa (bucle en Principal.cpp)
void animarSorprendido();

// Para las TRANSICIONES: dibuja solo el primer frame (cara base)
void mostrarCaraSorprendido();

#endif // SORPRENDIDO_H
