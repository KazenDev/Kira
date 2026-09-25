/*
 * CortinaViejo.cpp - la version ANTERIOR (git show del commit previo), solo
 * para el bench. Funciones renombradas para linkearse junto a la nueva.
 * NO se compila en la firmware.
 */
/**
 * Cortina.cpp - Transicion CORTINA 🎭 (60 FPS, ~2s)
 *
 * La pantalla se cierra como una CORTINA: las columnas de los bordes
 * se apagan primero (con fade, una a una), despues las del medio y
 * por ultimo la central -> todo a negro. Se dibuja la cara nueva y
 * la cortina se ABRE al reves: centro -> bordes.
 *
 * Columnas (x): 0 y 4 se apagan juntas, luego 1 y 3, luego 2.
 * Al abrir: 2, luego 1 y 3, luego 0 y 4.
 */
#include "Cortina.h"

#define FRAMES_COL 21      // frames por par de columnas (~0.33s; 3x21x2=126 ~2s)

// ---------------------------------------------------------------------------
// Apaga (cerrar) o enciende (abrir) un par de columnas con fade.
// cols = las dos columnas a mover (pueden ser iguales: la central)
// ---------------------------------------------------------------------------
static void moverColumnas(int c1, int c2, bool cerrar)
{
    for (int s = 0; s <= FRAMES_COL; s++) {
        int br = cerrar ? (255 * (FRAMES_COL - s)) / FRAMES_COL
                        : (255 * s) / FRAMES_COL;
        for (int y = 0; y < 5; y++) {
            uBit.display.image.setPixelValue(c1, y, br);
            uBit.display.image.setPixelValue(c2, y, br);
        }
        uBit.sleep(16);   // 60 FPS
    }
}

// ---------------------------------------------------------------------------
// La CORTINA
// ---------------------------------------------------------------------------
void transicionCortinaVieja(EmocionActual destino)
{
    // 1) CERRAR: bordes (0,4) -> medio (1,3) -> centro (2,2)
    moverColumnas(0, 4, true);
    moverColumnas(1, 3, true);
    moverColumnas(2, 2, true);

    // 2) Dibujo la cara nueva (la cortina cerrada la oculta)
    uBit.display.image.clear();
    dibujarCaraDestino(destino);

    // 3) ABRIR: centro (2,2) -> medio (1,3) -> bordes (0,4)
    moverColumnas(2, 2, false);
    moverColumnas(1, 3, false);
    moverColumnas(0, 4, false);

    // Final limpio (por si acaso)
    dibujarCaraDestino(destino);
}
