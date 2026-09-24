/**
 * Botones.cpp - FUNCION: leer el estado de los botones A y B
 *
 * CODAL: uBit.buttonA.isPressed() / uBit.buttonB.isPressed() devuelven
 * un int distinto de cero si el boton esta presionado en ese momento.
 * A+B juntos se detecta con ambos valores a la vez.
 */
#include "Botones.h"

int leerBotones()
{
    int estado = 0;
    if (uBit.buttonA.isPressed())
        estado |= 1;   // bit 0 = boton A
    if (uBit.buttonB.isPressed())
        estado |= 2;   // bit 1 = boton B
    return estado;
}
