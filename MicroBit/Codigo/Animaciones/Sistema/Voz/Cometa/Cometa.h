/**
 * Cometa.h - Variante 4 del aro de voz: COMETA DE VOZ ☄️
 *
 * El radio del aro PERSIGUE tu nivel de voz con rastro: salta adelante
 * cuando hablas y deja una estela de anillos que se desvanecen mientras
 * retrocede lentamente. Parecido al aro clasico pero con el efecto
 * rastro del cometa (posiciones anteriores que se apagan).
 */
#ifndef VOZ_COMETA_H
#define VOZ_COMETA_H

#include "MicroBit.h"

extern MicroBit uBit;

// Un frame del cometa a 60fps (lo llama el director Voz)
void vozFrameCometa();

#endif // VOZ_COMETA_H
