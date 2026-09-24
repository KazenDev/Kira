/**
 * Lluvia.h - Patron de carga 3: LLUVIA CON TORMENTA 🌧️⚡ (v2)
 *
 * Gotas con posicion float a 60fps (sub-pixel, suaves), estela que marca
 * la direccion, velocidades variables, salpicaduras al tocar el piso y
 * tormenta con rayo zigzag + retumbo cada 7-12s. Bucle continuo.
 */
#ifndef LLUVIA_H
#define LLUVIA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Lluvia suave a 60fps con tormenta periodica
void animarLluvia(int ciclos = 1);

#endif // LLUVIA_H
