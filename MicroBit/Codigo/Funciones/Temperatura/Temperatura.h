/**
 * Temperatura.h - FUNCION: leer la temperatura ambiente del micro:bit
 *
 * El micro:bit v2 mide la temperatura del chip nRF52833 (aproximadamente
 * la del ambiente, con un pequeño desfase por el calor propio).
 *
 * Uso desde la IA (tool calling):
 *   -> comando serial:  SENSOR:TEMP
 *   <- respuesta:       TEMP:24
 */
#ifndef FUNCION_TEMPERATURA_H
#define FUNCION_TEMPERATURA_H

#include "MicroBit.h"

// Instancia global del micro:bit (definida en Principal.cpp)
extern MicroBit uBit;

// Lee la temperatura actual en grados Celsius (int).
// Ej: 24 = 24 °C.
int leerTemperatura();

#endif // FUNCION_TEMPERATURA_H
