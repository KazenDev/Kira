/**
 * HablarSorprendido.cpp - La boca del SORPRENDIDO que HABLA ("OH OH")
 * ASINCRONO con FIBERS
 *
 * Lo activa la IA con "TALK" cuando la emocion activa es SORPRENDIDO.
 * La cara base es la misma que Sorprendido.cpp (ojos bien abiertos +
 * boca "o") y la boca GASPEA: el diamante "o" se abre y cierra RAPIDO
 * a ritmo de habla de asombro (como exclamando cada palabra).
 *
 * ARQUITECTURA DE FIBRAS (igual que las otras emociones):
 *   - El bucle principal (Principal.cpp) mueve la boca "OH"
 *   - UNA FIBRA (create_fiber) parpadea los ojos EN PARALELO pero
 *     RARISIMO: 4-8s entre parpadeos (el asombrado mira fijo)
 *   fiber_sleep() cede la CPU -> las dos animaciones corren a la vez.
 *   La boca toca la fila 2-4; los ojos tocan (1,1) y (3,1).
 *   No se pisan -> sin condiciones de carrera.
 *
 * modoHablar es el MISMO global de la alegria (Alegria/Hablar.h):
 * cualquier comando que no sea TALK lo apaga y TODAS las fibras mueren.
 */
#include "HablarSorprendido.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Sorprendido.cpp)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// La boca "o": centro (2,3) + diamante (2,2),(1,3),(3,3),(2,4)
static const uint8_t O_DIAMANTE[4][2] = {
    {2, 2}, {1, 3}, {3, 3}, {2, 4}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void setPixel(const uint8_t* p, int v)
{
    uBit.display.image.setPixelValue(p[0], p[1], v);
}

// Parpadeo RARO y sincronizado: ambos ojos juntos paso a paso.
// Checa modoHablar en cada paso: si llega otra emocion, la fibra MUERE.
static void cerrarOjosSorprendidos(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = 255 - (255 * s) / steps;
        if (izq) setPixel(OJO_IZQ, b);
        if (der) setPixel(OJO_DER, b);
        fiber_sleep(delayMs);
    }
}

static void abrirOjosSorprendidos(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = (255 * s) / steps;
        if (izq) setPixel(OJO_IZQ, b);
        if (der) setPixel(OJO_DER, b);
        fiber_sleep(delayMs);
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeo RARISIMO mientras modoHablar siga activo
// (esperas 4-8s: el asombrado mira fijo, casi no parpadea)
// ---------------------------------------------------------------------------

static void fiberParpadeoSorprendido(void)
{
    while (modoHablar) {
        // 1) Decidir QUE parpadea: ~75% ambos, ~12% guino izq, ~12% guino der
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 88)      { izq = false; }   // guino derecho
        else if (r >= 75) { der = false; }   // guino izquierdo

        // 2) Parpadeo RAPIDO: cerrar ~40ms, cerrado ~80ms, abrir ~40ms
        cerrarOjosSorprendidos(izq, der, 2, 20);
        fiber_sleep(80);
        abrirOjosSorprendidos(izq, der, 2, 20);

        // 3) Espera LARGUISIMA al siguiente parpadeo: 4s a 8s
        int espera = 4000 + uBit.random(4000);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }

    // Sale del loop (modoHablar ya es false) -> la fibra se libera sola
    release_fiber();
}

// ---------------------------------------------------------------------------
// La boca "OH": el diamante se abre y cierra RAPIDO (un gaspo por tic)
// ---------------------------------------------------------------------------

static void ticBocaSorprendida()
{
    // abrir: el diamante crece (top -> lados -> fondo)
    for (int s = 1; s <= 3; s++) {
        setPixel(O_DIAMANTE[0], s >= 1 ? 255 : 0);
        setPixel(O_DIAMANTE[1], s >= 2 ? 255 : 0);
        setPixel(O_DIAMANTE[2], s >= 2 ? 255 : 0);
        setPixel(O_DIAMANTE[3], s >= 3 ? 255 : 0);
        fiber_sleep(15);
    }
    fiber_sleep(70);
    // cerrar: el diamante se achica hasta quedar SOLO el centro (2,3)
    // (el loop llega a s=0 para apagar tambien el píxel top (2,2))
    for (int s = 3; s >= 0; s--) {
        setPixel(O_DIAMANTE[0], s >= 1 ? 255 : 0);
        setPixel(O_DIAMANTE[1], s >= 2 ? 255 : 0);
        setPixel(O_DIAMANTE[2], s >= 2 ? 255 : 0);
        setPixel(O_DIAMANTE[3], s >= 3 ? 255 : 0);
        fiber_sleep(15);
    }
    fiber_sleep(60);
}

// ---------------------------------------------------------------------------
// La cara base para hablar: ojos bien abiertos + boca "o"
// ---------------------------------------------------------------------------
static void dibujarCaraSorprendida()
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    uBit.display.image.setPixelValue(2, 3, 255);   // centro de la "o"
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con sorpresa activa): prepara la cara y lanza la fibra de parpadeo
void iniciarHablarSorprendido()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraSorprendida();
    uBit.serial.send("TALK-SUR\n");   // debug: confirmar el dispatch
    create_fiber(fiberParpadeoSorprendido);
}

// Una pasada de la boca "OH OH" (la llama el bucle principal)
void animarBocaSorprendida()
{
    ticBocaSorprendida();
    ticBocaSorprendida();
    ticBocaSorprendida();   // ~3 tics por pasada -> "OH OH OH"
}
