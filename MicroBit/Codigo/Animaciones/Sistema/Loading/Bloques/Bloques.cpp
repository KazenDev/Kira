/**
 * Bloques.cpp - Patron de carga 13: BLOQUES 🧱 (mini-Tetris)
 *
 * Caen bloques (de uno o de dos de ancho) en columnas al azar y se apilan.
 * Cuando una fila se llena destella tres veces y se borra; lo de arriba
 * baja. Si la pila llega al techo, se vacia de abajo para arriba y vuelve a
 * empezar. Todo el estado es `static`: el lote corta y retoma sin saltos.
 */
#include "Bloques.h"
#include "../LoadingBase.h"

static uint8_t pila[5] = {0, 0, 0, 0, 0};   // bits por fila (bit 0x10 = columna 0)
static int px = -1, py = 0, ancho = 1;       // la pieza que cae (-1 = ninguna)
static int reloj = 0;
static int destello = 0, filaLlena = -1;     // fila que destella antes de borrarse
static int vaciando = 0;

static uint8_t mascara(int x, int w)
{
    uint8_t m = 0;
    for (int i = 0; i < w; i++) m |= (0x10 >> (x + i));
    return m;
}

static void dibujar()
{
    uBit.display.image.clear();
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++) {
            bool lleno = pila[y] & (0x10 >> x);
            int v = lleno ? 150 : 0;
            if (y == filaLlena && lleno) v = (destello / 4) % 2 ? 255 : 30;
            if (v) uBit.display.image.setPixelValue(x, y, v);
        }
    if (px >= 0)
        for (int i = 0; i < ancho; i++)
            uBit.display.image.setPixelValue(px + i, py, 255);
}

static void paso()
{
    // 1) destello de una fila llena, despues se borra y baja lo de arriba
    if (filaLlena >= 0) {
        if (++destello >= 24) {
            for (int y = filaLlena; y > 0; y--) pila[y] = pila[y - 1];
            pila[0] = 0;
            filaLlena = -1;
            destello = 0;
        }
        return;
    }
    // 2) la pila toco el techo: se vacia de abajo para arriba
    if (vaciando) {
        if (++vaciando % 6 == 0) {
            for (int y = 4; y > 0; y--) pila[y] = pila[y - 1];
            pila[0] = 0;
            if (!(pila[0] | pila[1] | pila[2] | pila[3] | pila[4])) vaciando = 0;
        }
        return;
    }
    // 3) pieza nueva
    if (px < 0) {
        ancho = uBit.random(3) == 0 ? 2 : 1;
        px = uBit.random(6 - ancho);
        py = 0;
        if (pila[0] & mascara(px, ancho)) { vaciando = 1; px = -1; }
        return;
    }
    // 4) cae un renglon cada 7 frames
    if (++reloj % 7) return;
    uint8_t m = mascara(px, ancho);
    if (py == 4 || (pila[py + 1] & m)) {
        pila[py] |= m;
        if (pila[py] == 0x1F) { filaLlena = py; destello = 0; }
        px = -1;
    } else {
        py++;
    }
}

void animarBloques(int ciclos)
{
    for (int c = 0; c < ciclos; c++) {
        for (int f = 0; f < 300; f++) {
            if (frameSerial()) return;
            paso();
            dibujar();
            uBit.sleep(20);
        }
    }
}
