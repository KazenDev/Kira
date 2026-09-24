/**
 * Orbe.h - Variante 2 del aro de voz: ORBE DE ENERGIA ⚡
 *
 * La mas parecida al orbe real de ChatGPT: una mancha borrosa de LEDs
 * que TIEMBLA y HIERVE. El borde se agita con ruido aleatorio que
 * aumenta con tu voz: en silencio tiembla lento y tenue, cuando hablas
 * se vuelve inestable y brillante.
 */
#ifndef VOZ_ORBE_H
#define VOZ_ORBE_H

#include "MicroBit.h"

extern MicroBit uBit;

// Un frame del orbe a 60fps (lo llama el director Voz)
void vozFrameOrbe();

#endif // VOZ_ORBE_H
