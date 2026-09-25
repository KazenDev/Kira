/**
 * Cansado.h - Modulo de la emocion CANSADO 😪 (en reposo)
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * Esta carpeta contiene:
 *   Cansado.cpp     -> el cansancio en reposo (respira, parpadea peso,
 *                      bosteza y cabecea)
 *   HablarCansado.cpp -> la boca hablando con sueño (lip-sync) via TALK
 *
 * PATRON DE FRAME: animarCansado() muestra UN frame (~16 ms) y vuelve, como
 * Alegria y Triste. El bucle de Principal.cpp la llama ~60 veces por segundo
 * y el estado vive en el RELOJ, no en una cadena de sleep(). Antes eran
 * 950 ms de franja sorda (el parpadeo pesado son 1,04 s sin un solo
 * revisarSerial(), y sale dos veces por pasada). Ver Cansado.cpp.
 */
#ifndef CANSADO_H
#define CANSADO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra UN frame de la animacion de cansancio (bucle en Principal.cpp)
void animarCansado();

// Para las TRANSICIONES (y CALLA): dibuja la cara en reposo y ancla el ciclo
// de la animacion en este instante.
void mostrarCaraCansado();

#endif // CANSADO_H
