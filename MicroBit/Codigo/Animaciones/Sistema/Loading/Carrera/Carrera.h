/**
 * Carrera.h - Patron de carga 5: LA PERSECUCION 🏃💨
 *
 * 2 corredores (puntitos nítidos de 1 pixel) huyen del cazador con
 * rutas distintas, sin acorralarse en las esquinas (empuje desde los
 * bordes + deriva + wander). El cazador, mas rapido, DECIDE por cual
 * va cada ~1.5-3s (mas cercano / mas lejano / cambia de presa) y al
 * atrapar a uno hace un flash corto y nueva ronda. Bucle continuo.
 */
#ifndef CARRERA_H
#define CARRERA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Persecucion continua: 2 corredores vs 1 cazador (bucle, sin saltos)
void animarCarrera(int vueltas = 2);

#endif // CARRERA_H
