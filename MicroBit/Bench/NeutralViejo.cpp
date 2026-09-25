/**
 * NeutralViejo.cpp - la version ANTERIOR (git show), solo para el bench.
 * NO se compila en la firmware.
 */
/**
 * Neutral.cpp - La emocion NEUTRAL 😑 (en reposo) - la cara "bruh"
 *
 * BULE INFINITO SIN REINICIOS: la cara NUNCA se borra de la pantalla.
 * Ojos CERRADOS (## . ## = dos lineas de parpados) + boca recta:
 *
 *   1. Respira CALMADA: el brillo sube/baja a ritmo medio (ni
 *      agitada como el enojo ni lenta como la tristeza)
 *   2. PEEK: los ojos cerrados se ENTREABREN de vez en cuando (los
 *      parpados exteriores bajan y las pupilas quedan al descubierto)
 *      y vuelven a cerrarse - como el que levanta la vista aburrido
 *   3. La boca recta a veces se RETUERCE un poquito (un "meh":
 *      un lado del labio se levanta)
 *
 * Misma arquitectura que las demas: UNA pasada por llamada, el bucle
 * infinito vive en Principal.cpp que revisa el serial.
 * La boca que HABLA (los 3 LEDs parpadeando) vive en
 * HablarNeutral.cpp (TALK).
 */
#include "Neutral.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
// Ojos CERRADOS: parpados exteriores (0,1)(4,1) + pupilas (1,1)(3,1)
// (## . ##) - NO son 4 ojos: es cada ojo cerrado de 2 pixeles
static const uint8_t PARPADO_IZQ[2] = {0, 1};
static const uint8_t PARPADO_DER[2] = {4, 1};
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Boca RECTA neutra: (1,3)(2,3)(3,3)
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
void mostrarCaraNeutralVieja()   // publica: el primer frame (para las transiciones)
{
    uBit.display.setBrightness(85);
    uBit.display.image.clear();

    // Ojos cerrados completos: parpados + pupilas (todo encendido)
    setPixel(PARPADO_IZQ, 255);
    setPixel(PARPADO_DER, 255);
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);

    // Boca recta neutra
    for (int i = 0; i < 3; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// FASE 1+3: respira CALMADA (ritmo medio)
// ---------------------------------------------------------------------------
static void respiraNeutral()
{
    for (int b = 65; b <= 115; b += 5) {
        uBit.display.setBrightness(b);
        uBit.sleep(30);
    }
    for (int b = 115; b >= 65; b -= 5) {
        uBit.display.setBrightness(b);
        uBit.sleep(30);
    }
    uBit.display.setBrightness(85);
    uBit.sleep(150);
}

// ---------------------------------------------------------------------------
// FASE 2: PEEK - los ojos cerrados se ENTREABREN (los parpados bajan y
// quedan las pupilas al descubierto) y vuelven a cerrarse
// ---------------------------------------------------------------------------
static void ojosPeek()
{
    // entreabrir: los parpados exteriores se apagan del todo (quedan las
    // pupilas solas = ojos abiertos) y vuelven a cerrarse
    for (int s = 0; s <= 4; s++) {
        int p = 255 * (4 - s) / 4;   // 255 -> 0
        setPixel(PARPADO_IZQ, p);
        setPixel(PARPADO_DER, p);
        uBit.sleep(30);
    }
    uBit.sleep(420);                 // mirando un momento (pupilas solas)
    // volver a cerrar: los parpados suben
    for (int s = 0; s <= 4; s++) {
        int p = 255 * s / 4;         // 0 -> 255
        setPixel(PARPADO_IZQ, p);
        setPixel(PARPADO_DER, p);
        uBit.sleep(30);
    }
    uBit.sleep(120);
}

// ---------------------------------------------------------------------------
// FASE 4: la boca se RETUERCE un poquito (un "meh": el lado izq sube)
// ---------------------------------------------------------------------------
static void bocaReto()
{
    // el lado izquierdo del labio se levanta (meh) y vuelve
    for (int s = 0; s <= 3; s++) {
        uBit.display.image.setPixelValue(1, 3, 255 - 200 * s / 3);   // baja
        uBit.display.image.setPixelValue(1, 2, 200 * s / 3);         // sube el de arriba
        uBit.sleep(35);
    }
    uBit.sleep(300);                 // el meh un momento
    for (int s = 3; s >= 0; s--) {
        uBit.display.image.setPixelValue(1, 3, 255 - 200 * s / 3);
        uBit.display.image.setPixelValue(1, 2, 200 * s / 3);
        uBit.sleep(35);
    }
    // restaura la boca recta completa
    for (int i = 0; i < 3; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// La animacion neutral: UNA pasada del bucle.
// ---------------------------------------------------------------------------
static int pasada = 0;   // contador de pasadas (debug)

void animarNeutralVieja()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada.
    if (revisarSerial()) return;
    uBit.serial.printf("N%d\n", ++pasada);   // debug: cada pasada

    // La cara base se dibuja UNA vez (idempotente)
    mostrarCaraNeutralVieja();
    if (revisarSerial()) return;

    respiraNeutral();    // 1: respira calmada
    if (revisarSerial()) return;
    ojosPeek();          // 2: los ojos cerrados se entreabren
    if (revisarSerial()) return;
    respiraNeutral();    // 3: respira calmada otra vez
    if (revisarSerial()) return;
    bocaReto();          // 4: la boca se retuerce (meh)
    if (revisarSerial()) return;
    ojosPeek();          // 5: otro peek
}
