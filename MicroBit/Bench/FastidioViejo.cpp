/**
 * FastidioViejo.cpp - la version ANTERIOR (git show), solo para el bench.
 * NO se compila en la firmware.
 */
/**
 * Fastidio.cpp - La emocion FASTIDIO 😤 (en reposo) - "ya me canse de esto"
 *
 * BULE INFINITO SIN REINICIOS: la cara NUNCA se borra de la pantalla.
 * El look (el que mando el usuario):
 *
 *     # # . # #     <- cejas APRETADAS (4 pixeles de ceja)
 *     # . . # .     <- ojos DESPLAZADOS hacia afuera (mirando de reojo)
 *     . . . . .
 *     . # # # .     <- boca recta
 *     . . . . .
 *
 *   1. Respira FASTIDIADA: el brillo pulsa medio-fuerte (el suspiro
 *      del que aguanta el mal humor)
 *   2. Ceja que TIEMBLA: los bordes internos de las cejas tiemblan
 *      rapido (la irritacion que sube)
 *   3. Parpadeo DURO: los ojos se cierran DE GOLPE un instante (el
 *      que intenta mantener la calma)
 *   4. "Tsk": el centro del labio pulsa (el tsk tsk del fastidio)
 *
 * Misma arquitectura que las demas: UNA pasada por llamada, el bucle
 * infinito vive en Principal.cpp que revisa el serial.
 * La boca que HABLA (apretada, sin ganas) vive en
 * HablarFastidio.cpp (TALK).
 */
#include "Fastidio.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
// Cejas apretadas: 2 pixeles por ceja, pegadas a las esquinas
static const uint8_t CEJA_IZQ[2] = {0, 0};     // extremo izq de la ceja izq
static const uint8_t CEJA_IZQ_IN[2] = {1, 0};  // borde interno (tiembla)
static const uint8_t CEJA_DER[2] = {4, 0};     // extremo der de la ceja der
static const uint8_t CEJA_DER_IN[2] = {3, 0};  // borde interno (tiembla)

// Ojos DESPLAZADOS hacia afuera (mirando de reojo, fastidiado)
static const uint8_t OJO_IZQ[2] = {0, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Boca RECTA: (1,3)(2,3)(3,3)
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
// La cara base: se dibuja UNA vez y se queda (nunca se borra en el bucle)
// ---------------------------------------------------------------------------
void mostrarCaraFastidioVieja()   // publica: el primer frame (para las transiciones)
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();

    // Cejas apretadas
    setPixel(CEJA_IZQ, 255);
    setPixel(CEJA_IZQ_IN, 255);
    setPixel(CEJA_DER, 255);
    setPixel(CEJA_DER_IN, 255);

    // Ojos de reojo
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);

    // Boca recta
    for (int i = 0; i < 3; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// FASE 1: respira FASTIDIADA (ritmo medio-fuerte - el suspiro del mal humor)
// ---------------------------------------------------------------------------
static void respiraFastidio()
{
    for (int b = 70; b <= 130; b += 6) {
        uBit.display.setBrightness(b);
        uBit.sleep(25);
    }
    for (int b = 130; b >= 70; b -= 6) {
        uBit.display.setBrightness(b);
        uBit.sleep(25);
    }
    uBit.display.setBrightness(90);
    uBit.sleep(150);
}

// ---------------------------------------------------------------------------
// FASE 2: la ceja TIEMBLA (los bordes internos parpadean rapido -
// la irritacion que sube)
// ---------------------------------------------------------------------------
static void cejaTiembla()
{
    for (int i = 0; i < 3; i++) {
        setPixel(CEJA_IZQ_IN, 80);
        setPixel(CEJA_DER_IN, 80);
        uBit.sleep(60);
        setPixel(CEJA_IZQ_IN, 255);
        setPixel(CEJA_DER_IN, 255);
        uBit.sleep(60);
    }
}

// ---------------------------------------------------------------------------
// FASE 3: parpadeo DURO (los ojos se cierran de golpe un instante)
// ---------------------------------------------------------------------------
static void parpadeoDuro()
{
    setPixel(OJO_IZQ, 0);
    setPixel(OJO_DER, 0);
    uBit.sleep(130);
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    uBit.sleep(110);
}

// ---------------------------------------------------------------------------
// FASE 4: "tsk" - el centro del labio pulsa (el tsk tsk del fastidio)
// ---------------------------------------------------------------------------
static void bocaTsk()
{
    for (int i = 0; i < 2; i++) {
        setPixel(BOCA[1], 90);       // (2,3) se atenua -> tsk
        uBit.sleep(80);
        setPixel(BOCA[1], 255);
        uBit.sleep(80);
    }
}

// ---------------------------------------------------------------------------
// La animacion fastidio: UNA pasada del bucle.
// ---------------------------------------------------------------------------
static int pasada = 0;   // contador de pasadas (debug)

void animarFastidioVieja()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada.
    if (revisarSerial()) return;
    uBit.serial.printf("F%d\n", ++pasada);   // debug: cada pasada

    // La cara base se dibuja UNA vez (idempotente)
    mostrarCaraFastidioVieja();
    if (revisarSerial()) return;

    respiraFastidio();   // 1: respira fastidiada (suspiro)
    if (revisarSerial()) return;
    cejaTiembla();       // 2: la ceja tiembla (irritacion)
    if (revisarSerial()) return;
    parpadeoDuro();      // 3: parpadeo duro (aguanta la calma)
    if (revisarSerial()) return;
    bocaTsk();           // 4: tsk tsk
}
