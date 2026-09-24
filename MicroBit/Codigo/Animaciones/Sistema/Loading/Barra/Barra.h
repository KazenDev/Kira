/**
 * Barra.h - Patron de carga 4: ECUALIZADOR REAL 🎚️⚡ (v3)
 *
 * 5 barras verticales con altura float y sub-pixel a 60fps, alimentadas
 * por las 5 bandas de frecuencia REALES del microfono (FFT de 128 puntos,
 * modulo MicFft). Picos que decaen y gradiente. Bucle continuo.
 */
#ifndef BARRA_H
#define BARRA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Ecualizador real: espectro del microfono en 5 bandas
void animarBarra(int ciclos = 1);

#endif // BARRA_H
