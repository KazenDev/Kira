/**
 * Luz.h - Patron de carga 9: CIRCULO DE CARGA CON LUZ 🕯️📊
 *
 * El sensor de luz (los LEDs como fotodiodos) controla un circulo de
 * carga: luz = se llena, oscuridad = se vacia. Al completar: flash y
 * respira.
 *
 * v2 - AUTO-CALIBRACION: al iniciar (o con el comando serial "CALIB")
 * pide tapar (▼) e iluminar (☀) 3s cada fase y mapea [min,max] del
 * ambiente al circulo. Sin diferencia real -> fallback 0..255.
 */
#ifndef LUZ_H
#define LUZ_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Circulo de carga controlado por el sensor de luz (bucle continuo)
void animarLuz(int vueltas = 2);

// Pide recalibrar el sensor (lo llama el comando serial "CALIB")
void recalibrarLuz();

#endif // LUZ_H
