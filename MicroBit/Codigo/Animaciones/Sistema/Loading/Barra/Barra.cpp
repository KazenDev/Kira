/**
 * Barra.cpp - Patron de carga 4: ECUALIZADOR REAL 🎚️⚡ (v3)
 *
 * v3 (ESPECTRO REAL): las barras ya no bailan al azar. Ahora leen las
 * 5 bandas de frecuencia REALES del microfono (modulo MicFft):
 *   - Bajos de verdad a la izquierda, agudos de verdad a la derecha
 *   - Si tocas una nota grave, la barra izquierda salta sola; un silbido,
 *     la derecha
 *   - Conserva todo el mimo de v2: alturas float + sub-pixel a 60fps,
 *     ataque rapido / release lento, picos que decaen, gradiente
 *   - Debug: cada ~1.2s imprime las 5 bandas por serial para calibrar
 *     (B0..B4 en 0..90)
 *
 * v2: ecualizador simulado con beats al azar (personalidad por banda).
 */
#include "Barra.h"
#include "../LoadingBase.h"
#include "../../MicFft/MicFft.h"

static float alturas[5]   = {0, 0, 0, 0, 0};   // altura actual de cada barra
static float objetivos[5] = {0, 0, 0, 0, 0};   // altura a donde quiere ir
static float picos[5]     = {0, 0, 0, 0, 0};   // maximo alcanzado (decae solo)

// Movimiento suave + pintado de las 5 barras con sub-pixel y picos
static void actualizarYpintar()
{
    for (int x = 0; x < 5; x++) {
        // ataque rapido, release lento (el rebote del ecualizador)
        float dif = objetivos[x] - alturas[x];
        float paso = (dif > 0.0f) ? 0.30f : 0.13f;
        alturas[x] += dif * paso;
        if (dif < 0.08f && dif > -0.08f) alturas[x] = objetivos[x];  // llego
        if (alturas[x] < 0.0f) alturas[x] = 0.0f;
        if (alturas[x] > 4.5f) alturas[x] = 4.5f;

        // pico: sube con la barra, decae solo
        if (alturas[x] > picos[x]) picos[x] = alturas[x];
        picos[x] -= 0.05f;
        if (picos[x] < 0.0f) picos[x] = 0.0f;

        // pintar columna: cuerpo con gradiente + punta parcial
        for (int y = 0; y < 5; y++) {
            float dist = 4 - y;                    // 4 abajo, 0 arriba
            float cov = alturas[x] - dist;         // cuanto cubre esta fila
            int br;
            if (cov >= 1.0f)
                br = 150 + (int)(105.0f * dist / 4.0f);   // cuerpo (gradiente)
            else if (cov > 0.0f)
                br = (int)(255.0f * cov);                  // punta parcial
            else
                br = 0;                                    // fuera de la barra
            uBit.display.image.setPixelValue(x, y, br);
        }
        // pixel de pico: marca el maximo, se desvanece al caer
        if (picos[x] > 0.0f) {
            int pRow = (int)(4.0f - picos[x] + 0.5f);
            if (pRow < 0) pRow = 0;
            if (pRow > 4) pRow = 4;
            int pb = (int)(picos[x] * 56.0f);
            if (pb > 255) pb = 255;
            uBit.display.image.setPixelValue(x, pRow, pb);
        }
    }
}

void animarBarra(int ciclos)
{
    // Idempotente: si el mic ya esta corriendo no hace nada; si lo
    // apagamos con micFftDetener() al salir, lo reactiva (stream + corriente).
    micFftIniciar();

    static int dbg = 0;                // contador para el debug serial

    for (int c = 0; c < ciclos; c++) {
        for (int f = 0; f < 400; f++) {                // ~6.4s por ciclo
            if (frameSerial()) return;                 // abortar si manda algo

            // objetivos = bandas REALES del microfono (0..4.5)
            const float *b = micFftBandas();
            for (int x = 0; x < 5; x++) objetivos[x] = b[x];

            actualizarYpintar();

            // debug: imprime bandas + ventanas FFT cada ~1.2s (calibrar con tonos)
            if (++dbg >= 75) {
                dbg = 0;
                uBit.serial.printf("B%d %d %d %d %d W%d\n",
                    (int)(b[0] * 20.0f), (int)(b[1] * 20.0f),
                    (int)(b[2] * 20.0f), (int)(b[3] * 20.0f),
                    (int)(b[4] * 20.0f), micFftConteo());
            }

            uBit.sleep(16);
        }
    }
    // NO limpia al final: se repinta todo cada frame (bucle continuo)
}
