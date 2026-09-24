/**
 * Aro.h - Variante 0 del aro de voz: ARO QUE CRECE 🗣️🌀
 *
 * El clasico: un aro centrado que crece con la fuerza de tu voz y
 * respira en silencio. Pintado con sub-pixel a 60fps (radio fluido,
 * sin saltos de a LED).
 */
#ifndef VOZ_ARO_H
#define VOZ_ARO_H

#include "MicroBit.h"

extern MicroBit uBit;

// Un frame del aro a 60fps (lo llama el director Voz)
void vozFrameAro();

#endif // VOZ_ARO_H
