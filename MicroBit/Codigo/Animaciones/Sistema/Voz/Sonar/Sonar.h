/**
 * Sonar.h - Variante 1 del aro de voz: SONAR / ECOS 📡
 *
 * Cada palabra emite una ONDA que viaja del centro hacia afuera y se
 * desvanece (como el ping de un radar). Si hablas seguido, las ondas
 * se apilan y viajan juntas. En silencio: puntito respirando.
 */
#ifndef VOZ_SONAR_H
#define VOZ_SONAR_H

#include "MicroBit.h"

extern MicroBit uBit;

// Un frame del sonar a 60fps (lo llama el director Voz)
void vozFrameSonar();

#endif // VOZ_SONAR_H
