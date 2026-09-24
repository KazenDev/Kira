/**
 * Luz.h - FUNCION: leer la luz ambiente del micro:bit
 *
 * El micro:bit v2 NO tiene fotosensor dedicado: usa la propia matriz LED
 * como sensor (los LEDs miden la luz que les llega). CODAL lo expone como
 * uBit.display.readLightLevel() y devuelve 0..255 (0 = oscuro, 255 = muy
 * iluminado).
 *
 * Uso desde la IA (tool calling):
 *   -> comando serial:  SENSOR:LUZ
 *   <- respuesta:       LUZ:120
 */
#ifndef FUNCION_LUZ_H
#define FUNCION_LUZ_H

#include "MicroBit.h"

extern MicroBit uBit;

// Lee la luz ambiente en 0..255 (0 oscuro, 255 brillante).
int leerLuz();

#endif // FUNCION_LUZ_H
