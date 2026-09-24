/**
 * Arena.h - Patron de carga 7: SANDBOX DE ARENA INTERACTIVO 🏜️🌀
 *
 * La arena responde a la ROTACION Y AL MOVIMIENTO de la micro:bit:
 *   - Inclinacion = gravedad (la arena cae al lado inclinado)
 *   - Sacudida fuerte = DESPARRAME (los granos saltan al azar) 💥
 *   - Giro rapido = INERCIA (la arena "flota" un momento antes de caer)
 * 60fps sub-pixel, 8 granos, bucle continuo.
 */
#ifndef ARENA_H
#define ARENA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Sandbox de arena: 8 granos con fisica real (gravedad + movimiento)
void animarArena(int ciclos = 2);

#endif // ARENA_H
