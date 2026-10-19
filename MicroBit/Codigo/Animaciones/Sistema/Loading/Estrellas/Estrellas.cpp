/**
 * Estrellas.cpp - Patron de carga 12: ESTRELLAS ✨
 *
 * Un cielo nocturno: cada tanto se prende una estrella al azar (brillo
 * distinto cada una) y se apaga LENTO con el rastro (decay 94%), asi que
 * siempre hay varias titilando a distintas alturas de brillo. A veces cruza
 * una estrella fugaz en diagonal.
 */
#include "Estrellas.h"
#include "../LoadingBase.h"

void animarEstrellas(int ciclos)
{
    static int fugaz = -1;   // paso de la estrella fugaz (-1 = no hay)

    for (int c = 0; c < ciclos; c++) {
        for (int f = 0; f < 300; f++) {
            if (frameRastro(94)) return;
            if (uBit.random(6) == 0) {
                int x = uBit.random(5), y = uBit.random(5);
                if (uBit.display.image.getPixelValue(x, y) < 40)
                    uBit.display.image.setPixelValue(x, y, 110 + uBit.random(146));
            }
            // estrella fugaz: rara, cruza de arriba-izquierda a abajo-derecha
            if (fugaz < 0 && uBit.random(400) == 0) fugaz = 0;
            if (fugaz >= 0) {
                int p = fugaz / 3;
                if (p < 5) uBit.display.image.setPixelValue(p, p, 255);
                if (++fugaz >= 15) fugaz = -1;
            }
            uBit.sleep(20);
        }
    }
}
