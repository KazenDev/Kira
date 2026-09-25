/**
 * MiedoViejo.cpp - la version ANTERIOR (git show), solo para el bench.
 * NO se compila en la firmware.
 */
/**
 * Miedo.cpp - La emocion MIEDO 😨 (en reposo) - "¡AAAAH!"
 *
 * BULE INFINITO SIN REINICIOS: la cara NUNCA se borra de la pantalla.
 * El look (el que mando el usuario):
 *
 *     . # . # .     <- cejas LEVANTADAS de terror
 *     # . . . #     <- ojos BIEN ABIERTOS en las esquinas
 *     . # # # .     \
 *     . # . # .      > la boca GRANDE abierta (gritando)
 *     . # # # .     /
 *
 *   1. Corazon ACELERADO: el brillo late rapido e irregular (pum-pum)
 *   2. TIEMBLA todo: el brillo vibra al azar (el cuerpo tiembla)
 *   3. Los ojos MIRAN al centro (la amenaza) y vuelven
 *   4. Parpadeo RAPIDISIMO: parpadea varias veces seguidas (miedo)
 *   5. Labios que TIEMBLAN: la boca abierta vibra
 *
 * Misma arquitectura que las demas: UNA pasada por llamada, el bucle
 * infinito vive en Principal.cpp que revisa el serial.
 * La boca que HABLA (tartamudea) vive en HablarMiedo.cpp (TALK).
 */
#include "Miedo.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
// Cejas levantadas (un pixel arriba del ojo, hacia el centro)
static const uint8_t CEJA_IZQ[2] = {1, 0};
static const uint8_t CEJA_DER[2] = {3, 0};

// Ojos BIEN ABIERTOS en las esquinas (mirando fijo, de terror)
static const uint8_t OJO_IZQ[2] = {0, 1};
static const uint8_t OJO_DER[2] = {4, 1};

// "Pupilas" hacia adentro (cuando los ojos MIRAN la amenaza)
static const uint8_t PUPILA_IZQ[2] = {1, 1};
static const uint8_t PUPILA_DER[2] = {3, 1};

// Boca GRANDE abierta (el grito): 8 pixeles
static const uint8_t BOCA[8][2] = {
    {1, 2}, {2, 2}, {3, 2},      // fila 2: .###.
    {1, 3}, {3, 3},              // fila 3: .#.#.
    {1, 4}, {2, 4}, {3, 4}       // fila 4: .###.
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
void mostrarCaraMiedoVieja()   // publica: el primer frame (para las transiciones)
{
    uBit.display.setBrightness(100);
    uBit.display.image.clear();

    setPixel(CEJA_IZQ, 255);
    setPixel(CEJA_DER, 255);
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    for (int i = 0; i < 8; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// FASE 1: corazon ACELERADO (pulso rapido e irregular del brillo - pum-pum)
// ---------------------------------------------------------------------------
static void corazonAcelerado()
{
    for (int i = 0; i < 3; i++) {
        uBit.display.setBrightness(160);
        uBit.sleep(50);
        uBit.display.setBrightness(100);
        uBit.sleep(80);
        uBit.display.setBrightness(140);
        uBit.sleep(40);
        uBit.display.setBrightness(100);
        uBit.sleep(70);
    }
}

// ---------------------------------------------------------------------------
// FASE 2: TIEMBLA todo (el brillo vibra al azar - el cuerpo tiembla)
// ---------------------------------------------------------------------------
static void tiemblaCuerpo()
{
    for (int i = 0; i < 12; i++) {
        uBit.display.setBrightness(85 + uBit.random(70));   // 85..155 al azar
        uBit.sleep(30);
    }
    uBit.display.setBrightness(100);
}

// ---------------------------------------------------------------------------
// FASE 3: los ojos MIRAN al centro (la amenaza) y vuelven
// ---------------------------------------------------------------------------
static void ojoMira()
{
    setPixel(OJO_IZQ, 0);
    setPixel(OJO_DER, 0);
    setPixel(PUPILA_IZQ, 255);
    setPixel(PUPILA_DER, 255);
    uBit.sleep(180);
    setPixel(PUPILA_IZQ, 0);
    setPixel(PUPILA_DER, 0);
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    uBit.sleep(100);
}

// ---------------------------------------------------------------------------
// FASE 4: parpadeo RAPIDISIMO (varias veces seguidas - el ojo del miedo)
// ---------------------------------------------------------------------------
static void parpadeoRapido()
{
    for (int i = 0; i < 3; i++) {
        setPixel(OJO_IZQ, 0);
        setPixel(OJO_DER, 0);
        uBit.sleep(60);
        setPixel(OJO_IZQ, 255);
        setPixel(OJO_DER, 255);
        uBit.sleep(90);
    }
}

// ---------------------------------------------------------------------------
// FASE 5: los labios TIEMBLAN (la boca abierta vibra)
// ---------------------------------------------------------------------------
static void bocaTiembla()
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 8; j++) setPixel(BOCA[j], 150);
        uBit.sleep(50);
        for (int j = 0; j < 8; j++) setPixel(BOCA[j], 255);
        uBit.sleep(50);
    }
}

// ---------------------------------------------------------------------------
// La animacion miedo: UNA pasada del bucle.
// ---------------------------------------------------------------------------
static int pasada = 0;   // contador de pasadas (debug)

void animarMiedoVieja()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada.
    if (revisarSerial()) return;
    uBit.serial.printf("M%d\n", ++pasada);   // debug: cada pasada

    // La cara base se dibuja UNA vez (idempotente)
    mostrarCaraMiedoVieja();
    if (revisarSerial()) return;

    corazonAcelerado();  // 1: pum-pum (corazon acelerado)
    if (revisarSerial()) return;
    tiemblaCuerpo();     // 2: temblor de todo el cuerpo
    if (revisarSerial()) return;
    ojoMira();           // 3: los ojos miran la amenaza
    if (revisarSerial()) return;
    parpadeoRapido();    // 4: parpadeo rapidisimo
    if (revisarSerial()) return;
    bocaTiembla();       // 5: los labios tiemblan
}
