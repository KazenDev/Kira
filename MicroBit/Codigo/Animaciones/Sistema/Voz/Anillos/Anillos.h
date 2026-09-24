/**
 * Anillos.h - Variante 3 del aro de voz: ANILLOS DOBLES 🪐
 *
 * Dos anillos concentricos: el INTERNO sigue tu voz al instante
 * (ataque rapido) y el EXTERNO va atrasado (release lento). Cuando
 * hablas se abre una brecha entre ambos — como si el sonido empujara
 * al anillo interno hacia afuera y el externo lo persiguiera.
 */
#ifndef VOZ_ANILLOS_H
#define VOZ_ANILLOS_H

#include "MicroBit.h"

extern MicroBit uBit;

// Un frame de los anillos a 60fps (lo llama el director Voz)
void vozFrameAnillos();

#endif // VOZ_ANILLOS_H
