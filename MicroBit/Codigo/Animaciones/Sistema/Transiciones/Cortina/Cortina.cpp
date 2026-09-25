/**
 * Cortina.cpp - Transicion CORTINA 🎭 (60 FPS, 132 frames = ~2.1s)
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

#define FRAMES_COL 21      // frames por par de columnas; 6 pares x 22 = 132

// ---------------------------------------------------------------------------
// Apaga (cerrar) o enciende (abrir) un par de columnas con fade.
// cols = las dos columnas a mover (pueden ser iguales: la central)
//
// Devuelve false si llego un comando de la IA: en ese caso la transicion
// entera se aborta. Antes esta rutina (y las otras dos transiciones) duraban
// 2 segundos SIN MIRAR EL SERIAL, asi que la cara se enteraba de un cambio de
// emocion hasta 2,08 s tarde (medido). Con un chequeo por frame son 16 ms.
//
// OJO: moverColumnas devuelve false apenas encuentra un comando, pero el
// comando ya fue procesado adentro de revisarSerial(). Si ese comando era un
// cambio de emocion, hacerTransicion() guardo el destino pendiente en vez de
// anidar otra transicion (que desbordaria la pila: Morfosis solo reserva 580
// bytes por nivel). Ver el guard de Transiciones.cpp.
// ---------------------------------------------------------------------------
static bool moverColumnas(int c1, int c2, bool cerrar)
{
    for (int s = 0; s <= FRAMES_COL; s++) {
        if (revisarSerial()) return false;

        int br = cerrar ? (255 * (FRAMES_COL - s)) / FRAMES_COL
                        : (255 * s) / FRAMES_COL;

        if (c1 == c2) {
            // La columna central se pasaba a si misma: el original escribia
            // los mismos 5 pixeles DOS veces por frame, 22 frames por pasada
            // y 2 pasadas (cerrar y abrir) = 220 escrituras al pedo.
            for (int y = 0; y < 5; y++)
                uBit.display.image.setPixelValue(c1, y, br);
        } else {
            for (int y = 0; y < 5; y++) {
                uBit.display.image.setPixelValue(c1, y, br);
                uBit.display.image.setPixelValue(c2, y, br);
            }
        }
        uBit.sleep(16);   // 60 FPS
    }
    return true;
}

// ---------------------------------------------------------------------------
// La CORTINA
// ---------------------------------------------------------------------------
void transicionCortina(EmocionActual destino)
{
    // 1) CERRAR: bordes (0,4) -> medio (1,3) -> centro (2,2)
    if (!moverColumnas(0, 4, true))  return;
    if (!moverColumnas(1, 3, true))  return;
    if (!moverColumnas(2, 2, true))  return;
    if (revisarSerial()) return;

    // 2) Dibujo la cara nueva (la cortina cerrada la oculta)
    uBit.display.image.clear();
    dibujarCaraDestino(destino);

    // 3) ABRIR: centro (2,2) -> medio (1,3) -> bordes (0,4)
    if (!moverColumnas(2, 2, false)) return;
    if (!moverColumnas(1, 3, false)) return;
    if (!moverColumnas(0, 4, false)) return;
    if (revisarSerial()) return;

    // Final limpio (por si acaso)
    dibujarCaraDestino(destino);
}
