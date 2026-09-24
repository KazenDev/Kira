/**
 * HablarFastidio.cpp - La boca del FASTIDIO que HABLA (habla apretado)
 * ASINCRONO con FIBERS
 *
 * Lo activa la IA con "TALK" cuando la emocion activa es FASTIDIO.
 * La cara base es la misma que Fastidio.cpp (cejas + ojos de reojo +
 * boca recta) y la boca HABLA APRETADA: los 3 LEDs pulsan TENUES
 * (nunca se apagan del todo) - el tono "pfff" del que responde sin
 * ganas, entre dientes.
 *
 * ARQUITECTURA DE FIBRAS (igual que las otras emociones):
 *   - El bucle principal (Principal.cpp) pulsa la boca
 *   - UNA FIBRA (create_fiber) hace parpadeos DUROS de ojos EN
 *     PARALELO: cada tanto los ojos se cierran de golpe un instante
 *     (aguantando el mal humor) y a veces la ceja tiembla junto.
 *   fiber_sleep() cede la CPU -> las dos animaciones corren a la vez.
 *   La boca toca la fila 3; los ojos la fila 1. No se pisan.
 *
 * modoHablar es el MISMO global de la alegria (Alegria/Hablar.h):
 * cualquier comando que no sea TALK lo apaga y TODAS las fibras mueren.
 */
#include "HablarFastidio.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Fastidio.cpp)
// ---------------------------------------------------------------------------
static const uint8_t CEJA_IZQ_IN[2] = {1, 0};   // borde interno de la ceja
static const uint8_t CEJA_DER_IN[2] = {3, 0};

static const uint8_t OJO_IZQ[2] = {0, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Los 3 LEDs de la boca recta (los que pulsan al hablar)
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
// FIBRA: parpadeos DUROS de ojos impredecibles mientras modoHablar siga
// activo. 80% ambos ojos, 10% guino izq, 10% guino der. A veces la ceja
// tiembla junto con el parpadeo.
// ---------------------------------------------------------------------------

static void fiberOjosFastidio(void)
{
    while (modoHablar) {
        // 1) Decidir QUE ojo se cierra
        int r = uBit.random(100);
        bool cierroIzq = true, cierroDer = true;
        if (r >= 90)      { cierroIzq = false; }   // guino derecho
        else if (r >= 80) { cierroDer = false; }   // guino izquierdo

        // 2) Parpadeo DURO: cierran de golpe, un instante, abren
        if (cierroIzq) setPixel(OJO_IZQ, 0);
        if (cierroDer) setPixel(OJO_DER, 0);
        fiber_sleep(140);
        setPixel(OJO_IZQ, 255);
        setPixel(OJO_DER, 255);

        // 3) A veces (1/3) la ceja tiembla junto al parpadeo
        if (uBit.random(3) == 0) {
            setPixel(CEJA_IZQ_IN, 80);
            setPixel(CEJA_DER_IN, 80);
            fiber_sleep(60);
            setPixel(CEJA_IZQ_IN, 255);
            setPixel(CEJA_DER_IN, 255);
        }

        // 4) Espera ALEATORIA al siguiente parpadeo: 1.5s a 4s
        int espera = 1500 + uBit.random(2500);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }

    // Sale del loop (modoHablar ya es false) -> la fibra se libera sola
    release_fiber();
}

// ---------------------------------------------------------------------------
// La boca HABLA APRETADA: los 3 LEDs pulsan tenues (60->240, nunca se
// apagan del todo) = hablar entre dientes, sin ganas
// ---------------------------------------------------------------------------

// Un "tic" de habla fastidiada
static void ticBocaFastidio()
{
    // subir (tenue, sin ganas)
    for (int s = 0; s <= 2; s++) {
        int b = 60 + 180 * s / 2;   // 60 -> 240 (nunca 0 ni 255)
        setPixel(BOCA[0], b);
        setPixel(BOCA[1], b);
        setPixel(BOCA[2], b);
        fiber_sleep(18);
    }
    fiber_sleep(70);
    // bajar
    for (int s = 2; s >= 0; s--) {
        int b = 60 + 180 * s / 2;
        setPixel(BOCA[0], b);
        setPixel(BOCA[1], b);
        setPixel(BOCA[2], b);
        fiber_sleep(18);
    }
    fiber_sleep(70);
}

// ---------------------------------------------------------------------------
// La cara base para hablar: la misma cara fastidiada
// ---------------------------------------------------------------------------
static void dibujarCaraFastidio()
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();
    setPixel(CEJA_IZQ_IN, 255);
    setPixel(CEJA_DER_IN, 255);
    uBit.display.image.setPixelValue(0, 0, 255);
    uBit.display.image.setPixelValue(4, 0, 255);
    uBit.display.image.setPixelValue(0, 1, 255);   // ojo izq
    uBit.display.image.setPixelValue(3, 1, 255);   // ojo der
    for (int i = 0; i < 3; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con fastidio activo): prepara la cara y lanza la fibra de ojos
void iniciarHablarFastidio()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraFastidio();
    uBit.serial.send("TALK-FAS\n");   // debug: confirmar el dispatch
    create_fiber(fiberOjosFastidio);
}

// Una pasada de la boca apretada pulsando (la llama el bucle principal)
void animarBocaFastidio()
{
    ticBocaFastidio();
    ticBocaFastidio();
    ticBocaFastidio();
    ticBocaFastidio();   // ~4 tics por pasada -> ritmo de conversacion
}
