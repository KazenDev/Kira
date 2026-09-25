/**
 * Neutral.h - Modulo de la emocion NEUTRAL 😑
 *
 * La cara "bruh": OJOS CERRADOS (dos lineas de parpados: ## . ##)
 * + boca recta neutra. El aburrimiento con vida:
 *   - Respira CALMADA (ritmo medio, ni agitada ni lenta)
 *   - Los ojos cerrados se ENTREABREN de vez en cuando (un "peek"
 *     como el que levanta la vista aburrido) y vuelven a cerrarse
 *   - La boca recta a veces se RETUERCE un poquito (un "meh")
 *
 * PATRON DE FRAME: animarNeutral() muestra UN frame (~16 ms) y vuelve, como
 * las otras seis ya migradas. El bucle de Principal.cpp la llama ~60 veces
 * por segundo y el estado vive en el RELOJ, no en una cadena de sleep().
 * Antes eran 750 ms de franja sorda (el peek son 840 ms sin un solo
 * revisarSerial(), y sale dos veces por pasada). Ver Neutral.cpp.
 *
 * El "meh" es ASIMETRIA a proposito: solo se mueve el labio izquierdo. Hay
 * evidencia de que la asimetria aumenta la credibilidad y la naturalidad
 * percibidas de un personaje, y de que las caras simetricas son justamente las
 * que mas parecen un robot.
 */
#ifndef NEUTRAL_H
#define NEUTRAL_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra UN frame de la animacion neutral (bucle en Principal.cpp)
void animarNeutral();

// Para las TRANSICIONES (y CALLA): dibuja la cara en reposo y ancla el ciclo
// de la animacion en este instante.
void mostrarCaraNeutral();

#endif // NEUTRAL_H
