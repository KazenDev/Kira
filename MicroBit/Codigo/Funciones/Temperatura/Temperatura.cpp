/**
 * Temperatura.cpp - FUNCION: leer la temperatura ambiente del micro:bit
 *
 * CODAL: uBit.thermometer.getTemperature() devuelve int en grados Celsius.
 * El sensor es el termometro interno del SoC nRF52833 (v2). Para que la
 * IA pueda "sentir" el ambiente sin esperar el muestreo lento por defecto,
 * forzamos una muestra nueva con updateSample().
 */
#include "Temperatura.h"

int leerTemperatura()
{
    // updateSample() lee el sensor AHORA (no espera el periodo interno)
    uBit.thermometer.updateSample();
    return uBit.thermometer.getTemperature();
}
