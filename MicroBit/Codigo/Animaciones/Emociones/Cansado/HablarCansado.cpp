/**
 * HablarCansado.cpp - La boca del CANSADO que HABLA (lento, con sueno)
 * ASINCRONO con FIBERS
 *
 * Lo activa la IA con "TALK" cuando la emocion activa es CANSADO.
 * La cara base es la misma que Cansado.cpp (ojos + boca chica) y la
 * boca habla LENTA y APAGADA: los 3 LEDs se encienden y apagan a
 * ritmo de "con sueno" (mas lento y mas tenue que la alegria) - el
 * "siii... ya voy..." del que se esta durmiendo.
 *
 * ARQUITECTURA DE FIBRAS (igual que las otras emociones):
 *   - El bucle principal (Principal.cpp) mueve la boca
 *   - UNA FIBRA (create_fiber) hace parpadeos PESADOS de ojos EN
 *     PARALELO: se cierran lento y quedan cerrados UN BUEN RATO,
 *     con esperas largas (3-7s) - el cansado casi no abre los ojos.
 *   fiber_sleep() cede la CPU -> las dos animaciones corren a la vez.
 *   La boca toca la fila 3; los ojos la fila 1. No se pisan.
 *
 * modoHablar es el MISMO global de la alegria (Alegria/Hablar.h):
 * cualquier comando que no sea TALK lo apaga y TODAS las fibras mueren.
 */
#include "HablarCansado.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Cansado.cpp)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Los 3 LEDs de la boca chica (los que hablan lento)
static const uint8_t BOCA[3][2] = {
    {1, 3}, {2, 3}, {3, 3}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void setPixel(const uint8_t* p, int v)
{
    uBit.display.image.setPixelValue(p[0], p[1], v);
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeos PESADOS de ojos EN PARALELO mientras modoHablar siga
// activo. Se cierran lento, quedan cerrados 350ms, abren lento, y espera
// 3-7s (el cansado casi no abre los ojos).
// ---------------------------------------------------------------------------

static void fiberOjosCansado(void)
{
    while (modoHablar) {
        // 1) Parpadeo pesado: cerrar lento
        for (int s = 0; s <= 5 && modoHablar; s++) {
            int v = 255 - 255 * s / 5;
            setPixel(OJO_IZQ, v);
            setPixel(OJO_DER, v);
            fiber_sleep(45);
        }
        fiber_sleep(350);   // cerrados un buen rato

        // 2) Abrir lento
        for (int s = 5; s >= 0 && modoHablar; s--) {
            int v = 255 - 255 * s / 5;
            setPixel(OJO_IZQ, v);
            setPixel(OJO_DER, v);
            fiber_sleep(45);
        }

        // 3) Espera LARGA e impredecible: 3s a 7s (casi no parpadea)
        int espera = 3000 + uBit.random(4000);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }

    // Sale del loop (modoHablar ya es false) -> la fibra se libera sola
    release_fiber();
}

// ---------------------------------------------------------------------------
// La boca habla LENTA y APAGADA (con sueno): pulso tenue y lento
// ---------------------------------------------------------------------------

// Un "tic" de habla cansada (mas lento y mas tenue que la alegria)
static void ticBocaCansado()
{
    // subir (tenue: max 200)
    for (int s = 0; s <= 2; s++) {
        int b = 200 * s / 2;
        setPixel(BOCA[0], b);
        setPixel(BOCA[1], b);
        setPixel(BOCA[2], b);
        fiber_sleep(30);
    }
    fiber_sleep(140);
    // bajar
    for (int s = 2; s >= 0; s--) {
        int b = 200 * s / 2;
        setPixel(BOCA[0], b);
        setPixel(BOCA[1], b);
        setPixel(BOCA[2], b);
        fiber_sleep(30);
    }
    fiber_sleep(140);
}

// ---------------------------------------------------------------------------
// La cara base para hablar: la misma cara cansada (apagada)
// ---------------------------------------------------------------------------
static void dibujarCaraCansado()
{
    uBit.display.setBrightness(75);
    uBit.display.image.clear();
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    for (int i = 0; i < 3; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con cansado activo): prepara la cara y lanza la fibra de ojos
void iniciarHablarCansado()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraCansado();
    uBit.serial.send("TALK-CAN\n");   // debug: confirmar el dispatch
    create_fiber(fiberOjosCansado);
}

// Una pasada de la boca hablando lento (la llama el bucle principal)
void animarBocaCansado()
{
    ticBocaCansado();
    ticBocaCansado();
    ticBocaCansado();   // ~3 tics por pasada -> lento, con sueno
}
