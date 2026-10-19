/**
 * Bloques.h - Patron de carga 13: BLOQUES 🧱
 *
 * un mini-Tetris: caen bloques, se apilan, una fila llena destella y se borra.
 * Todo el codigo de ESTE patron vive en Bloques.cpp.
 */
#ifndef BLOQUES_H
#define BLOQUES_H

#include "MicroBit.h"

extern MicroBit uBit;

void animarBloques(int ciclos = 1);

#endif // BLOQUES_H
