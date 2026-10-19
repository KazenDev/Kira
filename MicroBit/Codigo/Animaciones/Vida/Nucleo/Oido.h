/**
 * Oido.h - El OIDO de Kira: nota a la gente por el ruido (solo el NIVEL)
 *
 * No graba ni manda audio: recibe un numero de "cuanto ruido hay" (0..100)
 * varias veces por segundo y decide dos cosas:
 *
 *   VOCES    alguien se puso a hablar cerca DESPUES de un rato de calma
 *            (subio sostenido sobre la base al menos 1,2 s, y antes hubo 40 s
 *            sin bulla). Es el "¿quien anda ahi?".
 *   APLAUSO  un golpe de ruido de golpe y muy por encima de la base.
 *
 * La BASE se adapta sola al lugar: baja rapido cuando hay silencio y sube
 * lento con el ruido sostenido. Asi el bullicio constante de una feria NO es
 * un evento (es la base nueva); lo que cuenta es el CAMBIO.
 *
 * "Sordo": mientras la propia Kira suena (sus sonidos, o su voz cuando la IA
 * habla) no se escucha: si no, reaccionaria a si misma.
 */
#ifndef VIDA_OIDO_H
#define VIDA_OIDO_H

#include <stdint.h>

enum EventoOido
{
    OIDO_NADA = 0,
    OIDO_VOCES,
    OIDO_APLAUSO
};

struct DetectorOido
{
    bool     listo;
    float    rapido;        // nivel suavizado (~0,25 s)
    float    base;          // el piso del lugar (se adapta)
    float    anterior;      // la muestra anterior (para los golpes)
    uint32_t ultimoT;
    bool     enBulla;
    bool     avisado;
    uint32_t altoDesde;
    uint32_t finBulla;      // cuando termino la ultima bulla
    uint32_t silencioPrevio;
    uint32_t ultimoVoces;
    uint32_t ultimoAplauso;
    uint32_t arranque;      // cuando empezo a escuchar (periodo de ajuste)
};

void oidoIniciar(DetectorOido &d, uint32_t t);

// Una muestra de nivel (0..100). Devuelve un EventoOido.
int oidoMuestra(DetectorOido &d, float nivel, uint32_t t, bool sordo);

// Cuanto mas que la base hay ahora (0 = el ruido de siempre)
float oidoActividad(const DetectorOido &d);

#endif // VIDA_OIDO_H
