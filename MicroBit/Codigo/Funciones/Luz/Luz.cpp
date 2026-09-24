/**
 * Luz.cpp - FUNCION: leer la luz ambiente del micro:bit
 *
 * CODAL: uBit.display.readLightLevel() devuelve 0..255 usando la matriz
 * LED como fotosensor. NOTA: mientras se mide la luz, la matriz se usa
 * como sensor (parpadea un instante y no muestra la cara). Por eso la
 * medicion es rapida y devuelve el control al display.
 */
#include "Luz.h"

int leerLuz()
{
    // el modo de medicion necesita una pasada para estabilizarse
    uBit.display.readLightLevel();
    uBit.sleep(20);
    return uBit.display.readLightLevel();
}
