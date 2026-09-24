/**
 * Morfosis.h - Transicion MORFOSIS 🧬
 */
#ifndef MORFOSIS_H
#define MORFOSIS_H

#include "../Transiciones.h"

// Los pixeles de la cara actual VIAJAN hasta formar la cara nueva
// (el primer frame). 60 FPS, ~1.2s.
void transicionMorfosis(EmocionActual destino);

#endif // MORFOSIS_H
