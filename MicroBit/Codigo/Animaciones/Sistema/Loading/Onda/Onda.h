/**
 * Onda.h - Patron de carga 8: ONDA INTERACTIVA 🌊
 *
 * Onda senoidal REAL a 60fps que responde al movimiento de la placa:
 *   - Inclinacion = velocidad y direccion (la onda fluye cuesta abajo)
 *   - Movimiento brusco (jerk) = agitacion de la amplitud
 *   - Rafagas de olas que decaen solas (como tirar una piedra al agua)
 * Linea senoidal sub-pixel con rastro, bucle continuo.
 */
#ifndef ONDA_H
#define ONDA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Onda senoidal interactiva (acelerometro), bucle continuo
void animarOnda(int ciclos = 2);

#endif // ONDA_H
