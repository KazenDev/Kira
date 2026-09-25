/**
 * CansadoViejo.cpp - la version ANTERIOR (git show del commit previo al
 * patron de frame), solo para el bench. NO se compila en la firmware.
 */
/**
 * Cansado.cpp - La emocion CANSADO 😪 (en reposo) - "ya no puedo mas"
 *
 * BULE INFINITO SIN REINICIOS: la cara NUNCA se borra de la pantalla.
 * El look: ojos normales pero que se cierran LENTO y pesado + boca
 * chica que BOSTEZA en grande:
 *
 *   1. Respira CANSADA: el brillo pulsa lento y BAJO (75 max - la
 *      cara del que se esta durmiendo es apagada)
 *   2. Parpadeo PESADO: los ojos se cierran LENTO (6 pasos), quedan
 *      cerrados un BUEN rato (350ms) y abren lento - el parpadeo del
 *      que lucha por mantener los ojos abiertos
 *   3. BOSTEZO: la boca chica se abre EN GRANDE (3x3, un bostezo
 *      completo), queda abierta un rato y se cierra
 *   4. Otro parpadeo pesado
 *   5. CABECEO: el brillo se va a pique (75->40) y vuelve - la cabeza
 *      que cae por el sueno
 *
 * Misma arquitectura que las demas: UNA pasada por llamada, el bucle
 * infinito vive en Principal.cpp que revisa el serial.
 * La boca que HABLA (lento, con sueno) vive en HablarCansado.cpp (TALK).
 */
#include "Cansado.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
// Ojos normales (pero que se cierran pesado)
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Boca CHICA (cerrada, fila 3)
static const uint8_t BOCA_CHICA[3][2] = {
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
void mostrarCaraCansadoVieja()   // publica: el primer frame (para las transiciones)
{
    uBit.display.setBrightness(75);   // apagada: el cansado no brilla
    uBit.display.image.clear();

    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    for (int i = 0; i < 3; i++)
        setPixel(BOCA_CHICA[i], 255);
}

// ---------------------------------------------------------------------------
// FASE 1: respira CANSADA (lenta y baja)
// ---------------------------------------------------------------------------
static void respiraCansada()
{
    for (int b = 55; b <= 95; b += 5) {
        uBit.display.setBrightness(b);
        uBit.sleep(40);
    }
    for (int b = 95; b >= 55; b -= 5) {
        uBit.display.setBrightness(b);
        uBit.sleep(40);
    }
    uBit.display.setBrightness(75);
    uBit.sleep(200);
}

// ---------------------------------------------------------------------------
// FASE 2: parpadeo PESADO - los ojos se cierran LENTO y quedan cerrados
// un buen rato (el que se esta durmiendo)
// ---------------------------------------------------------------------------
static void parpadeoPesado()
{
    // cerrar lento
    for (int s = 0; s <= 5; s++) {
        int v = 255 - 255 * s / 5;
        setPixel(OJO_IZQ, v);
        setPixel(OJO_DER, v);
        uBit.sleep(45);
    }
    uBit.sleep(350);   // cerrados un buen rato
    // abrir lento
    for (int s = 5; s >= 0; s--) {
        int v = 255 - 255 * s / 5;
        setPixel(OJO_IZQ, v);
        setPixel(OJO_DER, v);
        uBit.sleep(45);
    }
    uBit.sleep(150);
}

// ---------------------------------------------------------------------------
// FASE 3: BOSTEZO - la boca chica se abre EN GRANDE y se cierra
// ---------------------------------------------------------------------------
static void bostezo()
{
    // 1) abrir: la fila de arriba y la de abajo aparecen de a poco
    for (int s = 0; s <= 3; s++) {
        int v = 255 * s / 3;
        uBit.display.image.setPixelValue(1, 2, v);
        uBit.display.image.setPixelValue(2, 2, v);
        uBit.display.image.setPixelValue(3, 2, v);
        uBit.display.image.setPixelValue(1, 4, v);
        uBit.display.image.setPixelValue(2, 4, v);
        uBit.display.image.setPixelValue(3, 4, v);
        uBit.sleep(45);
    }
    uBit.sleep(400);   // bostezo bien abierto

    // 2) cerrar: las filas extra se apagan de a poco
    for (int s = 3; s >= 0; s--) {
        int v = 255 * s / 3;
        uBit.display.image.setPixelValue(1, 2, v);
        uBit.display.image.setPixelValue(2, 2, v);
        uBit.display.image.setPixelValue(3, 2, v);
        uBit.display.image.setPixelValue(1, 4, v);
        uBit.display.image.setPixelValue(2, 4, v);
        uBit.display.image.setPixelValue(3, 4, v);
        uBit.sleep(45);
    }
    // 3) restaura la boca chica (la fila del medio nunca se apago)
    for (int i = 0; i < 3; i++)
        setPixel(BOCA_CHICA[i], 255);
}

// ---------------------------------------------------------------------------
// FASE 5: CABECEO - el brillo se va a pique (la cabeza cae por el sueno)
// ---------------------------------------------------------------------------
static void cabeceo()
{
    for (int s = 0; s <= 3; s++) {
        uBit.display.setBrightness(75 - 35 * s / 3);   // 75 -> 40
        uBit.sleep(40);
    }
    uBit.sleep(150);
    for (int s = 3; s >= 0; s--) {
        uBit.display.setBrightness(75 - 35 * s / 3);   // 40 -> 75
        uBit.sleep(40);
    }
    uBit.sleep(100);
}

// ---------------------------------------------------------------------------
// La animacion cansado: UNA pasada del bucle.
// ---------------------------------------------------------------------------
static int pasada = 0;   // contador de pasadas (debug)

void animarCansadoVieja()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada.
    if (revisarSerial()) return;
    uBit.serial.printf("C%d\n", ++pasada);   // debug: cada pasada

    // La cara base se dibuja UNA vez (idempotente)
    mostrarCaraCansadoVieja();
    if (revisarSerial()) return;

    respiraCansada();    // 1: respira lenta y baja
    if (revisarSerial()) return;
    parpadeoPesado();    // 2: parpadeo pesado (se cierran lento)
    if (revisarSerial()) return;
    bostezo();           // 3: bostezo grande
    if (revisarSerial()) return;
    parpadeoPesado();    // 4: otro parpadeo pesado
    if (revisarSerial()) return;
    cabeceo();           // 5: cabeceo (se duerme un segundo)
}
