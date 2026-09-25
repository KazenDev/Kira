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
 *
 * PATRON DE FRAME: animarTriste() muestra UN frame (~16 ms) y vuelve, como
 * hace Alegria y como hacia metroFrame() del metronomo. El bucle de
 * Principal.cpp la llama ~60 veces por segundo y el estado de la animacion
 * vive en el RELOJ, no en una cadena de sleep(). Antes eran 1.415 ms de
 * franja sorda (la fase de respirar son 1,46 s sin un solo revisarSerial()),
 * la peor de las ocho emociones. Ver Triste.cpp.
 */
#ifndef TRISTE_H
#define TRISTE_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra UN frame de la animacion de tristeza (bucle en Principal.cpp)
void animarTriste();

// Para las TRANSICIONES (y CALLA): dibuja la cara en reposo y ancla el ciclo
// de la animacion en este instante.
void mostrarCaraTriste();

#endif // TRISTE_H
