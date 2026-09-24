/**
 * LoadingBase.cpp - Implementacion de la base COMPARTIDA de los loadings
 */
#include "LoadingBase.h"
#include "../Sistema.h"   // para revisarSerial() -> abortar si llega un comando

// ---------------------------------------------------------------------------
// Rastro real: baja el brillo de TODOS los pixeles encendidos (decay).
// Cada frame la luz deja atras una estela que se desvanece SOLA.
// ---------------------------------------------------------------------------
void decaerRastro(int porciento)
{
    for (int x = 0; x < 5; x++)
        for (int y = 0; y < 5; y++) {
            int v = uBit.display.image.getPixelValue(x, y);
            if (v > 4)
                uBit.display.image.setPixelValue(x, y, v * porciento / 100);
            else if (v > 0)
                uBit.display.image.setPixelValue(x, y, 0);
        }
}

// Un frame con rastro: decae + si llego comando, avisa para abortar.
// OJO: NO limpia la pantalla despues de procesar el comando. revisarSerial()
// ya dibujo la nueva emocion (ej: SAD) y si limpiaramos la borrariamos
// (el bucle principal no redibuja las emociones estaticas). El patron
// aborta justo despues y la siguiente animacion redibuja todo desde cero.
bool frameRastro(int porciento)
{
    decaerRastro(porciento);
    if (revisarSerial()) return true;
    return false;
}

// Un frame sin rastro: solo chequea serial (para patrones que no decaen)
bool frameSerial()
{
    if (revisarSerial()) return true;
    return false;
}

// ---------------------------------------------------------------------------
// Geometrias compartidas
// ---------------------------------------------------------------------------
const uint8_t RING[16][2] = {
    {0,0},{1,0},{2,0},{3,0},{4,0},   // borde superior (izq -> der)
    {4,1},{4,2},{4,3},{4,4},          // borde derecho (arriba -> abajo)
    {3,4},{2,4},{1,4},{0,4},          // borde inferior (der -> izq)
    {0,3},{0,2},{0,1}                 // borde izquierdo (abajo -> arriba)
};

const uint8_t SPIRAL[25][2] = {
    {0,0},{1,0},{2,0},{3,0},{4,0},
    {4,1},{4,2},{4,3},{4,4},
    {3,4},{2,4},{1,4},{0,4},
    {0,3},{0,2},{0,1},
    {1,1},{2,1},{3,1},{3,2},{3,3},{2,3},{1,3},{1,2},
    {2,2}
};

// Enciende un anillo: 0 = centro, 1 = cuadrado 3x3, 2 = borde
void setAnillo(int radius, int brightness)
{
    if (radius == 0) {
        uBit.display.image.setPixelValue(2, 2, brightness);
    }
    else if (radius == 1) {
        for (int x = 1; x <= 3; x++)
            for (int y = 1; y <= 3; y++)
                if (!(x == 2 && y == 2))
                    uBit.display.image.setPixelValue(x, y, brightness);
    }
    else {
        for (int i = 0; i < 16; i++)
            uBit.display.image.setPixelValue(RING[i][0], RING[i][1], brightness);
    }
}
