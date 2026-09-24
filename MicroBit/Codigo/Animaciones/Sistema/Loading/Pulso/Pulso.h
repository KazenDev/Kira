/**
 * Pulso.h - Patron de carga 2: PULSO DEL CORAZON REAL 💓 (v2)
 *
 * Latido real en el borde: lub-dub con caida exponencial fluida, ritmo
 * natural con variabilidad (HRV) y latidos ectopicos impredecibles
 * cada 6-10 latidos (prematuro + pausa compensatoria). Lento (~63 BPM),
 * en bucle continuo sin reinicios.
 */
#ifndef PULSO_H
#define PULSO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Latido real en el borde: lub-dub, fluido, con ritmo impredecible
void animarPulso(int ciclos = 1);

#endif // PULSO_H
