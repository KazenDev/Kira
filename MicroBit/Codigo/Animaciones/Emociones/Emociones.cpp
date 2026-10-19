/**
 * Emociones.cpp - Registro de las 8 personalidades
 *
 * (El Corazon ❤️ se elimino como emocion: ahora es la reaccion CARINO,
 * cuando la tocan varias veces o le dejan el dedo en el logo.)
 */
#include "Emociones.h"

static const Personalidad *const TABLA[NUM_EMOCIONES] = {
    &PERS_ALEGRIA,       // EM_ALEGRIA
    &PERS_TRISTE,        // EM_TRISTE
    &PERS_ENOJADO,       // EM_ENOJADO
    &PERS_SORPRENDIDO,   // EM_SORPRENDIDO
    &PERS_NEUTRAL,       // EM_NEUTRAL
    &PERS_FASTIDIO,      // EM_FASTIDIO
    &PERS_MIEDO,         // EM_MIEDO
    &PERS_CANSADO,       // EM_CANSADO
};

const Personalidad *personalidadDe(int emocion)
{
    if (emocion < 0 || emocion >= NUM_EMOCIONES) return TABLA[0];
    return TABLA[emocion];
}
