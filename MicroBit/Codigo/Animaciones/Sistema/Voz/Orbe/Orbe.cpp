/**
 * Orbe.cpp - Variante 2 del aro de voz: ORBE DE ENERGIA ⚡
 *
 * Como el orbe azul de los agentes de voz: una mancha borrosa que
 * tiembla. La tecnica: un "campo de ruido" (perlin-ish barato con senos
 * por pixel) se suma al halo del orbe. La AMPLITUD del ruido crece con
 * tu voz -> en silencio es un temblor tenue, al hablar hierve.
 *
 * Cada pixel: brillo = halo(d) * (0.65 + amplitud * ruido(pixel, tiempo))
 * El ruido usa 2 senos por eje con fases distintas por pixel: coherente
 * en el espacio (se ve "ondular", no estatica) y vivo en el tiempo.
 */
#include "Orbe.h"
#include "../Voz.h"
#include <math.h>

// Campo de ruido barato: [-1, 1], coherente por pixel y por tiempo
static float ruidoPixel(int x, int y, float t)
{
    const float KPI = 3.14159265f;
    float v = 0.0f;
    v += sinf(x * 1.7f + t * 2.2f);
    v += sinf(y * 1.3f - t * 1.8f);
    v += sinf((x + y) * 2.1f + t * 3.1f);
    v += sinf((x - y) * 1.1f - t * 1.4f);
    return v / 4.0f;   // ~[-1, 1]
}

void vozFrameOrbe()
{
    float n = vozNivelSuave() / 4.5f;          // 0..1
    float t = uBit.systemTime() / 1000.0f;

    // Radio del orbe: crece con la voz + respira suave
    float respiro = 0.12f * sinf(t * 2.6f);
    float radio = 1.05f + n * 1.25f + respiro * (1.0f - n * 0.5f);

    // Amplitud del temblor: crece con la voz (0.25 silencio -> 1.0 grito)
    float amplitud = 0.25f + n * 0.85f;

    // Brillo base: tenue respirando, pleno con voz
    float brillo = 80.0f + n * 175.0f;

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            float dx = x - 2.0f, dy = y - 2.0f;
            float d = sqrtf(dx * dx + dy * dy);

            // Halo del orbe: pleno adentro, se desvanece hacia afuera
            float halo = 1.0f - (d - radio) / 1.1f;
            if (halo < 0.0f) halo = 0.0f;
            if (halo > 1.0f) halo = 1.0f;
            halo = halo * halo;   // suaviza el borde

            // El ruido deforma el borde (mas fuerte cuanto mas lejos
            // del centro esta el pixel: el borde es lo que "hierve")
            float ruido = ruidoPixel(x, y, t);
            float borde = 1.0f - fabsf(d - radio) / 0.8f;
            if (borde < 0.0f) borde = 0.0f;
            if (borde > 1.0f) borde = 1.0f;
            float deform = 1.0f + amplitud * ruido * borde * 0.6f;
            if (deform < 0.0f) deform = 0.0f;

            int br = (int)(brillo * halo * deform);
            if (br > 255) br = 255;
            uBit.display.image.setPixelValue(x, y, br);
        }
    }

    uBit.sleep(16);   // ~60 fps
}
