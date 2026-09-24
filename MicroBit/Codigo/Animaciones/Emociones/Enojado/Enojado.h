/**
 * Enojado.h - Modulo de la emocion ENOJADO 😠
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * El enojo es TERSO y BRUSCO, todo lo contrario a la tristeza:
 *   - Respira AGITADA: el brillo sube/baja RAPIDO (mas que la alegria)
 *   - Cejas que se FRUNCEN en oleadas: se tensan (se engrosan hacia
 *     el centro) y se relajan, cada vez mas seguidas
 *   - Parpadeo BRUSCO: se cierran rapido, cerrados un instante
 *   - La mandibula APRIETA: los dientes apretados (boca seria)
 */
#ifndef ENOJADO_H
#define ENOJADO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra una pasada de la animacion de enojo (bucle en Principal.cpp)
void animarEnojado();

// Para las TRANSICIONES: dibuja solo el primer frame (cara base)
void mostrarCaraEnojado();

#endif // ENOJADO_H
