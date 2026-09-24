/**
 * Carrera.cpp - Patron de carga 5: LA PERSECUCION 🏃💨 (v3)
 *
 * v3 (PUNTITOS NITIDOS): la v2 pintaba "manchas de luz suave" que al
 * moverse entre pixeles titilaban y confundian. Ahora cada personaje
 * es UN PIXEL BIEN DEFINIDO (como el usuario pidio: "un puntito y
 * otro puntito"). El rastro (decay) da la fluidez del movimiento.
 *
 *   🏃🏃  2 corredores huyendo del cazador. No se acorralan en las
 *         esquinas: empuje fuerte desde los bordes + deriva propia
 *         de cada uno + un poco de wander. Energia variable
 *         (0.65..1.2): se cansan y revive el otro.
 *   👁️  1 cazador mas rapido (0.05/frame) que DECIDE por cual va
 *         cada ~1.5-3s (mas cercano / mas lejano / cambia de presa).
 *         Sin parpadeo: la decision se nota por el cambio de rumbo.
 *         Cuando atrapa a uno: flash corto (3 frames) y nueva ronda.
 *
 * Bucle continuo (estado estatico): la persecucion nunca se corta.
 */
#include "Carrera.h"
#include "../LoadingBase.h"
#include <math.h>

// --- Estado de la persecucion (estatico = bucle continuo) ---
static float rx[2], ry[2];      // corredores (x, y en 0..4 con sub-pixel)
static float cx, cy;            // cazador
static float energia[2];        // 0.65..1.2 (se cansa / se recupera)
static int   objetivo = 0;      // a quien persigue el cazador
static int   timerDecide = 90;  // frames hasta re-decidir presa
static int   timerEnergia = 0;  // frames hasta renovar energias
static int   flash = 0;         // frames de flash de captura
static int   respiro = 0;       // pausa tras la captura (sin blanco)
static bool  iniciado = false;

// Pinta UN PUNTO NITIDO: el pixel mas cercano a la posicion, a pleno
// brillo (sin radio ni desvanecido -> nada de titileo). El rastro del
// frame anterior (decay) deja la estela que da la sensacion de fluido.
static void pintarPunto(float x, float y, int base)
{
    int px = (int)(x + 0.5f);
    int py = (int)(y + 0.5f);
    if (px < 0) px = 0;
    if (px > 4) px = 4;
    if (py < 0) py = 0;
    if (py > 4) py = 4;
    if (base > uBit.display.image.getPixelValue(px, py))
        uBit.display.image.setPixelValue(px, py, base);
}

// Nueva ronda: corredores cerca del centro pero en lados opuestos,
// cazador al centro. Asi no arrancan acorralados en las esquinas.
static void nuevaRonda()
{
    rx[0] = 1.0f + (uBit.random(10) / 10.0f) * 0.6f;   // 1.0..1.6
    ry[0] = 1.0f + (uBit.random(10) / 10.0f) * 0.6f;
    rx[1] = 3.0f - (uBit.random(10) / 10.0f) * 0.6f;   // 2.4..3.0
    ry[1] = 3.0f - (uBit.random(10) / 10.0f) * 0.6f;
    cx = 2.0f;
    cy = 2.0f;
    energia[0] = 0.9f;
    energia[1] = 0.9f;
    objetivo = uBit.random(2);
    timerDecide = 80 + uBit.random(100);
    timerEnergia = 0;
}

// Mueve un corredor: huye del cazador + deriva propia + wander +
// EMPUJE FUERTE desde los bordes (no se acorrala en las esquinas)
static void moverCorredor(int i)
{
    float dx = rx[i] - cx, dy = ry[i] - cy;
    float d = sqrtf(dx * dx + dy * dy);
    float ux = 0, uy = 0;
    if (d > 0.01f) { ux = dx / d; uy = dy / d; }

    float perpx = -uy, perpy = ux;             // perpendicular
    float signo = (i == 0) ? 1.0f : -1.0f;     // cada uno a su lado

    float vel = 0.045f * energia[i];           // 0.029..0.054
    if (d < 1.4f) vel += 0.015f;               // adrenalina: cazador cerca

    // wander: un poquito de azar para rutas vivas
    float wx = ((float)(uBit.random(21) - 10)) * 0.004f;
    float wy = ((float)(uBit.random(21) - 10)) * 0.004f;

    // ORBITA: la deriva perpendicular fuerte hace que circulen
    // alrededor del cazador en vez de huir en linea recta a las
    // esquinas (asi ya no se pegan a los bordes).
    float orbx = perpx * signo * 0.050f;
    float orby = perpy * signo * 0.050f;

    // si el cazador esta lejos, una leve atraccion al centro los
    // trae de vuelta al campo (no se quedan merodeando afuera)
    float cenx = 0, ceny = 0;
    if (d > 2.0f) {
        cenx = (2.0f - rx[i]) * 0.02f;
        ceny = (2.0f - ry[i]) * 0.02f;
    }

    // empuje desde los bordes (fuerte: no quedarse pegado)
    float bx = 0, by = 0;
    if (rx[i] < 0.8f) bx = 1.0f; else if (rx[i] > 3.2f) bx = -1.0f;
    if (ry[i] < 0.8f) by = 1.0f; else if (ry[i] > 3.2f) by = -1.0f;

    rx[i] += ux * vel + orbx + cenx + bx * 0.045f + wx;
    ry[i] += uy * vel + orby + ceny + by * 0.045f + wy;

    if (rx[i] < 0.15f) rx[i] = 0.15f;
    if (rx[i] > 3.85f) rx[i] = 3.85f;
    if (ry[i] < 0.15f) ry[i] = 0.15f;
    if (ry[i] > 3.85f) ry[i] = 3.85f;
}

