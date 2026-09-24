/**
 * Bateria.cpp - FUNCION: lectura de voltaje y fuente de alimentacion
 *
 * getPowerData() consulta el KL27/interface del micro:bit. No todos los modos
 * de alimentacion exponen una lectura de bateria; por eso 0 se conserva como
 * "no disponible" y no se inventa un porcentaje.
 */
#include "Bateria.h"

extern MicroBit uBit;

static int microvoltsAMillivolts(uint32_t uv)
{
    if (uv == 0 || uv > 10000000) return 0;
    return (int)((uv + 500) / 1000);
}

LecturaBateria leerBateria()
{
    LecturaBateria l;
    l.bateria_mv = 0;
    l.vin_mv = 0;
    l.fuente = 0;

    MicroBitPowerData data = uBit.power.getPowerData();
    l.bateria_mv = microvoltsAMillivolts(data.batteryMicroVolts);
    l.vin_mv = microvoltsAMillivolts(data.vinMicroVolts);
    l.fuente = (int)uBit.power.getPowerSource();
    return l;
}
