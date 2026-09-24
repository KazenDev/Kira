/**
 * Cometa.cpp - Variante 4 del aro de voz: COMETA DE VOZ ☄️
 *
 * El radio del aro PERSIGUE tu nivel de voz con rastro:
 *   - al hablar, el radio salta ADELANTE (ataque casi instantaneo)
 *   - al callarte, retrocede LENTO (release largo)
 *   - deja una ESTELA: las ultimas posiciones del radio se guardan y
 *     se pintan como anillos fantasma que se desvanecen (rastro real,
 *     como el del cometa del loading 0).
 *
 * Fisica del rastro: buffer circular de radios recientes; cada entrada
 * tiene una "edad" y su brillo decae con ella. El anillo vivo es el mas
 * reciente (brillante); los demas son fantasmas tenues.
 */
#include "Cometa.h"
#include "../Voz.h"
#include <math.h>

#define RASTRO 6   // cuantos radios fantasma guardamos

static float radioVivo = 0.3f;        // el anillo que persigue el nivel
static float rastro[RASTRO];          // posiciones recientes (mas viejas al final)
static float rastroBrillo[RASTRO];    // brillo de cada fantasma (decae)

void vozFrameCometa()
{
    float n = vozNivelSuave() / 4.5f;          // 0..1
    float t = uBit.systemTime() / 1000.0f;

    // Objetivo: puntito en silencio -> pantalla llena al gritar
    float objetivo = 0.30f + n * 2.10f + 0.10f * sinf(t * 3.0f);

    // El cometa: ataque rapido (salta al hablar), release MUY lento
    float dif = objetivo - radioVivo;
    radioVivo += dif * (dif > 0.0f ? 0.40f : 0.05f);

    // Desplazar el rastro: lo nuevo al principio, lo viejo se corre
    for (int i = RASTRO - 1; i > 0; i--) {
        rastro[i] = rastro[i - 1];
        rastroBrillo[i] = rastroBrillo[i - 1];
    }
    rastro[0] = radioVivo;
    rastroBrillo[0] = 1.0f;
    // El brillo de cada fantasma decae con su antiguedad
    for (int i = RASTRO - 1; i > 0; i--)
        rastroBrillo[i] *= 0.62f;

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            float dx = x - 2.0f, dy = y - 2.0f;
            float d = sqrtf(dx * dx + dy * dy);

            int br = 0;

            // Fantasmas (mas viejos = mas tenues)
            for (int i = RASTRO - 1; i >= 1; i--) {
                if (rastroBrillo[i] <= 0.03f) continue;
                float cerca = 1.0f - fabsf(d - rastro[i]) / 0.8f;
                if (cerca > 0.0f) {
                    if (cerca > 1.0f) cerca = 1.0f;
                    br += (int)(120.0f * rastroBrillo[i] * cerca);
                }
            }

            // El cometa vivo (brillante)
            float cercaV = 1.0f - fabsf(d - radioVivo) / 0.7f;
            if (cercaV > 0.0f) {
                if (cercaV > 1.0f) cercaV = 1.0f;
                br += (int)(230.0f * cercaV * cercaV);
            }

            if (br > 255) br = 255;
            uBit.display.image.setPixelValue(x, y, br);
        }
    }

    uBit.sleep(16);   // ~60 fps
}
