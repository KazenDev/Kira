/**
 * Botones.h - FUNCION: leer el estado de los botones A y B
 *
 * El micro:bit v2 tiene 2 botones fisicos (A y B) que tambien se pueden
 * presionar juntos (A+B). CODAL: uBit.buttonA / uBit.buttonB (Button),
 * isPressed() devuelve != 0 si esta presionado.
 *
 * Uso desde la IA (tool calling):
 *   -> comando serial:  SENSOR:BOTON
 *   <- respuesta:       BOTON:1:0      (A presionado, B suelto)
 *                       BOTON:0:1      (A suelto, B presionado)
 *                       BOTON:1:1      (ambos presionados)
 */
#ifndef FUNCION_BOTONES_H
#define FUNCION_BOTONES_H

#include "MicroBit.h"

extern MicroBit uBit;

// Devuelve el estado de los botones:
//   bit 0 (valor 1): boton A presionado
//   bit 1 (valor 2): boton B presionado
// Ej: 3 = A y B juntos, 1 = solo A, 0 = ninguno.
int leerBotones();

#endif // FUNCION_BOTONES_H
