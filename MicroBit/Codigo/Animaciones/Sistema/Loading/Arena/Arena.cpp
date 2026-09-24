/**
 * Arena.cpp - Patron de carga 7: SANDBOX DE ARENA INTERACTIVO 🏜️🌀 (v3)
 *
 * v4 (FUERZA TOTAL + INERCIA + SHAKE): la arena ya no solo ve la
 * INCLINACION — ahora SENTE EL MOVIMIENTO de la placa:
 *
 *   - FUERZA TOTAL: getX()/getY() devuelven gravedad proyectada +
 *     aceleracion dinamica del movimiento. La magnitud total
 *     sqrt(x^2+y^2+z^2) sube de ~1000mg (quieta) a 2000-3000mg al
 *     sacudir: es la senal de "cuanta fuerza recibe" la arena.
 *   - INERCIA REAL: la gravedad percibida se retrasa mas cuanto mas
 *     violento es el cambio (jerk). Si giras la placa rapido, la
 *     arena mantiene su direccion un momento ("flota") y luego cae.
 *   - SHAKE = DESPARRAME: al detectar sacudida fuerte (mag > 1.7g o
 *     jerk violento), los 8 granos saltan a posiciones aleatorias
 *     por toda la pantalla (como sacudir un bote de arena), con
 *     cooldown para que sea una vez por sacudida.
 *
 * v3: fisica de gravedad por inclinacion (falling sand).
 * v2: reloj de arena programado (caida/volteo).
 */
#include "Arena.h"
#include "../LoadingBase.h"
#include <math.h>

#define N_GRANOS 8

// Estado de los granos (estatico = bucle continuo)
static float posX[N_GRANOS], posY[N_GRANOS];   // celda actual (0..4)
static float visX[N_GRANOS], visY[N_GRANOS];   // posicion visual (sub-pixel)
static float acum[N_GRANOS];                   // acumulador de movimiento
static float gxSuave = 0.0f, gySuave = 0.0f;   // gravedad percibida (inercia)
static float gxPrev = 0.0f, gyPrev = 0.0f;     // lecturas previas (jerk)
static int   coolShake = 0;                    // cooldown del desparrame
static bool  iniciado = false;

// Matriz de ocupacion (se reconstruye cada frame)
static bool ocupado[5][5];

static bool dentro(int x, int y)
{
    return (x >= 0 && x <= 4 && y >= 0 && y <= 4);
}

// Siembra inicial: una montanita de arena al centro
static void sembrar()
{
    const float s[N_GRANOS][2] = {
        {1.0f, 1.0f}, {2.0f, 1.0f}, {3.0f, 1.0f},
        {1.0f, 2.0f}, {2.0f, 2.0f}, {3.0f, 2.0f},
        {1.5f, 3.0f}, {2.5f, 3.0f}
    };
    for (int i = 0; i < N_GRANOS; i++) {
        posX[i] = s[i][0];
        posY[i] = s[i][1];
        visX[i] = s[i][0];
        visY[i] = s[i][1];
        acum[i] = 0.0f;
    }
    // La gravedad percibida arranca con la inclinacion actual (sin
    // transitorio de convergencia al iniciar)
    gxSuave = (float)uBit.accelerometer.getX() / 1000.0f;
    gySuave = (float)uBit.accelerometer.getY() / 1000.0f;
}

// DESPARRAME por sacudida: los granos saltan a posiciones aleatorias
// (como sacudir un bote de arena). Con rango 0.3..3.7 para que no
// queden pegados a la pared.
static void desparramar()
{
    for (int i = 0; i < N_GRANOS; i++) {
        posX[i] = 0.3f + (uBit.random(340) / 100.0f);
        posY[i] = 0.3f + (uBit.random(340) / 100.0f);
        acum[i] = 0.0f;
    }
}

