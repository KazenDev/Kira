/**
 * Acelerometro.cpp - FUNCION: leer el acelerometro del micro:bit
 *
 * CODAL: uBit.accelerometer.getX()/getY()/getZ() devuelven mili-g;
 * getPitch()/getRoll() devuelven grados. Los tres ejes usan el mismo
 * muestreo interno: se leen en una sola pasada para que sean coherentes.
 */
#include "Acelerometro.h"

LecturaAcelerometro leerAcelerometro()
{
    LecturaAcelerometro l;
    l.x = uBit.accelerometer.getX();
    l.y = uBit.accelerometer.getY();
    l.z = uBit.accelerometer.getZ();
    l.pitch = uBit.accelerometer.getPitch();
    l.roll = uBit.accelerometer.getRoll();
    return l;
}
