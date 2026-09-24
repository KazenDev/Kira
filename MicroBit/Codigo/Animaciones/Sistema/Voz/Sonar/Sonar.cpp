/**
 * Sonar.cpp - Variante 1 del aro de voz: SONAR / ECOS 📡
 *
 * Cada "palabra" (cuando la voz pasa de silencio a hablar) emite una
 * ONDA que viaja del centro hacia afuera y se desvanece. Si hablas
 * seguido las ondas se apilan y viajan juntas, como el ping de un
 * radar. En silencio queda solo el puntito central respirando.
 *
 * Fisica de cada onda:
 *   radio crece a velocidad constante (~1.3 unidades/s)
 *   vida decae de 1.0 a 0 (brillo = vida * fuerza)
 *   el LED se enciende segun que tan cerca esta del radio (halo)
 *
 * Emision: cuando el nivel suavizado supera ~0.2 (de 4.5) se emite una
 * onda, con un minimo de 250ms entre ondas -> si hablas seguido, pings
 * continuos; una palabra sola emite un ping unico.
 */
#include "Sonar.h"
#include "../Voz.h"
#include <math.h>

#define MAX_ONDAS 5
#define VEL_ONDAS  1.3f     // unidades de radio por segundo
#define VIDA_ONDAS 1.1f     // segundos que vive una onda
#define MIN_PING   0.25f    // minimo entre ondas (segundos)

// Onda activa: radio (0..~3) y vida (1.0 nueva -> 0.0 muerta)
static float ondasRadio[MAX_ONDAS];
static float ondasVida[MAX_ONDAS];
static float ultimoPing = 0.0f;   // cuando se emitio la ultima onda
static bool  hablando  = false;   // la voz estaba activa el frame pasado

void vozFrameSonar()
{
    float t = uBit.systemTime() / 1000.0f;
    float n = vozNivelSuave() / 4.5f;          // 0..1
    float dt = 1.0f / 60.0f;                   // un frame ~16ms

    // --- Emitir onda al empezar a hablar (o si seguís, cada 250ms) ---
    if (n > 0.20f && (t - ultimoPing) > MIN_PING) {
        // Busca el slot mas viejo (o el primero libre)
        int slot = 0;
        float masVieja = ondasVida[0];
        for (int i = 1; i < MAX_ONDAS; i++) {
            if (ondasVida[i] < masVieja) { masVieja = ondasVida[i]; slot = i; }
        }
        ondasRadio[slot] = 0.35f;
        ondasVida[slot]  = 1.0f;
        ultimoPing = t;
    }
    hablando = (n > 0.20f);

    // --- Avanzar ondas ---
    for (int i = 0; i < MAX_ONDAS; i++) {
        if (ondasVida[i] <= 0.0f) continue;
        ondasRadio[i] += VEL_ONDAS * dt;
        ondasVida[i]  -= dt / VIDA_ONDAS;
        if (ondasVida[i] < 0.0f) ondasVida[i] = 0.0f;
    }

    // --- Respiro del puntito central (silencio) ---
    float osc = sinf(t * 3.1f);
    float brilloCentro = 70.0f + n * 150.0f
                       + 35.0f * (1.0f - n) * (0.5f + 0.5f * osc);

    // --- Pintar: puntito + todas las ondas ---
    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            float dx = x - 2.0f, dy = y - 2.0f;
            float d = sqrtf(dx * dx + dy * dy);

            int br = 0;

            // Puntito central (halo angosto)
            float cercaC = 1.0f - fabsf(d - 0.25f) / 0.6f;
            if (cercaC > 0.0f) {
                if (cercaC > 1.0f) cercaC = 1.0f;
                br += (int)(brilloCentro * cercaC);
            }

            // Ondas: cada una aporta su halo * vida
            for (int i = 0; i < MAX_ONDAS; i++) {
                if (ondasVida[i] <= 0.0f) continue;
                float cerca = 1.0f - fabsf(d - ondasRadio[i]) / 0.7f;
                if (cerca > 0.0f) {
                    if (cerca > 1.0f) cerca = 1.0f;
                    br += (int)(230.0f * ondasVida[i] * cerca * cerca);
                }
            }

            if (br > 255) br = 255;
            uBit.display.image.setPixelValue(x, y, br);
        }
    }

    uBit.sleep(16);   // ~60 fps
}