// Mueve la arena segun la gravedad (gx, gy en -1..1)
static void moverArena(float gx, float gy)
{
    // paso por eje (celda): solo si la inclinacion supera el umbral
    int sx = (fabsf(gx) > 0.20f) ? ((gx > 0) ? 1 : -1) : 0;
    int sy = (fabsf(gy) > 0.20f) ? ((gy > 0) ? 1 : -1) : 0;
    if (sx == 0 && sy == 0) return;          // plano: arena reposa

    float mag = sqrtf(gx * gx + gy * gy);    // que tan inclinada
    if (mag > 1.0f) mag = 1.0f;
    float rate = mag;                        // velocidad proporcional

    // reconstruir ocupacion (celdas redondeadas)
    for (int a = 0; a < 5; a++)
        for (int b = 0; b < 5; b++)
            ocupado[a][b] = false;
    for (int i = 0; i < N_GRANOS; i++) {
        int cx = (int)(posX[i] + 0.5f), cy = (int)(posY[i] + 0.5f);
        if (dentro(cx, cy)) ocupado[cx][cy] = true;
    }

    // Fisica de arena real: procesar los granos de ABAJO hacia ARRIBA
    // (segun la gravedad) para que los de abajo se asienten primero y
    // los de arriba caigan sobre ellos. Orden simple: proyeccion en -g.
    int orden[N_GRANOS];
    bool hecho[N_GRANOS] = {false, false, false, false, false, false, false, false};
    for (int paso = 0; paso < N_GRANOS; paso++) {
        int mejor = -1;
        float mejorProj = -1e9f;
        for (int i = 0; i < N_GRANOS; i++) {
            if (hecho[i]) continue;
            float proj = posX[i] * gx + posY[i] * gy;   // proyeccion en g
            if (proj > mejorProj) { mejorProj = proj; mejor = i; }
        }
        orden[paso] = mejor;
        hecho[mejor] = true;
    }

    for (int p = 0; p < N_GRANOS; p++) {
        int i = orden[p];
        acum[i] += rate * 0.10f;             // ~6 celdas/s a inclinacion 1
        if (acum[i] < 1.0f) continue;
        acum[i] = 0.0f;

        int cx = (int)(posX[i] + 0.5f), cy = (int)(posY[i] + 0.5f);
        if (!dentro(cx, cy)) continue;
        ocupado[cx][cy] = false;             // se libera al intentar mover

        // 1) caida directa (diagonal natural si inclinado en 2 ejes)
        if (dentro(cx + sx, cy + sy) && !ocupado[cx + sx][cy + sy]) {
            posX[i] = (float)(cx + sx); posY[i] = (float)(cy + sy);
        }
        // 2) deslizar en un solo eje (monticulos: la arena rueda de lado)
        else if (sx != 0 && dentro(cx + sx, cy) && !ocupado[cx + sx][cy]) {
            posX[i] = (float)(cx + sx); posY[i] = (float)cy;
        }
        else if (sy != 0 && dentro(cx, cy + sy) && !ocupado[cx][cy + sy]) {
            posX[i] = (float)cx; posY[i] = (float)(cy + sy);
        }
        // 3) si todo bloqueado -> se queda (APILA). El intento 1 ya
        //    probo la diagonal, asi que los monticulos crecen natural.

        int nc = (int)(posX[i] + 0.5f), nr = (int)(posY[i] + 0.5f);
        if (dentro(nc, nr)) ocupado[nc][nr] = true;
    }
}

// Pinta UN grano como puntito nítido (sub-pixel: se desliza fluido)
static void pintarGrano(int i, int base)
{
    int px = (int)(visX[i] + 0.5f);
    int py = (int)(visY[i] + 0.5f);
    if (px < 0) px = 0;
    if (px > 4) px = 4;
    if (py < 0) py = 0;
    if (py > 4) py = 4;
    if (base > uBit.display.image.getPixelValue(px, py))
        uBit.display.image.setPixelValue(px, py, base);
}

void animarArena(int vueltas)
{
    if (!iniciado) { sembrar(); iniciado = true; }

    static int dbg = 0;
    for (int v = 0; v < vueltas; v++) {
        for (int f = 0; f < 400; f++) {              // ~6.4s por pasada
            if (frameRastro(85)) return;             // rastro + abortar serial

            // --- FUERZA TOTAL: gravedad proyectada + movimiento ---
            float gx = (float)uBit.accelerometer.getX() / 1000.0f;
            float gy = (float)uBit.accelerometer.getY() / 1000.0f;
            float gz = (float)uBit.accelerometer.getZ() / 1000.0f;
            if (gx > 1.0f) gx = 1.0f; if (gx < -1.0f) gx = -1.0f;
            if (gy > 1.0f) gy = 1.0f; if (gy < -1.0f) gy = -1.0f;

            // Jerk: que tan violento es el cambio frame a frame
            float jerk = fabsf(gx - gxPrev) + fabsf(gy - gyPrev);
            gxPrev = gx; gyPrev = gy;

            // Magnitud total (1g quieta, >1.7g al sacudir)
            float mag = sqrtf(gx*gx + gy*gy + gz*gz);

            // --- SHAKE = DESPARRAME (con cooldown) ---
            // Umbrales afinados: mag > 1.7 (impacto real) O jerk > 1.2
            // (movimiento MUY violento). Un movimiento rapido de muneca
            // no dispersa; solo la sacudida fuerte.
            if (coolShake > 0) coolShake--;
            if ((mag > 1.7f || jerk > 1.2f) && coolShake == 0) {
                desparramar();
                coolShake = 30;              // ~0.5s entre sacudidas
            }

            // --- INERCIA REAL: la gravedad percibida se retrasa ---
            // cuanto mas violento el cambio, mas tarda en seguir a la
            // placa (la arena "flota" un momento al girar rapido).
            float k = 0.30f - jerk * 0.12f;
            if (k < 0.08f) k = 0.08f;
            gxSuave += (gx - gxSuave) * k;
            gySuave += (gy - gySuave) * k;

            moverArena(gxSuave, gySuave);

            // posicion visual interpola hacia la celda (suave)
            for (int i = 0; i < N_GRANOS; i++) {
                visX[i] += (posX[i] - visX[i]) * 0.30f;
                visY[i] += (posY[i] - visY[i]) * 0.30f;
            }

            // pintar: dos tonos de arena para dar textura
            for (int i = 0; i < N_GRANOS; i++)
                pintarGrano(i, (i % 2 == 0) ? 230 : 200);

            // debug: gravedad (mg), mag total y jerk cada ~1.2s
            if (++dbg >= 75) {
                dbg = 0;
                uBit.serial.printf("G%d %d M%d J%d\n",
                    (int)(gx * 1000.0f), (int)(gy * 1000.0f),
                    (int)(mag * 1000.0f), (int)(jerk * 1000.0f));
            }

            uBit.sleep(16);
        }
    }
    // NO limpia al final: bucle continuo (estado estatico)
}
