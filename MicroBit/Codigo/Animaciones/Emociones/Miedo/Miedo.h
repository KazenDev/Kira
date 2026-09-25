/**
 * Miedo.h - Modulo de la emocion MIEDO 😨 (en reposo)
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * Esta carpeta contiene:
 *   Miedo.cpp        -> el miedo en reposo (corazon acelerado, tiembla,
 *                       mira al centro, parpadea rapido, labios temblan)
 *   HablarMiedo.cpp  -> la boca tartamudeando (lip-sync) via TALK
 *
 * PATRON DE FRAME: animarMiedo() muestra UN frame (~16 ms) y vuelve, como
 * Alegria, Triste y Cansado. El bucle de Principal.cpp la llama ~60 veces por
 * segundo y el estado vive en el RELOJ, no en una cadena de sleep(). Antes
 * eran 590 ms de franja sorda (el corason acelerado son 720 ms sin un solo
 * revisarSerial()). Ver Miedo.cpp.
 *
 * Es la cara mas grande de las ocho: 14 pixeles en uso (2 cejas + 2 ojos en
 * las esquinas + 8 de boca abierta + 2 pupilas que van al centro).
 */
#ifndef MIEDO_H
#define MIEDO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra UN frame de la animacion de miedo (bucle en Principal.cpp)
void animarMiedo();

// Para las TRANSICIONES (y CALLA): dibuja la cara en reposo y ancla el ciclo
// de la animacion en este instante.
void mostrarCaraMiedo();

#endif // MIEDO_H
