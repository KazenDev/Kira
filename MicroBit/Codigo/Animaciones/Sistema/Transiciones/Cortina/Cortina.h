/**
 * Cortina.h - Transicion CORTINA 🎭
 */
#ifndef CORTINA_H
#define CORTINA_H

#include "../Transiciones.h"

// Las columnas se cierran de los bordes al centro (como una cortina),
// se dibuja la cara nueva y se abren del centro a los bordes.
// 60 FPS, ~2.0s (126 frames).
void transicionCortina(EmocionActual destino);

#endif // CORTINA_H
