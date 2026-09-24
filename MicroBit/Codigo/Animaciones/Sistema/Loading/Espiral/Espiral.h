/**
 * Espiral.h - Patron de carga 1: ESPIRAL 🌀 (v2, doble sentido)
 *
 * Espiral parametrica lenta que se dibuja con rastro: gira adentro/afuera
 * en un sentido (CW) y al llegar al borde invierte el giro (CCW), con
 * transiciones continuas. En bucle continuo, sin reinicios.
 */
#ifndef ESPIRAL_H
#define ESPIRAL_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Espiral lenta con doble sentido de giro y rastro largo
void animarEspiral(int ciclos = 1);

#endif // ESPIRAL_H
