/**
 * Cometa.cpp - Patron de carga 0: COMETA ☄️ (v3, orbita CONTINUA)
 *
 * v3 (bucle sin reinicios): la posicion de la orbita es una variable
 * static que NUNCA vuelve a 0 entre pasadas del bucle. El patron ademas
 * NO limpia la pantalla al terminar: el rastro se desvanece solo con el
 * decay y la cometa sigue girando donde se quedo -> circulo infinito,
 * sin el "salto" repentino de reinicio.
 *
 * v2 (base): 3 sub-frames por posicion (~60ms) = vuelta lenta de ~1s.
 * Rastro largo con decay 80% (cola visible ~6 posiciones).
 */
#include "Cometa.h"
#include "../LoadingBase.h"

void animarCometa(int vueltas)
{
    // posicion continua entre pasadas: la orbita nunca se reinicia
    static int pos = 0;

    for (int v = 0; v < vueltas; v++) {
        for (int p = 0; p < 16; p++) {
            // 3 sub-frames por posicion: la cometa va LENTA y suave
            for (int s = 0; s < 3; s++) {
                if (frameRastro(80)) return;   // rastro largo + abortar si manda algo
                uBit.display.image.setPixelValue(RING[pos][0], RING[pos][1], 255);
                uBit.sleep(20);
            }
            pos = (pos + 1) % 16;              // siguiente posicion (continua)
        }
    }
    // NO limpia al final: el rastro se desvanece solo y la orbita sigue
}
