/**
 * EnojadoViejo.cpp - la version ANTERIOR (git show), solo para el bench.
 * NO se compila en la firmware.
 */
/**
 * Enojado.cpp - La emocion ENOJADO 😠 (en reposo)
 *
 * BULE INFINITO SIN REINICIOS: la cara NUNCA se borra de la pantalla.
 * Cejas fruncidas + boca con dientes apretados y micro-movimientos
 * TERSOS y BRUSCOS:
 *
 *   1. Respira AGITADA: el brillo sube/baja RAPIDO (mas que la alegria)
 *   2. Cejas que se FRUNCEN en oleadas: se tensan (se engrosan hacia
 *      el centro) y se relajan, en oleadas cada vez mas seguidas
 *   3. Parpadeo BRUSCO: los ojos se cierran RAPIDO, quedan cerrados
 *      un instante y abren rapido (sin fades lentos)
 *   4. La mandibula APRIETA: el hueco de los dientes (2,4) se enciende
 *      y apaga en pulsos = dientes apretados
 *
 * Misma arquitectura que la alegria/tristeza: UNA pasada por llamada,
 * el bucle infinito vive en Principal.cpp que revisa el serial.
 * La boca que HABLA (gruñido) vive en HablarEnojado.cpp (TALK).
 */
#include "Enojado.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Cejas (esquinas) y su extension al fruncirse (hacia el centro)
static const uint8_t CEJA_IZQ[2] = {0, 0};
static const uint8_t CEJA_DER[2] = {4, 0};
static const uint8_t CEJA_IZQ_F[2] = {1, 0};
static const uint8_t CEJA_DER_F[2] = {3, 0};

// Boca con dientes: linea de dientes (1,3)(2,3)(3,3) + mandibula
// (0,4)(1,4)(3,4)(4,4). El hueco (2,4) se cierra al APRETAR.
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

// ---------------------------------------------------------------------------
// La cara base: se dibuja UNA vez y se queda (nunca se borra en el bucle)
// ---------------------------------------------------------------------------
void mostrarCaraEnojadoVieja()   // publica: el primer frame (para las transiciones)
{
    uBit.display.setBrightness(95);   // el enojo es INTENSO (no tenue)
    uBit.display.image.clear();

    // Cejas fruncidas (base)
    setPixel(CEJA_IZQ, 255);
    setPixel(CEJA_DER, 255);

    // Ojos
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);

    // Boca con dientes (hueco de la mandibula abierto)
    for (int i = 0; i < 7; i++)
        setPixel(BOCA[i], 255);
}

// ---------------------------------------------------------------------------
// FASE 1+4: respira AGITADA (brillo sube/baja RAPIDO)
// ---------------------------------------------------------------------------
static void respiraAgitada()
{
    for (int rep = 0; rep < 2; rep++) {
        for (int b = 55; b <= 145; b += 8) {
            uBit.display.setBrightness(b);
            uBit.sleep(18);
        }
        for (int b = 145; b >= 55; b -= 8) {
            uBit.display.setBrightness(b);
            uBit.sleep(18);
        }
    }
    uBit.display.setBrightness(95);
    uBit.sleep(120);
}

// ---------------------------------------------------------------------------
// FASE 2: las cejas se FRUNCEN en oleadas (se tensan y relajan)
// ---------------------------------------------------------------------------
static void cejasFruncen()
{
    for (int wave = 0; wave < 3; wave++) {
        // relajadas: solo las esquinas
        setPixel(CEJA_IZQ, 255);
        setPixel(CEJA_DER, 255);
        setPixel(CEJA_IZQ_F, 0);
        setPixel(CEJA_DER_F, 0);
        uBit.sleep(200);
        // fruncidas: se engrosan hacia el centro (oleada mas rapida)
        setPixel(CEJA_IZQ_F, 255);
        setPixel(CEJA_DER_F, 255);
        uBit.sleep(200 - wave * 30);   // cada oleada mas tensa
    }
}

// ---------------------------------------------------------------------------
// FASE 3: parpadeo BRUSCO (rapido, sin fades lentos)
// ---------------------------------------------------------------------------
static void parpadeoBrusco()
{
    for (int s = 0; s <= 2; s++) {
        int b = 255 - 255 * s / 2;
        setPixel(OJO_IZQ, b);
        setPixel(OJO_DER, b);
        uBit.sleep(20);
    }
    uBit.sleep(80);                    // cerrados un instante
    for (int s = 0; s <= 2; s++) {
        int b = 255 * s / 2;
        setPixel(OJO_IZQ, b);
        setPixel(OJO_DER, b);
        uBit.sleep(20);
    }
    uBit.sleep(150);
}

// ---------------------------------------------------------------------------
// FASE EXTRA: la mandibula APRIETA (el hueco de los dientes pulsa)
// ---------------------------------------------------------------------------
static void bocaAprieta()
{
    for (int i = 0; i < 6; i++) {
        uBit.display.image.setPixelValue(2, 4, (i % 2) ? 255 : 60);
        uBit.sleep(90);
    }
    uBit.display.image.setPixelValue(2, 4, 255);   // queda apretada
    uBit.sleep(150);
}

// ---------------------------------------------------------------------------
// La animacion de enojo: UNA pasada del bucle.
// ---------------------------------------------------------------------------
static int pasada = 0;   // contador de pasadas (debug)

void animarEnojadoVieja()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada.
    if (revisarSerial()) return;
    uBit.serial.printf("E%d\n", ++pasada);   // debug: cada pasada

    // La cara base se dibuja UNA vez (idempotente)
    mostrarCaraEnojadoVieja();
    if (revisarSerial()) return;

    respiraAgitada();    // 1: respira agitada
    if (revisarSerial()) return;
    parpadeoBrusco();    // 2: parpadeo brusco
    uBit.sleep(100);
    if (revisarSerial()) return;
    cejasFruncen();      // 3: las cejas se fruncen en oleadas
    if (revisarSerial()) return;
    bocaAprieta();       // 4: la mandibula aprieta
    if (revisarSerial()) return;
    parpadeoBrusco();    // 5: otro parpadeo brusco
    if (revisarSerial()) return;
    respiraAgitada();    // 6: respira agitada otra vez
}
