/**
 * Triste.h - Modulo de la emocion TRISTE 😢
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * La tristeza es LENTA y PESADA, todo lo contrario a la alegria:
 *   - Respira lento (el brillo sube/baja despacio y mas tenue)
 *   - Parpadeo PESADO: los ojos se cierran lento y quedan cerrados
 *     un momento (como cuando estas cansado/triste)
 *   - La boca tiembla un poquito (el labio a punto de llorar)
 *   - De vez en cuando (cada ~4-7 pasadas, al azar) cae una LAGRIMA
 *     que se junta bajo un ojo y baja por la mejilla
 */
#ifndef TRISTE_H
#define TRISTE_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra una pasada de la animacion de tristeza (bucle en Principal.cpp)
void animarTriste();

// Para las TRANSICIONES: dibuja solo el primer frame (cara base)
void mostrarCaraTriste();

#endif // TRISTE_H
