/**
 * Gestos.cpp - FUNCION: gesto estable y magnitud de aceleracion
 *
 * No intenta inventar velocidad: el acelerometro no la mide directamente.
 * Si se necesita una estimacion de movimiento, la magnitud y el gesto son
 * datos honestos; una velocidad por integracion acumularia mucho error.
 */
#include "Gestos.h"
#include "MicroBit.h"
#include <math.h>

extern MicroBit uBit;

LecturaGesto leerGesto()
{
    LecturaGesto l;
    Sample3D s = uBit.accelerometer.getSample();
    unsigned long suma = (unsigned long)s.x * (unsigned long)s.x;
    suma += (unsigned long)s.y * (unsigned long)s.y;
    suma += (unsigned long)s.z * (unsigned long)s.z;

    l.codigo = (int)uBit.accelerometer.getGesture();
    l.magnitud_mg = (int)sqrtf((float)suma);
    return l;
}
