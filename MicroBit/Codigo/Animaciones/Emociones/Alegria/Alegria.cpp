/**
 * Alegria.cpp - La emocion ALEGRIA (en reposo)
 *
 * BULE INFINITO SIN REINICIOS: la cara NUNCA se borra de la pantalla.
 * Sonrisa COMPLETA (esquinas + curva inferior) y micro-movimientos:
 *
 *   1. Respira: brillo global sube/baja suave
 *   2. Parpadeo suave x2 (ojos se cierran con fade)
 *   3. Respira otra vez
 *   4. Guino jugueton
 *
 * La boca que HABLA (lip-sync) vive en Hablar.cpp, activada por TALK.
 * TODO fluido: transiciones por pasos de brillo sobre el framebuffer
 * de 60 FPS por hardware.
 */
#include "Alegria.h"
// Ruta relativa: Alegria.cpp esta en Animaciones/Emociones/Alegria/
// Sistema.h esta en Animaciones/Sistema/ -> ../../Sistema/Sistema.h
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Sonrisa COMPLETA: esquinas (0,3),(4,3) + curva inferior (1,4),(2,4),(3,4)
static const uint8_t BOCA[5][2] = {
    {0,3}, {4,3}, {1,4}, {2,4}, {3,4}
};

// ---------------------------------------------------------------------------
// Helpers de transicion suave (fade por pasos)
// ---------------------------------------------------------------------------

// Enciende/apaga un pixel en pasos de brillo (suave, sin saltos)
static void fadePixel(const uint8_t* p, int from, int to, int steps, int delayMs)
{
    int diff = to - from;
    for (int s = 0; s <= steps; s++) {
        int b = from + (diff * s) / steps;
        uBit.display.image.setPixelValue(p[0], p[1], b);
        uBit.sleep(delayMs);
    }
}

// Fija un pixel directamente
static void setPixel(const uint8_t* p, int v)
{
    uBit.display.image.setPixelValue(p[0], p[1], v);
}

// ---------------------------------------------------------------------------
// La cara base: se dibuja UNA vez y se queda (nunca se borra en el bucle)
// ---------------------------------------------------------------------------
void mostrarCaraAlegria()   // publica: el primer frame (para las transiciones)
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();

    // Ojos
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);

    // Sonrisa COMPLETA (los 5 pixeles de la boca encendidos)
    for (int i = 0; i < 5; i++)
        setPixel(BOCA[i], 255);
}

// alias interno (la animacion lo usa); la publica es mostrarCaraAlegria()
static void dibujarCaraBase()
{
    mostrarCaraAlegria();
}

// ---------------------------------------------------------------------------
// FASE 1+3: respira feliz (brillo global sube y baja suave)
// ---------------------------------------------------------------------------
static void respiraFeliz()
{
    for (int rep = 0; rep < 2; rep++) {
        for (int b = 60; b <= 120; b += 5) {
            uBit.display.setBrightness(b);
            uBit.sleep(22);
        }
        for (int b = 120; b >= 60; b -= 5) {
            uBit.display.setBrightness(b);
            uBit.sleep(22);
        }
    }
    uBit.display.setBrightness(90);
    uBit.sleep(150);
}

// ---------------------------------------------------------------------------
// FASE 2: parpadeo suave (los dos ojos se cierran con fade)
// ---------------------------------------------------------------------------
static void parpadeoSueve()
{
    fadePixel(OJO_IZQ, 255, 0, 5, 40);
    fadePixel(OJO_DER, 255, 0, 5, 40);
    uBit.sleep(120);
    fadePixel(OJO_IZQ, 0, 255, 5, 40);
    fadePixel(OJO_DER, 0, 255, 5, 40);
}

// ---------------------------------------------------------------------------
// FASE 4: guino jugueton (solo el ojo izquierdo)
// ---------------------------------------------------------------------------
static void guino()
{
    fadePixel(OJO_IZQ, 255, 0, 4, 45);
    uBit.sleep(300);
    fadePixel(OJO_IZQ, 0, 255, 4, 45);
    uBit.sleep(200);
}

// ---------------------------------------------------------------------------
// La animacion de alegria: UNA pasada del bucle.
//
// NOTA: NO usa while(true) a proposito. El bucle infinito vive en
// Principal.cpp, que entre pasada y pasada revisa si llego un comando
// serial de la IA (asi puede cambiar de emocion o activar TALK).
// ---------------------------------------------------------------------------
void animarAlegria()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada.
    if (revisarSerial()) return;

    // La cara base se dibuja UNA vez (idempotente)
    dibujarCaraBase();
    if (revisarSerial()) return;

    respiraFeliz();     // 1: respira
    if (revisarSerial()) return;
    parpadeoSueve();    // 2: parpadeo suave
    uBit.sleep(250);
    if (revisarSerial()) return;
    parpadeoSueve();    // parpadea dos veces
    uBit.sleep(200);
    if (revisarSerial()) return;
    respiraFeliz();     // 3: respira otra vez
    if (revisarSerial()) return;
    guino();            // 4: guino
}
