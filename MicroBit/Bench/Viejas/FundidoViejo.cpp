/*
 * FundidoViejo.cpp - la version ANTERIOR (git show del commit previo), solo
 * para el bench. Funciones renombradas para linkearse junto a la nueva.
 * NO se compila en la firmware.
 */
/**
 * Fundido.cpp - Transicion FUNDIDO 🌫️ (60 FPS, ~2s)
 *
 * La cara actual se apaga SUAVEMENTE a negro (brillo global 90->0),
 * se dibuja la cara nueva y el brillo sube de 0->90: la nueva cara
 * "aparece" de la oscuridad.
 *
 * Usa el brillo GLOBAL (setBrightness) con pasos de 16ms = 60 FPS.
 */
#include "Fundido.h"

#define FRAMES_BR 62      // frames por mitad (62x2=124 ~2s)

// ---------------------------------------------------------------------------
// El FUNDIDO
// ---------------------------------------------------------------------------
void transicionFundidoVieja(EmocionActual destino)
{
    // 1) Apago la cara actual: brillo 90 -> 0 (fade out)
    for (int s = 0; s <= FRAMES_BR; s++) {
        uBit.display.setBrightness(90 - (90 * s) / FRAMES_BR);
        uBit.sleep(16);   // 60 FPS
    }

    // 2) Dibujo la cara nueva (a brillo 0 queda invisible)
    uBit.display.image.clear();
    dibujarCaraDestino(destino);

    // 3) La cara aparece: brillo 0 -> 90 (fade in)
    for (int s = 0; s <= FRAMES_BR; s++) {
        uBit.display.setBrightness((90 * s) / FRAMES_BR);
        uBit.sleep(16);
    }

    // Final limpio: dibujo una vez mas con el brillo de la emocion
    dibujarCaraDestino(destino);
}
