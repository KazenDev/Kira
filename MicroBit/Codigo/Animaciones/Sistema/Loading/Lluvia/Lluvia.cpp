/**
 * Lluvia.cpp - Patron de carga 3: LLUVIA CON TORMENTA 🌧️⚡ (v2)
 *
 * v2 (remodelada): adios a las gotas que saltaban de pixel en pixel a
 * 20ms. Ahora cada gota tiene posicion FLOAT y se mueve a 60fps (16ms):
 *   - SUB-PIXEL: la gota cruza entre 2 pixeles y el brillo se REPARTE
 *     (interpolacion) -> cae suave como seda, sin saltos
 *   - DIRECCION LEGIBLE: cabeza brillante + estela arriba (90/35) -> se
 *     ve claramente que la lluvia CAE
 *   - VELOCIDAD NATURAL E IMPREDECIBLE: cada gota tiene su propia
 *     velocidad al azar (lluvia fina lenta vs gotas gordas rapidas x1.8),
 *     spawns escalonados al azar (0.4-2.3s), densidad moderada (5 gotas)
 *   - SALPICADURA: al tocar el piso destella en 3 pixeles (x, x+/-1)
 *   - TORMENTA: cada 7-12s un RAYO: flash de cielo (pantalla a 170) ->
 *     rayo en ZIGZAG que baja -> fade -> RETUMBO (3 pulsos como el eco
 *     del trueno) -> la lluvia se intensifica (todas respawnean pronto)
 *   - BUCLE CONTINUO: estado static + sin clear (la lluvia sigue)
 */
#include "Lluvia.h"
#include "../LoadingBase.h"

static const int N_GOTAS = 5;
static float gx[N_GOTAS];      // columna 0-4
static float gy[N_GOTAS];      // posicion y continua (-0.6..4.2)
static float gv[N_GOTAS];      // velocidad px/frame (0.040..0.090)
static int   gTimer[N_GOTAS];  // espera de spawn o salpicadura (frames)
static int   gFase[N_GOTAS];   // 0=espera, 1=cayendo, 2=salpicando
static bool  iniciado = false;

// Primera vez: repartir gotas con delays escalonados + azar
static void iniciarGotas()
{
    for (int i = 0; i < N_GOTAS; i++) {
        gFase[i] = 0;
        gTimer[i] = 20 + i * 45 + uBit.random(70);
        gx[i] = uBit.random(5);
    }
    iniciado = true;
}

// Render de una gota cayendo: cabeza interpolada entre 2 pixeles + estela
static void pintarGota(int i)
{
    if (gy[i] < 0.0f) return;        // aun fuera de pantalla (evita frac negativo)
    int y0 = (int)gy[i];             // piso
    float frac = gy[i] - y0;         // 0..1: cuanto avanzo al pixel de abajo
    int x = (int)gx[i];

    if (y0 >= 0 && y0 <= 4)
        uBit.display.image.setPixelValue(x, y0, (int)(255.0f * (1.0f - frac)));
    if (y0 + 1 >= 0 && y0 + 1 <= 4)
        uBit.display.image.setPixelValue(x, y0 + 1, (int)(255.0f * frac));

    // estela arriba: marca la direccion de caida
    if (y0 - 1 >= 0) uBit.display.image.setPixelValue(x, y0 - 1, 90);
    if (y0 - 2 >= 0) uBit.display.image.setPixelValue(x, y0 - 2, 35);
}

// Actualiza y pinta todas las gotas (un frame a 60fps)
static void actualizarGotas()
{
    for (int i = 0; i < N_GOTAS; i++) {
        if (gFase[i] == 0) {                         // esperando spawn
            if (gTimer[i] > 0) { gTimer[i]--; continue; }
            gFase[i] = 1;
            gy[i] = -0.6f;
            gx[i] = uBit.random(5);
            gv[i] = 0.040f + (float)uBit.random(50) / 1000.0f;
            if (uBit.random(7) == 0) gv[i] *= 1.8f;  // gota gorda rapida (14%)
            continue;
        }
        if (gFase[i] == 2) {                         // salpicando en el piso
            int x = (int)gx[i];
            uBit.display.image.setPixelValue(x, 4, 255);
            if (x > 0) uBit.display.image.setPixelValue(x - 1, 4, 180);
            if (x < 4) uBit.display.image.setPixelValue(x + 1, 4, 180);
            if (gTimer[i] > 0) { gTimer[i]--; continue; }
            gFase[i] = 0;
            gTimer[i] = 25 + uBit.random(120);       // 0.4-2.3s de espera
            continue;
        }
        // cayendo
        gy[i] += gv[i];
        if (gy[i] > 4.2f) {                          // llego al piso
            gFase[i] = 2;
            gTimer[i] = 3;                           // salpicadura breve
            continue;
        }
        pintarGota(i);
    }
}

// Tormenta: flash de cielo -> rayo zigzag -> fade -> retumbo del trueno
static void rayo()
{
    int xs[5];
    xs[0] = uBit.random(5);                          // el rayo baja en zigzag
    for (int r = 1; r < 5; r++) {
        xs[r] = xs[r - 1] + uBit.random(3) - 1;
        if (xs[r] < 0) xs[r] = 0;
        if (xs[r] > 4) xs[r] = 4;
    }
    // 1) FLASH: el cielo entero se ilumina
    for (int f = 0; f < 2; f++) {
        if (frameRastro(92)) return;
        for (int x = 0; x < 5; x++)
            for (int y = 0; y < 5; y++)
                uBit.display.image.setPixelValue(x, y, 170);
        uBit.sleep(16);
    }
    // 2) RAYO: el zigzag queda crispado sobre el cielo que se apaga
    for (int f = 0; f < 2; f++) {
        if (frameRastro(85)) return;
        for (int r = 0; r < 5; r++)
            uBit.display.image.setPixelValue(xs[r], r, 255);
        uBit.sleep(16);
    }
    // 3) FADE del rayo
    for (int f = 0; f < 3; f++) {
        if (frameRastro(80)) return;
        for (int r = 0; r < 5; r++)
            uBit.display.image.setPixelValue(xs[r], r, 160 - f * 40);
        uBit.sleep(16);
    }
    // 4) RETUMBO: ecos sutiles del trueno
    for (int p = 0; p < 3; p++) {
        if (frameRastro(70)) return;
        for (int x = 0; x < 5; x++)
            for (int y = 0; y < 5; y++)
                uBit.display.image.setPixelValue(x, y, 55 - p * 15);
        uBit.sleep(16);
        if (frameRastro(70)) return;
        uBit.sleep(120);                             // pausa entre ecos
    }
    // 5) LA LLUVIA SE INTENSIFICA: todas respawnean pronto
    for (int i = 0; i < N_GOTAS; i++) {
        gFase[i] = 0;
        gTimer[i] = 5 + uBit.random(40);
    }
}

void animarLluvia(int ciclos)
{
    if (!iniciado) iniciarGotas();
    static int proximoRayo = 420 + uBit.random(300);   // 1er rayo a los 7-12s

    for (int c = 0; c < ciclos; c++) {
        for (int f = 0; f < 400; f++) {                // ~6.4s por ciclo
            if (frameRastro(78)) return;               // rastro + abortar si manda algo
            actualizarGotas();
            if (proximoRayo <= 0) {
                proximoRayo = 420 + uBit.random(300);  // proximo rayo en 7-12s
                rayo();                                 // (reseteado ANTES: si aborta, no se repite)
            } else {
                proximoRayo--;
            }
            uBit.sleep(16);
        }
    }
    // NO limpia al final: la lluvia sigue (bucle continuo)
}
