/**
 * Alegria.h - Modulo de la emocion ALEGRIA
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * Esta carpeta contiene:
 *   Alegria.cpp  -> la alegria en reposo (sonrisa completa + vida)
 *   Hablar.cpp   -> la boca hablando (lip-sync) via comando TALK
 *
 * PATRON DE FRAME: animarAlegria() muestra UN frame (~16 ms) y vuelve, como
 * metroFrame() del metronomo. El bucle de Principal.cpp la llama ~60 veces por
 * segundo y el estado de la animacion vive en el RELOJ, no en una cadena de
 * sleep(). Gracias a eso un comando de la IA se nota en el frame siguiente y
 * no hasta 1,3 s despues. Ver el comentario de Alegria.cpp.
 */
#ifndef ALEGRIA_H
#define ALEGRIA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra UN frame de la animacion de alegria (bucle en Principal.cpp)
void animarAlegria();

// Para las TRANSICIONES (y CALLA): dibuja la cara en reposo y ancla el ciclo
// de la animacion en este instante.
void mostrarCaraAlegria();

#endif // ALEGRIA_H
