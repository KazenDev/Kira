/**
 * TristeViejo.cpp - la version ANTERIOR (git show del commit previo al patron
 * de frame), solo para el bench. Funcion renombrada para linkearse junto a la
 * nueva. NO se compila en la firmware.
 */
/**
 * Triste.cpp - La emocion TRISTE 😢 (en reposo)
 *
 * BULE INFINITO SIN REINICIOS: la cara NUNCA se borra de la pantalla.
 * Boca INVERTIDA (frown) y micro-movimientos LENTOS y PESADOS:
 *
 *   1. Respira lento: el brillo sube/baja despacio y mas tenue que
 *      la alegria (la tristeza es apagada)
 *   2. Parpadeo PESADO: los ojos se cierran lento y quedan cerrados
 *      un momento (como ojos cansados), dos veces por pasada
 *   3. La boca tiembla un poquito: el medio del labio tiembla al azar
 *      (el labio a punto de llorar)
 *   4. De vez en cuando (cada 4-7 pasadas, impredecible) cae una
 *      LAGRIMA: se junta bajo un ojo (al azar izq/der), brilla como
 *      agua y baja por la mejilla con estela hasta el menton
 *
 * Misma arquitectura que la alegria: UNA pasada por llamada, el bucle
 * infinito vive en Principal.cpp que revisa el serial entre pasadas.
 */
#include "Triste.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y) - mismas que la alegria
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Boca INVERTIDA (frown): medio (1,3)(2,3)(3,3) + esquinas (0,4)(4,4)
// (las esquinas MAS BAJAS que el medio = la curva al reves que la alegria)
static const uint8_t BOCA[5][2] = {
    {0,4}, {4,4}, {1,3}, {2,3}, {3,3}
};

// ---------------------------------------------------------------------------
// Helpers de transicion suave (fade por pasos) - mismos que la alegria
// ---------------------------------------------------------------------------

// Fija un pixel directamente
static void setPixel(const uint8_t* p, int v)
{
    uBit.display.image.setPixelValue(p[0], p[1], v);
}

// ---------------------------------------------------------------------------
// La cara base: se dibuja UNA vez y se queda (nunca se borra en el bucle)
// ---------------------------------------------------------------------------
void mostrarCaraTristeVieja()   // publica: el primer frame (para las transiciones)
{
    uBit.display.setBrightness(80);   // mas tenue que la alegria (90)
    uBit.display.image.clear();

    // Ojos
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);

    // Boca invertida (frown)
    for (int i = 0; i < 5; i++)
        setPixel(BOCA[i], 255);
}

static void dibujarCaraBase()
{
    mostrarCaraTristeVieja();
}

// ---------------------------------------------------------------------------
// FASE 1+3: respira LENTA (brillo sube/baja despacio y tenue)
// ---------------------------------------------------------------------------
static void respiraLenta()
{
    for (int b = 45; b <= 100; b += 4) {
        uBit.display.setBrightness(b);
        uBit.sleep(45);
    }
    for (int b = 100; b >= 45; b -= 4) {
        uBit.display.setBrightness(b);
        uBit.sleep(45);
    }
    uBit.display.setBrightness(80);
    uBit.sleep(200);
}

// ---------------------------------------------------------------------------
// FASE 2+4: parpadeo PESADO (se cierran lento y quedan cerrados un momento)
// ---------------------------------------------------------------------------
static void parpadeoPesado()
{
    // AMBOS ojos se mueven JUNTOS paso a paso (si se hicieran
    // secuenciales se veria como parpadeo desincronizado)
    for (int s = 0; s <= 6; s++) {
        int b = 255 - (255 * s) / 6;   // se cierran lento
        setPixel(OJO_IZQ, b);
        setPixel(OJO_DER, b);
        uBit.sleep(50);
    }
    uBit.sleep(280);                     // cerrados un rato (cansados)
    for (int s = 0; s <= 6; s++) {
        int b = (255 * s) / 6;           // se abren lento
        setPixel(OJO_IZQ, b);
        setPixel(OJO_DER, b);
        uBit.sleep(50);
    }
    uBit.sleep(120);
}

