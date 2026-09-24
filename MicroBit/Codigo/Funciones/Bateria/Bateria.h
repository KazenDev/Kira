/**
 * Bateria.h - FUNCION: leer la alimentacion de la micro:bit
 *
 * El PowerManager de CODAL consulta el chip de interfaz y expone el voltaje
 * de bateria, el voltaje de entrada y la fuente activa. Los valores se
 * redondean a milivoltios para que el protocolo siga siendo legible por
 * serial y facil de parsear en el backend.
 */
#ifndef FUNCION_BATERIA_H
#define FUNCION_BATERIA_H

#include "MicroBit.h"

struct LecturaBateria
{
    int bateria_mv;  // voltaje de bateria; 0 = no disponible
    int vin_mv;      // voltaje de entrada; 0 = no disponible
    int fuente;      // 0 ninguna, 1 USB, 2 bateria, 3 USB + bateria
};

LecturaBateria leerBateria();

#endif // FUNCION_BATERIA_H
