/**
 * Anillos.cpp - Variante 3 del aro de voz: ANILLOS DOBLES 🪐
 *
 * Dos anillos concentricos:
 *   interno: sigue la voz AL INSTANTE (ataque rapido, mismo radio)
 *   externo: va ATRASADO (release lento, "persigue" al interno)
 * Cuando hablas, el interno salta y se abre una BRECHA con el externo;
 * al callarte, el externo lo alcanza y se funden de nuevo. En silencio
 * ambos respiran juntos (un solo puntito doble).
 *
 * Radios: 0.3 (puntito) -> 2.4 (pantalla llena). El externo parte de
 * una base mas grande (0.8) para que la brecha siempre sea visible.
 */
#include "Anillos.h"
#include "../Voz.h"
#include <math.h>

static float radioInterno = 0.3f;
static float radioExterno = 0.8f;

void vozFrameAnillos()
{
    float n = vozNivelSuave() / 4.5f;          // 0..1
    float t = uBit.systemTime() / 1000.0f;

    // Objetivo comun: crece con la voz + respiro
    float objetivo = 0.30f + n * 2.10f + 0.10f * sinf(t * 2.8f);

    // Interno: ataque rapido (sigue la voz casi al toque)
    radioInterno += (objetivo - radioInterno) * 0.35f;
    // Externo: release lento (va atrasado, persigue)
    radioExterno += (objetivo + 0.5f - radioExterno) * 0.06f;

    // Brillo: el interno mas fuerte (el que "habla"), el externo tenue
    float brilloInt = 110.0f + n * 145.0f;
    float brilloExt = 60.0f + n * 80.0f;

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            float dx = x - 2.0f, dy = y - 2.0f;
            float d = sqrtf(dx * dx + dy * dy);

            int br = 0;

            // Anillo interno (halo angosto y brillante)
            float cI = 1.0f - fabsf(d - radioInterno) / 0.7f;
            if (cI > 0.0f) {
                if (cI > 1.0f) cI = 1.0f;
                br += (int)(brilloInt * cI * cI);
            }

            // Anillo externo (halo mas ancho y tenue)
            float cE = 1.0f - fabsf(d - radioExterno) / 1.0f;
            if (cE > 0.0f) {
                if (cE > 1.0f) cE = 1.0f;
                br += (int)(brilloExt * cE * cE);
            }

            if (br > 255) br = 255;
            uBit.display.image.setPixelValue(x, y, br);
        }
    }

    uBit.sleep(16);   // ~60 fps
}
