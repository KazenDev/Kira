/**
 * HablarEnojado.cpp - La boca del ENOJADO que HABLA (gruñido)
 * ASINCRONO con FIBERS
 *
 * Lo activa la IA con "TALK" cuando la emocion activa es ENOJADO.
 * La cara base es la misma que Enojado.cpp (cejas fruncidas + boca
 * con dientes) y la boca GRUÑE: el hueco de la mandibula (2,4) se
 * abre y cierra RAPIDO a ritmo de habla enojada.
 *
 * ARQUITECTURA DE FIBRAS (igual que la alegria/tristeza):
 *   - El bucle principal (Principal.cpp) mueve la mandibula
 *   - UNA FIBRA (create_fiber) parpadea los ojos EN PARALELO con
 *     parpadeo BRUSCO e impredecible
 *   fiber_sleep() cede la CPU -> las dos animaciones corren a la vez.
 *   La boca toca la fila 3-4; los ojos tocan (1,1) y (3,1).
 *   No se pisan -> sin condiciones de carrera.
 *
 * modoHablar es el MISMO global de la alegria (Alegria/Hablar.h):
 * cualquier comando que no sea TALK lo apaga y TODAS las fibras mueren.
 */
#include "HablarEnojado.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Enojado.cpp)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Cejas + extension al fruncirse
static const uint8_t CEJA_IZQ[2] = {0, 0};
static const uint8_t CEJA_DER[2] = {4, 0};
static const uint8_t CEJA_IZQ_F[2] = {1, 0};
static const uint8_t CEJA_DER_F[2] = {3, 0};

// Boca con dientes (mandibula con hueco abierto en (2,4))
static const uint8_t BOCA[7][2] = {
    {1,3}, {2,3}, {3,3}, {0,4}, {1,4}, {3,4}, {4,4}
};

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void setPixel(const uint8_t* p, int v)
{
    uBit.display.image.setPixelValue(p[0], p[1], v);
}

// Parpadeo BRUSCO y sincronizado: ambos ojos se mueven JUNTOS paso a
// paso (secuenciales se veria desincronizado). Checa modoHablar en cada
// paso: si llega otra emocion, la fibra MUERE en el acto.
static void cerrarOjosEnojados(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = 255 - (255 * s) / steps;
        if (izq) setPixel(OJO_IZQ, b);
        if (der) setPixel(OJO_DER, b);
        fiber_sleep(delayMs);
    }
}

static void abrirOjosEnojados(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = (255 * s) / steps;
        if (izq) setPixel(OJO_IZQ, b);
        if (der) setPixel(OJO_DER, b);
        fiber_sleep(delayMs);
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeo BRUSCO e impredecible mientras modoHablar siga activo
// (el enojado casi no parpadea: esperas cortas 0.8-2.5s, blinks rapidos)
// ---------------------------------------------------------------------------

static void fiberParpadeoEnojado(void)
{
    while (modoHablar) {
        // 1) Decidir QUE parpadea: ~80% ambos, ~10% guino izq, ~10% guino der
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 90)      { izq = false; }   // guino derecho
        else if (r >= 80) { der = false; }   // guino izquierdo

        // 2) Parpadeo BRUSCO: cerrar ~40ms, cerrado ~70ms, abrir ~40ms
        cerrarOjosEnojados(izq, der, 2, 20);
        fiber_sleep(70);
        abrirOjosEnojados(izq, der, 2, 20);

        // 3) Espera ALEATORIA al siguiente parpadeo: 0.8s a 2.5s
        int espera = 800 + uBit.random(1700);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }

    // Sale del loop (modoHablar ya es false) -> la fibra se libera sola
    release_fiber();
}

// ---------------------------------------------------------------------------
// La boca GRUÑE: la mandibula se abre y cierra RAPIDO (fades cortos)
// ---------------------------------------------------------------------------

// Un "tic" de gruñido: el hueco (2,4) se abre y cierra a ritmo enojado
static void ticBocaEnojada()
{
    // abrir: la mandibula baja (2,4 se enciende)
    for (int s = 0; s <= 1; s++) {
        uBit.display.image.setPixelValue(2, 4, 255 * s);
        fiber_sleep(15);
    }
    fiber_sleep(60);
    // cerrar: la mandibula sube (2,4 se apaga, dientes apretados)
    for (int s = 0; s <= 1; s++) {
        uBit.display.image.setPixelValue(2, 4, 255 - 255 * s);
        fiber_sleep(15);
    }
    fiber_sleep(50);
}

// ---------------------------------------------------------------------------
// La cara base para hablar: cejas fruncidas + ojos + boca con dientes
// ---------------------------------------------------------------------------
static void dibujarCaraEnojada()
{
    uBit.display.setBrightness(95);
    uBit.display.image.clear();
    setPixel(CEJA_IZQ, 255);
    setPixel(CEJA_DER, 255);
    setPixel(CEJA_IZQ_F, 255);
    setPixel(CEJA_DER_F, 255);   // cejas bien fruncidas al hablar
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    // Boca con dientes SIEMPRE encendida (nunca se apaga mientras habla)
    for (int i = 0; i < 7; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con enojo activo): prepara la cara y lanza la fibra de parpadeo
void iniciarHablarEnojado()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraEnojada();
    uBit.serial.send("TALK-ANG\n");   // debug: confirmar el dispatch
    create_fiber(fiberParpadeoEnojado);
}

// Una pasada de la boca gruñendo (la llama el bucle principal)
void animarBocaEnojada()
{
    ticBocaEnojada();
    ticBocaEnojada();
    ticBocaEnojada();   // ~3 tics por pasada -> ritmo de gruñido
    ticBocaEnojada();   // el enojado habla MAS rapido (4 tics)
}
