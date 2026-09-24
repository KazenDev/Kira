/**
 * Luz.cpp - Patron de carga 9: CIRCULO DE CARGA CON LUZ 🕯️📊
 *
 * Una BARRA DE CARGA REAL controlada por el SENSOR DE LUZ de la
 * micro:bit (los LEDs de la matriz se usan como fotodiodos):
 *
 *   - readLightLevel() devuelve 0..255 (luz ambiente).
 *   - LUZ = CARGA (como un panel solar): la acercas a una lampara
 *     y el circulo se LLENA; la tapas con la mano y se VACIA.
 *   - El circulo es el borde (RING de 16 posiciones) que se llena
 *     en sentido horario, con sub-pixel en el frente (se desliza
 *     fluido, no salta) y rastro que se desvanece.
 *   - Al llegar al 100%: FLASH de celebracion + el circulo completo
 *     "respira" (brillo que sube y baja suave) mientras espera.
 *
 * v2 - AUTO-CALIBRACION 🎯
 *   El sensor v2 necesita BASTANTE luz directa (usa solo la fila
 *   superior de LEDs). Para que el demo funcione en cualquier
 *   ambiente, al iniciar (o con el comando serial "CALIB"):
 *     1. Fase TAPA (icono ▼, 3s): el usuario tapa el sensor ->
 *        se mide el MINIMO de luz.
 *     2. Fase LUZ (icono ☀, 3s): el usuario le apunta luz ->
 *        se mide el MAXIMO.
 *   Despues el circulo mapea [min, max] -> 0..16 posiciones.
 *   Si no hubo diferencia (no taparon o no iluminaron) -> fallback
 *   a 0..255, la animacion nunca se rompe.
 *
 * Reemplaza al viejo Kira (marquee de letras sin pulir).
 */
#include "Luz.h"
#include "../LoadingBase.h"
#include <math.h>

// Estado continuo (bucle sin saltos)
static float nivel = 0.0f;       // nivel de carga suavizado (0..16 posiciones)
static bool  lleno = false;      // para el flash de celebracion
static int   flash = 0;          // frames del flash
static float respira = 0.0f;     // fase de la "respiracion" del circulo lleno

// Calibracion: mapeo [calMin, calMax] -> 0..16
static int  calMin = 0, calMax = 255;
static bool calibrado = false;
static bool recalibrar = false;

// Iconos de las fases de calibracion (5x5): ▼ "tapame", ☀ "dame luz"
static const uint8_t ICONO_TAPA[5][5] = {
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0},
    {255, 255, 255, 255, 255},
    {0, 255, 255, 255, 0},
    {0, 0, 255, 0, 0}
};
static const uint8_t ICONO_LUZ[5][5] = {
    {255, 0, 255, 0, 255},
    {0, 255, 0, 255, 0},
    {255, 0, 255, 0, 255},
    {0, 255, 0, 255, 0},
    {255, 0, 255, 0, 255}
};

// Lo llama el comando serial "CALIB": pide recalibrar sin cortar el loading
void recalibrarLuz()
{
    recalibrar = true;
}

static void pintarIcono(const uint8_t icono[5][5])
{
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            uBit.display.image.setPixelValue(x, y, icono[y][x]);
}

