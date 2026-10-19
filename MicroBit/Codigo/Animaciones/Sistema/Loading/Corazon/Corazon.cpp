/**
 * Corazon.cpp - Patron de carga 11: CORAZON 💓
 *
 * Late como uno de verdad: dos golpes seguidos (lub-dub) y una pausa.
 * El golpe es el corazon GRANDE a full; entre golpes queda el chico,
 * bajando de brillo. Ciclo de ~1 s (60 latidos por minuto, tranquilo).
 */
#include "Corazon.h"
#include "../LoadingBase.h"

static const uint8_t GRANDE[5] = {0x0A, 0x1F, 0x1F, 0x0E, 0x04};
static const uint8_t CHICO[5]  = {0x00, 0x0A, 0x0E, 0x04, 0x00};

static void dibujar(const uint8_t filas[5], int brillo)
{
    uBit.display.image.clear();
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            if (filas[y] & (0x10 >> x))
                uBit.display.image.setPixelValue(x, y, brillo);
}

void animarCorazon(int ciclos)
{
    static int t = 0;   // frame dentro del latido (continuo entre pasadas)

    for (int c = 0; c < ciclos * 6; c++) {
        for (int f = 0; f < 50; f++) {          // 50 frames x 20 ms = 1 s
            if (frameSerial()) return;
            int k = t % 50;
            if (k < 5)        dibujar(GRANDE, 255);   // lub
            else if (k < 9)   dibujar(CHICO, 200);
            else if (k < 14)  dibujar(GRANDE, 230);   // dub
            else              dibujar(CHICO, 200 - (k - 14) * 2);  // pausa, se apaga suave
            uBit.sleep(20);
            t++;
        }
    }
}
