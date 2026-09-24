/**
 * Acelerometro.h - FUNCION: leer el acelerometro del micro:bit
 *
 * El micro:bit v2 tiene un acelerometro de 3 ejes (LSM303AGR) que mide
 * la aceleracion en mili-g (1000 = 1g = gravedad). Tambien da la
 * inclinacion (pitch/roll) en grados.
 *
 * Uso desde la IA (tool calling):
 *   -> comando serial:  SENSOR:ACCEL
 *   <- respuesta:       ACCEL:0:1000:5:0:0
 *                       (X=0, Y=1000 -> parado derecho, Z=5, pitch=0, roll=0)
 *
 * Referencia rapida:
 *   - Acostado boca arriba:   Z ≈ 1000, X ≈ 0, Y ≈ 0
 *   - Parado vertical:        Y ≈ 1000 (o X, segun como lo tengas)
 *   - Sacudiendolo:           valores cambian rapido y el pitch/roll baila
 */
#ifndef FUNCION_ACELEROMETRO_H
#define FUNCION_ACELEROMETRO_H

#include "MicroBit.h"

extern MicroBit uBit;

// Estructura con la lectura completa del acelerometro
struct LecturaAcelerometro
{
    int x;       // mili-g
    int y;       // mili-g
    int z;       // mili-g
    int pitch;   // grados (-180..180): inclinacion hacia adelante/atras
    int roll;    // grados (-180..180): inclinacion hacia los lados
};

// Lee todos los ejes + inclinacion de una vez.
LecturaAcelerometro leerAcelerometro();

#endif // FUNCION_ACELEROMETRO_H
