/**
 * Emociones.h - Las 8 PERSONALIDADES (las caras del micro:bit)
 *
 * CADA emocion vive en su propia carpeta con su personalidad para el motor
 * de vida (Animaciones/Vida/): como parpadea, como mira, como habla, sus
 * gestos y su dibujo. El motor es el mismo para todas.
 *
 *   Alegria/  Triste/  Enojado/  Sorprendido/
 *   Neutral/  Fastidio/  Miedo/  Cansado/
 *
 * Este header es PURO (no incluye CODAL): el bench de host lo compila tal
 * cual. El orden de la tabla es el del enum EmocionActual (Sistema.h).
 */
#ifndef EMOCIONES_H
#define EMOCIONES_H

#include "Alegria/Alegria.h"
#include "Triste/Triste.h"
#include "Enojado/Enojado.h"
#include "Sorprendido/Sorprendido.h"
#include "Neutral/Neutral.h"
#include "Fastidio/Fastidio.h"
#include "Miedo/Miedo.h"
#include "Cansado/Cansado.h"

#define NUM_EMOCIONES 8

// La personalidad de cada emocion, en el orden de EmocionActual
const Personalidad *personalidadDe(int emocion);

#endif // EMOCIONES_H
