/**
 * HablarNeutral.h - La boca del NEUTRAL que HABLA 😑🗣️
 *
 * Cada emocion tiene SU hablar. Cuando la IA manda "TALK" y la emocion
 * activa es NEUTRAL, el neutral habla asi:
 *   - Los 3 LEDs de la boca RECTA (1,3)(2,3)(3,3) PARPADEAN a ritmo
 *     de habla (como los dientes de la alegria pero con la boca
 *     neutra) - el "bruh" hablando sin expresion.
 *   - UNA FIBRA de CODAL hace "peeks" de ojos EN PARALELO: los
 *     parpados cerrados se entreabren de vez en cuando (1.5-5s) y
 *     vuelven a cerrarse, como el que habla con los ojos semicerrados.
 *
 * Al terminar de hablar (otra emocion o STOP) modoHablar se apaga,
 * la fibra muere sola y el bucle principal vuelve al neutral en reposo.
 */
#ifndef HABLARNEUTRAL_H
#define HABLARNEUTRAL_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// TALK (con neutral activo): dibuja la cara neutral hablando y lanza
// la fibra de peeks
void iniciarHablarNeutral();

// Una pasada de los 3 LEDs de la boca parpadeando (bucle principal)
void animarBocaNeutral();

#endif // HABLARNEUTRAL_H
