/**
 * Cometa.h - Patron de carga 0: COMETA ☄️
 *
 * Una luz orbita el borde de la matriz dejando un rastro que se desvanece
 * solo (decay exponencial). Todo el codigo de ESTE patron vive en
 * Cometa.cpp: toca este archivo sin romper los otros loadings.
 */
#ifndef COMETA_H
#define COMETA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Una luz orbitando el borde con rastro (~55 fps)
void animarCometa(int vueltas = 2);

#endif // COMETA_H
