/**
 * Sorprendido.cpp - La emocion SORPRENDIDO 😲 (en reposo)
 *
 * BULE INFINITO SIN REINICIOS: la cara NUNCA se borra de la pantalla.
 * Ojos bien abiertos + boca "o" y micro-movimientos de ASOMBRO:
 *
 *   1. Ojos que se DILATAN: miran fijo, bien abiertos y brillantes
 *   2. La boca "o" se ABRE en un diamante grande (como "WOW") y se
 *      cierra, dos veces por pasada
 *   3. De vez en cuando (1/3 de pasadas, al azar) un PULSO DE ASOMBRO:
 *      todo el brillo sube DE GOLPE hasta 255 y vuelve (el susto)
 *   4. Parpadeo RARISIMO: rapido y poco frecuente (casi no parpadea)
 *
 * Misma arquitectura que la alegria/tristeza/enojo: UNA pasada por
 * llamada, el bucle infinito vive en Principal.cpp que revisa el serial.
 * La boca que HABLA ("OH OH") vive en HablarSorprendido.cpp (TALK).
 */
#include "Sorprendido.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// La boca "o": centro (2,3) + el diamante grande (2,2),(1,3),(3,3),(2,4)
static const uint8_t O_CENTRO[2] = {2, 3};
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

// ---------------------------------------------------------------------------
// La cara base: se dibuja UNA vez y se queda (nunca se borra en el bucle)
// ---------------------------------------------------------------------------
void mostrarCaraSorprendido()   // publica: el primer frame (para las transiciones)
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();

    // Ojos bien abiertos
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);

    // Boca "o" pequeña (el centro)
    setPixel(O_CENTRO, 255);
}

// ---------------------------------------------------------------------------
// FASE 1: los ojos se DILATAN (miran fijo, bien abiertos y brillantes)
// ---------------------------------------------------------------------------
static void ojosDilatan()
{
    // el brillo de los ojos sube suave (pupilas que se dilatan)
    for (int s = 0; s <= 4; s++) {
        int b = 220 + 35 * s / 4;   // 220 -> 255
        setPixel(OJO_IZQ, b);
        setPixel(OJO_DER, b);
        uBit.sleep(30);
    }
    uBit.sleep(280);                 // mirando fijo un momento
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
}

// ---------------------------------------------------------------------------
// FASE 2: la boca "o" se ABRE en diamante grande y se cierra (WOW)
// ---------------------------------------------------------------------------
static void bocaOh()
{
    // abrir: el diamante crece en etapas (top -> lados -> fondo)
    for (int s = 1; s <= 3; s++) {
        setPixel(O_DIAMANTE[0], s >= 1 ? 255 : 0);   // top (2,2)
        setPixel(O_DIAMANTE[1], s >= 2 ? 255 : 0);   // lado izq (1,3)
        setPixel(O_DIAMANTE[2], s >= 2 ? 255 : 0);   // lado der (3,3)
        setPixel(O_DIAMANTE[3], s >= 3 ? 255 : 0);   // fondo (2,4)
        uBit.sleep(40);
    }
    uBit.sleep(180);                 // boc abierta de asombro

    // cerrar: el diamante se achica en etapas hasta quedar SOLO el centro
    // (el loop llega a s=0 para apagar tambien el píxel top (2,2))
    for (int s = 3; s >= 0; s--) {
        setPixel(O_DIAMANTE[0], s >= 1 ? 255 : 0);
        setPixel(O_DIAMANTE[1], s >= 2 ? 255 : 0);
        setPixel(O_DIAMANTE[2], s >= 2 ? 255 : 0);
        setPixel(O_DIAMANTE[3], s >= 3 ? 255 : 0);
        uBit.sleep(40);
    }
    // el centro (2,3) queda siempre encendido
}

// ---------------------------------------------------------------------------
// FASE 3: PULSO DE ASOMBRO (todo el brillo sube DE GOLPE y vuelve)
// ---------------------------------------------------------------------------
static void pulsoAsombro()
{
    uBit.serial.send("PULSO\n");     // debug: el susto aleatorio
    for (int b = 90; b <= 255; b += 15) {
        uBit.display.setBrightness(b);
        uBit.sleep(25);
    }
    for (int b = 255; b >= 90; b -= 15) {
        uBit.display.setBrightness(b);
        uBit.sleep(25);
    }
    uBit.display.setBrightness(90);
    uBit.sleep(150);
}

// ---------------------------------------------------------------------------
// FASE 4: parpadeo RARISIMO (rapido y poco frecuente)
// ---------------------------------------------------------------------------
static void parpadeoRaro()
{
    for (int s = 0; s <= 2; s++) {
        int b = 255 - 255 * s / 2;
        setPixel(OJO_IZQ, b);
        setPixel(OJO_DER, b);
        uBit.sleep(20);
    }
    uBit.sleep(90);
    for (int s = 0; s <= 2; s++) {
        int b = 255 * s / 2;
        setPixel(OJO_IZQ, b);
        setPixel(OJO_DER, b);
        uBit.sleep(20);
    }
    uBit.sleep(200);
}

// ---------------------------------------------------------------------------
// La animacion de sorpresa: UNA pasada del bucle.
// ---------------------------------------------------------------------------
static int pasada = 0;   // contador de pasadas (debug)

void animarSorprendido()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada.
    if (revisarSerial()) return;
    uBit.serial.printf("S%d\n", ++pasada);   // debug: cada pasada

    // La cara base se dibuja UNA vez (idempotente)
    mostrarCaraSorprendido();
    if (revisarSerial()) return;

    ojosDilatan();       // 1: ojos bien abiertos mirando fijo
    if (revisarSerial()) return;
    bocaOh();            // 2: la boca "o" se abre y cierra
    uBit.sleep(120);
    if (revisarSerial()) return;
    bocaOh();            // otro "wow"
    if (revisarSerial()) return;

    // 3: de vez en cuando (1/3) el susto: todo el brillo sube de golpe
    if (uBit.random(3) == 0)
        pulsoAsombro();
    if (revisarSerial()) return;

    parpadeoRaro();      // 4: parpadeo rarisimo
}
