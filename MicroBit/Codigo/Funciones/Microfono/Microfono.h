/**
 * Microfono.h - FUNCION: tomar una lectura del microfono onboard
 *
 * El microfono ya alimenta el FFT del modo Barra/VOZ. Esta funcion reutiliza esa
 * misma infraestructura para una lectura puntual para la IA, sin agregar otro
 * pipeline ni mantener el microfono encendido todo el tiempo.
 *
 * La lectura es RELATIVA (0..100), no una medicion acustica calibrada en dB:
 * incluye el nivel y cinco bandas de frecuencia (graves -> agudos).
 */
#ifndef FUNCION_MICROFONO_H
#define FUNCION_MICROFONO_H

#include "MicroBit.h"

struct LecturaMicrofono
{
    int nivel;       // maximo de las cinco bandas, relativo 0..100
    int bandas[5];   // graves, medios-bajos, medios, medios-altos, agudos
    int ventanas;    // ventanas FFT nuevas consumidas para esta lectura
    int estado;      // 1 = ok, 0 = ocupado, -1 = no se pudo muestrear
};

LecturaMicrofono leerMicrofono();

#endif // FUNCION_MICROFONO_H