// ---------------------------------------------------------------------------
// FASE 3: la boca tiembla un poquito (el labio a punto de llorar)
// ---------------------------------------------------------------------------
static void bocaTiembla()
{
    // el MEDIO de la boca tiembla al azar; las esquinas quedan firmes
    for (int i = 0; i < 14; i++) {              // ~0.7s
        uBit.display.image.setPixelValue(1, 3, 200 + uBit.random(55));
        uBit.display.image.setPixelValue(2, 3, 190 + uBit.random(65));
        uBit.display.image.setPixelValue(3, 3, 200 + uBit.random(55));
        uBit.sleep(50);
    }
    // restaura la boca completa
    for (int i = 0; i < 5; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// FASE EXTRA: la LAGRIMA (de vez en cuando). Se junta bajo un ojo al azar,
// brilla como el agua y baja por la mejilla con estela hasta el menton.
// ---------------------------------------------------------------------------
static void lagrima()
{
    int ex = uBit.random(2) ? 3 : 1;   // de cual ojo cae (izq o der)
    uBit.serial.send("LAGRIMA\n");    // debug: confirmar por serial

    // 1) Se junta bajo el ojo (fade in + destello de agua)
    for (int s = 0; s <= 5; s++) {
        uBit.display.image.setPixelValue(ex, 2, 220 * s / 5);
        uBit.sleep(35);
    }
    uBit.display.image.setPixelValue(ex, 2, 110);   // destello
    uBit.sleep(90);
    uBit.display.image.setPixelValue(ex, 2, 235);
    uBit.sleep(70);

    // 2) Cae: la gota avanza y deja estela (dos pixeles a la vez)
    uBit.display.image.setPixelValue(ex, 2, 90);
    uBit.display.image.setPixelValue(ex, 3, 240);
    uBit.sleep(140);
    uBit.display.image.setPixelValue(ex, 3, 90);
    uBit.display.image.setPixelValue(ex, 4, 240);   // llega al menton
    uBit.sleep(160);

    // 3) Se apaga y limpia la estela
    uBit.display.image.setPixelValue(ex, 4, 0);
    uBit.sleep(90);
    uBit.display.image.setPixelValue(ex, 2, 0);
    uBit.display.image.setPixelValue(ex, 3, 0);

    // restaura la boca (la lagrima paso por la curva del labio)
    for (int i = 0; i < 5; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// La animacion de tristeza: UNA pasada del bucle.
// Cada 4-7 pasadas (impredecible) cae la lagrima.
// ---------------------------------------------------------------------------
static int hastaLagrima = 4;   // pasadas restantes para la lagrima
static int pasada = 0;         // contador de pasadas (debug)

void animarTristeVieja()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada.
    if (revisarSerial()) return;
    uBit.serial.printf("T%d\n", ++pasada);   // debug: cada pasada

    // La cara base se dibuja UNA vez (idempotente)
    dibujarCaraBase();
    if (revisarSerial()) return;

    respiraLenta();      // 1: respira lento
    if (revisarSerial()) return;
    parpadeoPesado();    // 2: parpadeo pesado
    uBit.sleep(180);
    if (revisarSerial()) return;
    bocaTiembla();       // 3: la boca tiembla
    if (revisarSerial()) return;
    respiraLenta();      // 4: respira otra vez
    if (revisarSerial()) return;
    parpadeoPesado();    // 5: otro parpadeo pesado
    if (revisarSerial()) return;

    // 6: de vez en cuando, la lagrima cae (impredecible)
    if (--hastaLagrima <= 0) {
        lagrima();
        hastaLagrima = 4 + uBit.random(4);   // 4-7 pasadas
    }
}
