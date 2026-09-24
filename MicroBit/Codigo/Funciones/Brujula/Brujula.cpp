/**
 * Brujula.cpp - FUNCION: rumbo magnetico sin lanzar una calibracion bloqueante
 *
 * heading() puede abrir la UX de calibracion de CODAL. Un tool de IA no
 * debe arrancar una secuencia de 30 segundos sin avisar, por eso aqui solo
 * se devuelve el estado de calibracion y el campo magnetico disponible.
 */
#include "Brujula.h"
#include "MicroBit.h"

extern MicroBit uBit;

LecturaBrujula leerBrujula()
{
    LecturaBrujula l;
    l.rumbo = -1;
    l.campo = uBit.compass.getFieldStrength();
    l.calibrada = uBit.compass.isCalibrated() ? 1 : 0;

    if (l.calibrada) {
        int rumbo = uBit.compass.heading();
        if (rumbo >= 0 && rumbo < 360) l.rumbo = rumbo;
    }
    return l;
}
