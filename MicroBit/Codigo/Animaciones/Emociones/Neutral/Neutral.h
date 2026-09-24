/**
 * Neutral.h - Modulo de la emocion NEUTRAL 😑
 *
 * La cara "bruh": OJOS CERRADOS (dos lineas de parpados: ## . ##)
 * + boca recta neutra. El aburrimiento con vida:
 *   - Respira CALMADA (ritmo medio, ni agitada ni lenta)
 *   - Los ojos cerrados se ENTREABREN de vez en cuando (un "peek"
 *     como el que levanta la vista aburrido) y vuelven a cerrarse
 *   - La boca recta a veces se RETUERCE un poquito (un "meh")
 */
#ifndef NEUTRAL_H
#define NEUTRAL_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra una pasada de la animacion neutral (bucle en Principal.cpp)
void animarNeutral();

// Para las TRANSICIONES: dibuja solo el primer frame (cara base)
void mostrarCaraNeutral();

#endif // NEUTRAL_H
