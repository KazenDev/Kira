/**
 * Pensativa.cpp - Patron de carga 10: PENSATIVA 🤔
 *
 * La unica carga que es una CARA: Kira pensando. Los ojos van arriba
 * (fila 0) y miran a un lado, al centro, al otro, con pausas como quien
 * busca una idea; la boca de costado ("mmm") y abajo los puntos
 * suspensivos se prenden de a uno. Parpadea de vez en cuando.
 *
 * Sin rastro: se redibuja entera cada frame (es una cara, no una estela).
 */
#include "Pensativa.h"
#include "../LoadingBase.h"

void animarPensativa(int ciclos)
{
    static int t = 0;   // frames continuos: la mirada no se reinicia

    for (int c = 0; c < ciclos; c++) {
        for (int f = 0; f < 240; f++) {
            if (frameSerial()) return;
            uBit.display.image.clear();

            // mirada: izquierda -> centro -> derecha -> centro (~1,4 s cada una)
            int fase = (t / 70) % 4;
            int dx = fase == 0 ? -1 : (fase == 2 ? 1 : 0);
            bool parpadeo = (t % 190) < 5;
            if (!parpadeo) {
                uBit.display.image.setPixelValue(1 + dx, 0, 255);
                uBit.display.image.setPixelValue(3 + dx, 0, 255);
            }
            // boca de costado: "mmm"
            uBit.display.image.setPixelValue(2, 2, 110);
            uBit.display.image.setPixelValue(3, 2, 110);
            // puntos suspensivos que se van prendiendo
            int puntos = (t / 22) % 4;
            for (int i = 0; i < puntos; i++)
                uBit.display.image.setPixelValue(1 + i, 4, 150);

            uBit.sleep(20);
            t++;
        }
    }
}
