/**
 * Tactil.cpp - FUNCION: estado del logo capacitivo
 */
#include "Tactil.h"
#include "MicroBit.h"

extern MicroBit uBit;

LecturaTactil leerToque()
{
    LecturaTactil l;
    l.presionado = uBit.logo.isPressed() ? 1 : 0;
    l.lectura = uBit.logo.getValue();
    return l;
}