// Mueve el cazador hacia su presa + temblor minimo
static void moverCazador()
{
    float dx = rx[objetivo] - cx, dy = ry[objetivo] - cy;
    float d = sqrtf(dx * dx + dy * dy);
    if (d > 0.01f) {
        cx += dx / d * 0.050f;
        cy += dy / d * 0.050f;
    }
    cx += ((float)(uBit.random(21) - 10)) * 0.002f;
    cy += ((float)(uBit.random(21) - 10)) * 0.002f;
    if (cx < 0.15f) cx = 0.15f;
    if (cx > 3.85f) cx = 3.85f;
    if (cy < 0.15f) cy = 0.15f;
    if (cy > 3.85f) cy = 3.85f;
}

// El cazador DECIDE por quien va: mas cercano, mas lejano o cambia.
// Sin parpadeo: el cambio de rumbo se nota solo.
static void decidirObjetivo()
{
    timerDecide = 90 + uBit.random(90);        // ~1.5-3s
    int r = uBit.random(3);
    if (r == 0) {                              // al mas cercano
        float d0 = (rx[0]-cx)*(rx[0]-cx) + (ry[0]-cy)*(ry[0]-cy);
        float d1 = (rx[1]-cx)*(rx[1]-cx) + (ry[1]-cy)*(ry[1]-cy);
        objetivo = (d1 < d0) ? 1 : 0;
    } else if (r == 1) {                       // al mas lejano
        float d0 = (rx[0]-cx)*(rx[0]-cx) + (ry[0]-cy)*(ry[0]-cy);
        float d1 = (rx[1]-cx)*(rx[1]-cx) + (ry[1]-cy)*(ry[1]-cy);
        objetivo = (d1 > d0) ? 1 : 0;
    } else {                                   // cambia de presa
        objetivo = 1 - objetivo;
    }
}

// Renueva la energia de los corredores (alguno se cansa, otro revive)
static void renovarEnergia()
{
    timerEnergia = 50 + uBit.random(70);
    for (int i = 0; i < 2; i++) {
        float nuevo = 0.75f + (uBit.random(100) / 100.0f) * 0.45f;  // 0.75..1.2
        if (uBit.random(4) == 0)               // a veces se recupera
            nuevo = 1.05f + (uBit.random(15) / 100.0f);
        energia[i] = nuevo;
    }
}

void animarCarrera(int vueltas)
{
    if (!iniciado) { nuevaRonda(); iniciado = true; }

    for (int v = 0; v < vueltas; v++) {
        for (int f = 0; f < 400; f++) {              // ~6.4s por pasada
            if (frameRastro(80)) return;             // rastro + abortar serial

    // Flash de captura: corto (3 frames), luego RESPIRA un momento
    // (el rastro se apaga solo, sin pantallazos seguidos)
    if (flash > 0) {
        flash--;
        setAnillo(2, 255);
        setAnillo(1, 255);
        setAnillo(0, 255);
        if (flash == 0) { respiro = 24; nuevaRonda(); }
        uBit.sleep(16);
        continue;
    }
    if (respiro > 0) {           // pausa breve tras la captura
        respiro--;
        uBit.sleep(16);
        continue;
    }

            if (--timerDecide <= 0) decidirObjetivo();
            if (--timerEnergia <= 0) renovarEnergia();

            moverCorredor(0);
            moverCorredor(1);
            moverCazador();

            // Captura? El cazador alcanzo a su presa
            float dcx = rx[objetivo] - cx, dcy = ry[objetivo] - cy;
            if (dcx * dcx + dcy * dcy < 0.16f) {     // dist < 0.4 px
                flash = 3;
                continue;
            }

            // Pintar: corredores en dos tonos, cazador el mas brillante
            pintarPunto(rx[0], ry[0], 150);
            pintarPunto(rx[1], ry[1], 120);
            pintarPunto(cx, cy, 255);

            uBit.sleep(16);
        }
    }
    // NO limpia al final: bucle continuo (estado estatico, sin saltos)
}
