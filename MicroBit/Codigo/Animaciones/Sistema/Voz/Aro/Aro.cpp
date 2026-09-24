/**
 * Aro.cpp - Variante 0 del aro de voz: ARO QUE CRECE 🗣️🌀
 *
 * El clasico del orbe de los agentes de voz: un aro centrado que crece
 * con la fuerza de tu voz y respira en silencio.
 *
 * Geometria (centro 2,2; distancia por LED):
 *   radio ~0.3  -> puntito central
 *   radio ~1.0  -> aro 3x3
 *   radio ~2.0  -> borde 5x5 completo
 *   radio ~2.4+ -> pantalla llena (grito)
 * Cada LED se enciende segun que tan cerca esta del radio actual, con
 * halo suave -> el aro "respira" y se estira sin cortes.
 */
#include "Aro.h"
#include "../Voz.h"
#include <math.h>

void vozFrameAro()
{
    // Nivel real del microfono, suavizado (compartido por todas las voces)
    float n = vozNivelSuave() / 4.5f;          // normalizado 0..1

    // Respiro: oscilacion lenta (periodo ~2s). En silencio es notorio;
    // con voz se atenua (el aro sigue tu voz)
    float t = uBit.systemTime() / 1000.0f;
    float osc = sinf(t * 3.1f);
    float respiro = 0.13f * osc * (1.0f - n * 0.6f);

    // Radio: puntito en silencio -> pantalla llena al gritar
    float radio = 0.30f + n * 2.10f + respiro;

    // Brillo: tenue respirando, pleno con voz
    float brillo = 90.0f + n * 165.0f;
    brillo += 40.0f * (1.0f - n) * (0.5f + 0.5f * osc);

    // Pintar: cada LED segun que tan cerca esta del radio (halo)
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            float dx = x - 2.0f, dy = y - 2.0f;
            float d = sqrtf(dx * dx + dy * dy);
            float cerca = 1.0f - fabsf(d - radio) / 0.9f;
            if (cerca < 0.0f) cerca = 0.0f;
            if (cerca > 1.0f) cerca = 1.0f;
            int br = (int)(brillo * cerca * cerca);
            if (br > 255) br = 255;
            uBit.display.image.setPixelValue(x, y, br);
        }
    }

    uBit.sleep(16);   // ~60 fps
}
