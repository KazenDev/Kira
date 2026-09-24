/**
 * Flechas.h - Patron de carga 6: FLECHAS GIRATORIAS 🔄
 *
 * El spinner circular clasico: 4 flechas de luz giran por el borde con
 * punta sub-pixel a 60fps y rastro que se desvanece. El giro es fluido
 * (sin saltos de pixel) y en bucle continuo.
 */
#ifndef FLECHAS_H
#define FLECHAS_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// 4 flechas girando en el borde (spinner circular, bucle continuo)
void animarFlechas(int vueltas = 2);

#endif // FLECHAS_H
