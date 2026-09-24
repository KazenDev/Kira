/**
 * Alegria.h - Modulo de la emocion ALEGRIA
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * Esta carpeta contiene:
 *   Alegria.cpp  -> la alegria en reposo (sonrisa completa + vida)
 *   Hablar.cpp   -> la boca hablando (lip-sync) via comando TALK
 */
#ifndef ALEGRIA_H
#define ALEGRIA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra una pasada de la animacion de alegria (bucle en Principal.cpp)
void animarAlegria();

// Para las TRANSICIONES: dibuja solo el primer frame (cara base)
void mostrarCaraAlegria();

#endif // ALEGRIA_H
