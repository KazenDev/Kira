/**
 * HablarTriste.cpp - La boca de la TRISTEZA que HABLA (lip-sync triste)
 * ASINCRONO con FIBERS
 *
 * Lo activa la IA con "TALK" cuando la emocion activa es TRISTE.
 * La cara base es la misma que Triste.cpp (frown) y la boca se ABRE
 * hacia abajo a ritmo de habla: el medio del frown (2,3) se apaga y
 * se enciende (2,4) abajo, como quien habla a punto de llorar.
 * Las esquinas (0,4)(4,4) y los lados (1,3)(3,3) quedan firmes.
 *
 * ARQUITECTURA DE FIBRAS (igual que la alegria):
 *   - El bucle principal (Principal.cpp) mueve la boca "wah"
 *   - UNA FIBRA (create_fiber) parpadea los ojos EN PARALELO con
 *     parpadeo PESADO e impredecible (mas lento que la alegria)
 *   fiber_sleep() cede la CPU -> las dos animaciones corren a la vez.
 *   La boca toca la fila 3-4; los ojos tocan (1,1) y (3,1).
 *   No se pisan -> sin condiciones de carrera.
 *
 * modoHablar es el MISMO global de la alegria (Alegria/Hablar.h):
 * cualquier comando que no sea TALK lo apaga y AMBAS fibras mueren.
 */
#include "HablarTriste.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Triste.cpp)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Boca triste cerrada (frown): esquinas (0,4)(4,4) + medio (1,3)(2,3)(3,3)
static const uint8_t BOCA[5][2] = {
    {0,4}, {4,4}, {1,3}, {2,3}, {3,3}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void setPixel(const uint8_t* p, int v)
{
    uBit.display.image.setPixelValue(p[0], p[1], v);
}

// Parpadeo PESADO y sincronizado: ambos ojos se mueven JUNTOS paso a
// paso (secuenciales se veria desincronizado). Checa modoHablar en cada
// paso: si llega otra emocion, la fibra MUERE en el acto.
static void cerrarOjosTristes(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = 255 - (255 * s) / steps;
        if (izq) setPixel(OJO_IZQ, b);
        if (der) setPixel(OJO_DER, b);
        fiber_sleep(delayMs);
    }
}

static void abrirOjosTristes(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = (255 * s) / steps;
        if (izq) setPixel(OJO_IZQ, b);
        if (der) setPixel(OJO_DER, b);
        fiber_sleep(delayMs);
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeo PESADO e impredecible mientras modoHablar siga activo
// (mas lento y pesado que el de la alegria: esperas 2-6s)
// ---------------------------------------------------------------------------

static void fiberParpadeoTriste(void)
{
    while (modoHablar) {
        // 1) Decidir QUE parpadea: ~75% ambos, ~12.5% guino izq, ~12.5% guino der
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 88)      { izq = false; }   // guino derecho
        else if (r >= 75) { der = false; }   // guino izquierdo

        // 2) Parpadeo PESADO: cerrar ~200ms, cerrado ~200ms, abrir ~200ms
        cerrarOjosTristes(izq, der, 5, 40);
        fiber_sleep(200);
        abrirOjosTristes(izq, der, 5, 40);

        // 3) Espera ALEATORIA al siguiente parpadeo: 2s a 6s (triste = menos)
        int espera = 2000 + uBit.random(4000);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }

    // Sale del loop (modoHablar ya es false) -> la fibra se libera sola
    release_fiber();
}

// ---------------------------------------------------------------------------
// La boca "wah": se abre y cierra a ritmo de habla (fades, sin saltos)
// ---------------------------------------------------------------------------

// Un "tic" de habla triste: el medio del frown se abre hacia abajo
// ((2,3) se apaga, (2,4) se enciende) y vuelve a cerrarse.
static void ticBocaTriste()
{
    // abrir: (2,3) se apaga mientras (2,4) se enciende
    for (int s = 0; s <= 2; s++) {
        uBit.display.image.setPixelValue(2, 3, 255 - 255 * s / 2);
        uBit.display.image.setPixelValue(2, 4, 255 * s / 2);
        fiber_sleep(20);
    }
    fiber_sleep(90);          // abierta un momento
    // cerrar: vuelve al frown
    for (int s = 0; s <= 2; s++) {
        uBit.display.image.setPixelValue(2, 3, 255 * s / 2);
        uBit.display.image.setPixelValue(2, 4, 255 - 255 * s / 2);
        fiber_sleep(20);
    }
    fiber_sleep(90);          // pausa entre tics
}

// ---------------------------------------------------------------------------
// La cara base para hablar: frown completo + ojos
// ---------------------------------------------------------------------------
static void dibujarCaraTriste()
{
    uBit.display.setBrightness(80);
    uBit.display.image.clear();
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    // Frown SIEMPRE encendido (nunca se apaga mientras habla)
    for (int i = 0; i < 5; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con tristeza activa): prepara la cara y lanza la fibra de parpadeo
void iniciarHablarTriste()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraTriste();
    uBit.serial.send("TALK-SAD\n");   // debug: confirmar el dispatch
    create_fiber(fiberParpadeoTriste);
}

// Una pasada de la boca "wah" hablando (la llama el bucle principal)
void animarBocaTriste()
{
    ticBocaTriste();
    ticBocaTriste();
    ticBocaTriste();   // ~3 tics por pasada -> ritmo de conversacion
}
