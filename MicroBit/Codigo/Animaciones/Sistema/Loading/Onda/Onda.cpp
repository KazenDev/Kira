/**
 * Onda.cpp - Patron de carga 8: ONDA INTERACTIVA 🌊 (v2)
 *
 * v2 (SENOIDAL REAL + ACELEROMETRO): adios a la tabla de 16 valores
 * y a los 25fps. Ahora es una ONDA SENOIDAL DE VERDAD (sinf con la
 * FPU del nRF52833) a 60fps, que RESPONDE AL MOVIMIENTO:
 *
 *   - INCLINACION (getX) = VELOCIDAD y DIRECCION: inclinas a un lado,
 *     la onda viaja mas rapido hacia ese lado; la devuelves al centro,
 *     se frena. Como agua que fluye cuesta abajo.
 *   - JERK (cambio frame a frame) = AGITACION: al mover la placa
 *     rapido, la amplitud de la onda sube y tiembla (el mar se
 *     alborota) y vuelve a calmarse al quedarse quieta.
 *   - RAFAGAS: cada movimiento brusco acumula energia que DECAE
 *     lento, sumando olas extra a la base (como tirar una piedra
 *     al agua: las ondas se expanden y se apagan solas).
 *
 * Visual: linea senoidal sub-pixel (interpolacion vertical del brillo
 * para que la cresta se deslice fluido) + rastro que se desvanece.
 *
 * v1: tabla senoidal de 16 valores, 25fps, sin interactividad.
 */
#include "Onda.h"
#include "../LoadingBase.h"
#include <math.h>

// Estado continuo (bucle sin saltos)
static float fase = 0.0f;        // fase de la onda (radianes)
static float amp = 1.2f;         // amplitud actual (suavizada)
static float energia = 0.0f;     // rafagas acumuladas (decae lento)
static float gxPrev = 0.0f;      // lecturas previas (jerk en X e Y)
static float gyPrev = 0.0f;
static float vel = 0.12f;        // velocidad de la onda (suavizada)
static bool  iniciado = false;

// Pinta la cresta de la onda en la columna x con SUB-PIXEL vertical:
// el brillo se reparte entre los dos pixeles segun la fraccion, asi la
// cresta se desliza fluido entre filas (nada de saltos).
static void pintarCresta(int x, float yf, int base)
{
    int yA = (int)yf;                  // piso
    float frac = yf - (float)yA;       // 0..1
    int yB = yA + 1;
    int brA = (int)(base * (1.0f - frac));
    int brB = (int)(base * frac);
    // solo escribir si hay brillo: asi el rastro se desvanece natural
    if (yA >= 0 && yA <= 4 && brA > 0)
        uBit.display.image.setPixelValue(x, yA, brA);
    if (yB >= 0 && yB <= 4 && brB > 0)
        uBit.display.image.setPixelValue(x, yB, brB);
}

void animarOnda(int vueltas)
{
    static int dbg = 0;

    for (int v = 0; v < vueltas; v++) {
        for (int f = 0; f < 400; f++) {              // ~6.4s por pasada
            if (frameRastro(78)) return;             // rastro + abortar serial

            // --- Lecturas del acelerometro ---
            float gx = (float)uBit.accelerometer.getX() / 1000.0f;
            float gy = (float)uBit.accelerometer.getY() / 1000.0f;
            if (gx > 1.0f) gx = 1.0f; if (gx < -1.0f) gx = -1.0f;
            if (gy > 1.0f) gy = 1.0f; if (gy < -1.0f) gy = -1.0f;

            // Jerk: que tan violento es el movimiento frame a frame
            // (usa AMBOS ejes: una sacudida vertical tambien agita)
            float jerk = fabsf(gx - gxPrev) + fabsf(gy - gyPrev);
            gxPrev = gx; gyPrev = gy;

            // --- INCLINACION = velocidad y direccion ---
            // Placa plana: ~0.12 rad/frame (lento). Inclinada: hasta
            // ~0.42. Signo de gx = sentido (hacia el lado inclinado).
            // Se suaviza para que una sacudida no haga temblar la
            // direccion (el mar no cambia de rumbo de golpe).
            vel += ((0.12f + gx * 0.30f) - vel) * 0.08f;
            fase += vel;

            // --- JERK = agitacion de la amplitud ---
            // Objetivo: base 1.2 + la violencia del movimiento. Se
            // suaviza (no salta) y al calmarse la placa, vuelve.
            float ampObj = 1.2f + jerk * 3.0f;
            if (ampObj > 2.4f) ampObj = 2.4f;
            amp += (ampObj - amp) * 0.12f;

            // --- RAFAGAS: cada movimiento brusco acumula energia ---
            // que decae lento (0.98/frame) sumando olas extra.
            if (jerk > 0.30f) energia += jerk * 1.5f;
            if (energia > 6.0f) energia = 6.0f;
            energia *= 0.98f;

            float ampTotal = amp + energia * 0.20f;
            if (ampTotal > 2.6f) ampTotal = 2.6f;

            // --- Dibujar la senoidal real (5 columnas) ---
            // y = centro(2) + sin(fase + x*2pi/5) * amplitud
            const float DOS_PI = 6.28318531f;
            for (int x = 0; x < 5; x++) {
                float val = sinf(fase + (float)x * DOS_PI / 5.0f);
                float yf = 2.0f + val * ampTotal;
                if (yf < 0.0f) yf = 0.0f;
                if (yf > 4.0f) yf = 4.0f;
                // brillo de la cresta: 255 - x*18 (adelante mas vivo)
                pintarCresta(x, yf, 255 - x * 18);
            }

            // debug: inclinacion (gx), amplitud y energia cada ~1.2s
            if (++dbg >= 75) {
                dbg = 0;
                uBit.serial.printf("O%d %d A%d E%d\n",
                    (int)(gx * 1000.0f), (int)(gy * 1000.0f),
                    (int)(ampTotal * 100.0f), (int)(energia * 100.0f));
            }

            uBit.sleep(16);              // ~60 fps
        }
    }
    // NO limpia al final: bucle continuo (estado estatico, sin saltos)
}
