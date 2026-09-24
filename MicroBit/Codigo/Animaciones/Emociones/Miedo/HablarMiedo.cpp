/**
 * HablarMiedo.cpp - La boca del MIEDO que HABLA (tartamudea)
 * ASINCRONO con FIBERS
 *
 * Lo activa la IA con "TALK" cuando la emocion activa es MIEDO.
 * La cara base es la misma que Miedo.cpp (cejas + ojos de las esquinas
 * + boca GRANDE abierta) y la boca TARTAMUDEA: se abre y cierra con
 * ritmo irregular (t-t-t-...), como el que no puede sacar las
 * palabras del miedo.
 *
 * ARQUITECTURA DE FIBRAS (igual que las otras emociones):
 *   - El bucle principal (Principal.cpp) tartamudea la boca
 *   - UNA FIBRA (create_fiber) hace de ojos EN PARALELO: parpadeo
 *     rapidisimo, miradas al centro y hasta temblor de cuerpo.
 *   fiber_sleep() cede la CPU -> las dos animaciones corren a la vez.
 *   La boca toca filas 2-4; los ojos la fila 1. No se pisan.
 *
 * modoHablar es el MISMO global de la alegria (Alegria/Hablar.h):
 * cualquier comando que no sea TALK lo apaga y TODAS las fibras mueren.
 */
#include "HablarMiedo.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Miedo.cpp)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {0, 1};
static const uint8_t OJO_DER[2] = {4, 1};
static const uint8_t PUPILA_IZQ[2] = {1, 1};
static const uint8_t PUPILA_DER[2] = {3, 1};

// La boca GRANDE abierta (la que tartamudea)
static const uint8_t BOCA[8][2] = {
    {1, 2}, {2, 2}, {3, 2},
    {1, 3}, {3, 3},
    {1, 4}, {2, 4}, {3, 4}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void setPixel(const uint8_t* p, int v)
{
    uBit.display.image.setPixelValue(p[0], p[1], v);
}

// ---------------------------------------------------------------------------
// FIBRA: ojos del miedo EN PARALELO mientras modoHablar siga activo.
// 60% parpadeo rapidisimo, 25% mirada al centro, 15% temblor de cuerpo.
// ---------------------------------------------------------------------------

static void fiberOjosMiedo(void)
{
    while (modoHablar) {
        int r = uBit.random(100);
        if (r < 60) {
            // 1) Parpadeo rapidisimo (2 seguiditos)
            for (int i = 0; i < 2 && modoHablar; i++) {
                setPixel(OJO_IZQ, 0);
                setPixel(OJO_DER, 0);
                fiber_sleep(50);
                setPixel(OJO_IZQ, 255);
                setPixel(OJO_DER, 255);
                fiber_sleep(80);
            }
        }
        else if (r < 85) {
            // 2) Los ojos MIRAN al centro (la amenaza) y vuelven
            setPixel(OJO_IZQ, 0);
            setPixel(OJO_DER, 0);
            setPixel(PUPILA_IZQ, 255);
            setPixel(PUPILA_DER, 255);
            fiber_sleep(200);
            setPixel(PUPILA_IZQ, 0);
            setPixel(PUPILA_DER, 0);
            setPixel(OJO_IZQ, 255);
            setPixel(OJO_DER, 255);
        }
        else {
            // 3) Temblor de cuerpo (el brillo vibra un momento)
            for (int i = 0; i < 5 && modoHablar; i++) {
                uBit.display.setBrightness(85 + uBit.random(70));
                fiber_sleep(30);
            }
            uBit.display.setBrightness(100);
        }

        // Espera CORTA e impredecible: 0.5s a 2s (el miedo parpadea mucho)
        int espera = 500 + uBit.random(1500);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }

    // Sale del loop (modoHablar ya es false) -> la fibra se libera sola
    release_fiber();
}

// ---------------------------------------------------------------------------
// La boca TARTAMUDEA: se abre y cierra con ritmo irregular (t-t-t)
// ---------------------------------------------------------------------------

// Un "tartamudeo": la boca se cierra del todo y se reabre.
// holdMs = cuanto tiempo queda cerrada (corto = rapido, largo = trabado)
static void ticBocaMiedo(int holdMs)
{
    // cerrar (el grito se traga)
    for (int s = 0; s <= 3; s++) {
        int b = 255 * (3 - s) / 3;
        for (int j = 0; j < 8; j++) setPixel(BOCA[j], b);
        fiber_sleep(15);
    }
    fiber_sleep(holdMs);          // la pausa trabada del tartamudeo
    // reabrir (otra vez el grito)
    for (int s = 0; s <= 3; s++) {
        int b = 255 * s / 3;
        for (int j = 0; j < 8; j++) setPixel(BOCA[j], b);
        fiber_sleep(15);
    }
    fiber_sleep(70);
}

// ---------------------------------------------------------------------------
// La cara base para hablar: la misma cara del grito
// ---------------------------------------------------------------------------
static void dibujarCaraMiedo()
{
    uBit.display.setBrightness(100);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(1, 0, 255);   // cejas
    uBit.display.image.setPixelValue(3, 0, 255);
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    for (int i = 0; i < 8; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con miedo activo): prepara la cara y lanza la fibra de ojos
void iniciarHablarMiedo()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraMiedo();
    uBit.serial.send("TALK-MIE\n");   // debug: confirmar el dispatch
    create_fiber(fiberOjosMiedo);
}

// Una pasada de la boca tartamudeando (la llama el bucle principal).
// Ritmo: t-t- ... pausa ... t- ... t (el que no puede sacar las palabras)
void animarBocaMiedo()
{
    ticBocaMiedo(50);    // "t-"
    ticBocaMiedo(50);    // "t-"
    fiber_sleep(150);    // la pausa del tartamudeo
    ticBocaMiedo(50);    // "t-"
    ticBocaMiedo(120);   // "...miedo" (se traba un poco mas)
}
