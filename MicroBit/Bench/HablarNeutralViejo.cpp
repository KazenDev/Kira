/**
 * HablarNeutralViejo.cpp - la version ANTERIOR (git show), solo para el
 * bench. NO se compila en la firmware.
 */
/**
 * HablarNeutral.cpp - La boca del NEUTRAL que HABLA (el "bruh" habla)
 * ASINCRONO con FIBERS
 *
 * Lo activa la IA con "TALK" cuando la emocion activa es NEUTRAL.
 * La cara base es la misma que Neutral.cpp (ojos cerrados + boca
 * recta) y la boca PARPADEA: los 3 LEDs (1,3)(2,3)(3,3) se encienden
 * y apagan a ritmo de habla (como los dientes de la alegria, pero
 * con la boca neutra - el neutral habla sin expresion).
 *
 * ARQUITECTURA DE FIBRAS (igual que las otras emociones):
 *   - El bucle principal (Principal.cpp) parpadea los 3 LEDs
 *   - UNA FIBRA (create_fiber) hace "peeks" de ojos EN PARALELO:
 *     los parpados cerrados se entreabren de vez en cuando
 *   fiber_sleep() cede la CPU -> las dos animaciones corren a la vez.
 *   La boca toca la fila 3; los ojos tocan la fila 1. No se pisan.
 *
 * modoHablar es el MISMO global de la alegria (Alegria/Hablar.h):
 * cualquier comando que no sea TALK lo apaga y TODAS las fibras mueren.
 */
#include "HablarNeutral.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Neutral.cpp)
// ---------------------------------------------------------------------------
static const uint8_t PARPADO_IZQ[2] = {0, 1};
static const uint8_t PARPADO_DER[2] = {4, 1};

// Los 3 LEDs de la boca recta (los que parpadean al hablar)
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

// PEEK sincronizado: ambos parpados se mueven JUNTOS paso a paso.
// Checa modoHablar en cada paso: si llega otra emocion, la fibra MUERE.
//   cerrar=false -> ENTREABRIR: los parpados se apagan 255 -> 0
//   cerrar=true  -> CERRAR: los parpados suben 0 -> 255
static void peekOjosNeutral(bool izq, bool der, int steps, int delayMs, bool cerrar)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int p = cerrar ? 255 * s / steps : 255 * (steps - s) / steps;
        if (izq) setPixel(PARPADO_IZQ, p);
        if (der) setPixel(PARPADO_DER, p);
        fiber_sleep(delayMs);
    }
}

// ---------------------------------------------------------------------------
// FIBRA: peeks de ojos impredecibles mientras modoHablar siga activo
// ---------------------------------------------------------------------------

static void fiberPeekNeutral(void)
{
    while (modoHablar) {
        // 1) Decidir QUE ojo: ~75% ambos, ~12% izq, ~12% der
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 88)      { izq = false; }   // peek derecho
        else if (r >= 75) { der = false; }   // peek izquierdo

        // 2) El peek: se entreabren (~120ms), un momento, y cierran
        peekOjosNeutral(izq, der, 4, 30, false);   // entreabrir
        fiber_sleep(350);                          // mirando (solo pupilas)
        peekOjosNeutral(izq, der, 4, 30, true);    // cerrar

        // 3) Espera ALEATORIA al siguiente peek: 1.5s a 5s
        int espera = 1500 + uBit.random(3500);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }

    // Sale del loop (modoHablar ya es false) -> la fibra se libera sola
    release_fiber();
}

// ---------------------------------------------------------------------------
// Los 3 LEDs de la boca PARPADEAN a ritmo de habla (fades, sin saltos)
// ---------------------------------------------------------------------------

// Un "tic" de habla neutral: los 3 LEDs se encienden y apagan
static void ticBocaNeutral()
{
    // encender
    for (int s = 0; s <= 2; s++) {
        int b = 255 * s / 2;
        setPixel(BOCA[0], b);
        setPixel(BOCA[1], b);
        setPixel(BOCA[2], b);
        fiber_sleep(20);
    }
    fiber_sleep(80);
    // apagar
    for (int s = 2; s >= 0; s--) {
        int b = 255 * s / 2;
        setPixel(BOCA[0], b);
        setPixel(BOCA[1], b);
        setPixel(BOCA[2], b);
        fiber_sleep(20);
    }
    fiber_sleep(80);
}

// ---------------------------------------------------------------------------
// La cara base para hablar: ojos cerrados + boca recta
// ---------------------------------------------------------------------------
static void dibujarCaraNeutral()
{
    uBit.display.setBrightness(85);
    uBit.display.image.clear();
    setPixel(PARPADO_IZQ, 255);
    setPixel(PARPADO_DER, 255);
    uBit.display.image.setPixelValue(1, 1, 255);   // pupilas
    uBit.display.image.setPixelValue(3, 1, 255);
    for (int i = 0; i < 3; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con neutral activo): prepara la cara y lanza la fibra de peeks
void iniciarHablarNeutralVieja()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraNeutral();
    uBit.serial.send("TALK-NEU\n");   // debug: confirmar el dispatch
    create_fiber(fiberPeekNeutral);
}

// Una pasada de los 3 LEDs parpadeando (la llama el bucle principal)
void animarBocaNeutralVieja()
{
    ticBocaNeutral();
    ticBocaNeutral();
    ticBocaNeutral();   // ~3 tics por pasada -> ritmo de conversacion
}
