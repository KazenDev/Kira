/**
 * Fundido.h - Transicion FUNDIDO 🌫️
 */
#ifndef FUNDIDO_H
#define FUNDIDO_H

#include "../Transiciones.h"

// La pantalla se apaga suavemente a negro, se dibuja la cara nueva
// y aparece con fade. 60 FPS, ~2.0s (126 frames).
void transicionFundido(EmocionActual destino);

#endif // FUNDIDO_H