// Calibra el minimo (tapado) y el maximo (luz directa) del ambiente actual.
// Si llega un comando serial a mitad, aborta (se reintenta la proxima vez).
static void calibrarLuz()
{
    uBit.display.readLightLevel();   // activa el modo de medicion

    // --- Fase 1: TAPA (3s, 180 frames a 60fps) -> minimo ---
    uBit.serial.send("CALIB TAPA 3s...\n");
    int tapaMin = 255;
    for (int f = 0; f < 180; f++) {
        if (frameSerial()) return;               // abortar si llega comando
        if (f % 40 < 20) pintarIcono(ICONO_TAPA);
        else uBit.display.image.clear();
        int l = uBit.display.readLightLevel();
        if (l < tapaMin) tapaMin = l;
        uBit.sleep(16);
    }
    uBit.serial.printf("CALIB tapa min=%d\n", tapaMin);

    // --- Fase 2: LUZ (3s) -> maximo ---
    uBit.serial.send("CALIB LUZ 3s...\n");
    int luzMax = 0;
    for (int f = 0; f < 180; f++) {
        if (frameSerial()) return;
        if (f % 40 < 20) pintarIcono(ICONO_LUZ);
        else uBit.display.image.clear();
        int l = uBit.display.readLightLevel();
        if (l > luzMax) luzMax = l;
        uBit.sleep(16);
    }
    uBit.serial.printf("CALIB luz max=%d\n", luzMax);

    // --- Guarda: sin diferencia real -> usar el rango completo ---
    if (luzMax - tapaMin < 40) {
        calMin = 0; calMax = 255;
        uBit.serial.send("CALIB fallback 0..255\n");
    } else {
        calMin = tapaMin; calMax = luzMax;
        uBit.serial.send("CALIB listo\n");
    }

    calibrado = true;
    recalibrar = false;
    uBit.display.image.clear();

    // arranque limpio del circulo tras calibrar
    nivel = 0.0f; lleno = false; flash = 0; respira = 0.0f;
}

// Pinta el circulo segun el nivel (0..16): posiciones enteras llenas a
// pleno, el frente con sub-pixel (brillo parcial que se desliza).
static void pintarCirculo()
{
    for (int i = 0; i < 16; i++) {
        float pos = (float)i;
        int br;
        if (pos < nivel - 1.0f) {
            br = 255;                            // zona llena (pleno)
        } else if (pos < nivel) {
            float frac = nivel - pos;            // frente sub-pixel
            br = (int)(255.0f * frac);
            if (br < 0) br = 0;
            if (br > 255) br = 255;
        } else {
            br = 0;                              // zona vacia
        }
        // no bajar lo que ya esta mas brillante (el rastro decae solo)
        if (br > uBit.display.image.getPixelValue(RING[i][0], RING[i][1]))
            uBit.display.image.setPixelValue(RING[i][0], RING[i][1], br);
    }
}

void animarLuz(int vueltas)
{
    // Primera vez (o cuando llega "CALIB"): calibrar tapado/luz al ambiente
    if (!calibrado || recalibrar)
        calibrarLuz();

    static int dbg = 0;
    for (int v = 0; v < vueltas; v++) {
        for (int f = 0; f < 400; f++) {              // ~6.4s por pasada
            if (frameRastro(80)) return;             // rastro + abortar serial

            // --- Sensor de luz: mapeo CALIBRADO -> 0..16 posiciones ---
            int lvl = uBit.display.readLightLevel();
            float t = (float)(lvl - calMin) / (float)(calMax - calMin);
            if (t < 0.0f) t = 0.0f;
            if (t > 1.0f) t = 1.0f;
            float objetivo = t * 16.0f;
            nivel += (objetivo - nivel) * 0.10f;     // suavizado (no salta)

            // --- Flash + respiracion al completar ---
            if (nivel >= 15.5f && !lleno) {
                lleno = true;
                flash = 5;                           // flash de celebracion
            }
            if (nivel < 13.0f && lleno) lleno = false;

            if (flash > 0) {
                flash--;
                setAnillo(2, 255);                   // circulo blanco pleno
                setAnillo(0, 255);
            } else if (lleno) {
                // el circulo lleno "respira": brillo sube y baja suave
                respira += 0.05f;
                int br = 160 + (int)(95.0f * sinf(respira));
                for (int i = 0; i < 16; i++)
                    uBit.display.image.setPixelValue(RING[i][0], RING[i][1], br);
                uBit.display.image.setPixelValue(2, 2, br);
            } else {
                pintarCirculo();
            }

            // debug: luz cruda + nivel cada ~1.2s (calibrar)
            if (++dbg >= 75) {
                dbg = 0;
                uBit.serial.printf("L%d N%d\n", lvl, (int)(nivel * 100.0f));
            }

            uBit.sleep(16);              // ~60 fps
        }
    }
    // NO limpia al final: bucle continuo (estado estatico)
}
