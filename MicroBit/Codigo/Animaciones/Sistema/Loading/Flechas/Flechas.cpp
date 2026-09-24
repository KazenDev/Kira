/**
 * Flechas.cpp - Patron de carga 6: FLECHAS GIRATORIAS 🔄
 *
 * El spinner circular clasico: 4 flechas de luz giran por el borde
 * (RING de 16 posiciones), cada una con punta brillante, cuerpo y
 * cola que se desvanece. Interpolacion sub-pixel a 60fps: la punta
 * se reparte entre dos pixeles segun la fraccion, asi el giro es
 * fluido (sin saltos de pixel). Reemplaza al viejo Corazon (que
 * no parecia loading).
 */
#include "Flechas.h"
#include "../LoadingBase.h"

// Pinta UNA flecha cuyo centro (punta) esta en la posicion float `pos`
// del RING (0..16). La punta se reparte entre dos pixeles (sub-pixel),
// el cuerpo va atras y la cola se desvanece con el rastro.
static void pintarFlecha(float pos)
{
    // punta: pixel actual (pA) con el resto de brillo, siguiente (pB)
    // con la fraccion (así el giro se desliza fluido entre pixeles)
    int base = (int)pos;
    float frac = pos - (float)base;
    int pA = base % 16;
    int pB = (base + 1) % 16;

    int brA = (int)(255.0f * (1.0f - frac));
    int brB = (int)(255.0f * frac);
    if (brA > uBit.display.image.getPixelValue(RING[pA][0], RING[pA][1]))
        uBit.display.image.setPixelValue(RING[pA][0], RING[pA][1], brA);
    if (brB > uBit.display.image.getPixelValue(RING[pB][0], RING[pB][1]))
        uBit.display.image.setPixelValue(RING[pB][0], RING[pB][1], brB);

    // cuerpo y cola: tambien con interpolacion sub-pixel para que la
    // cometa se deslice ENTERA (no en bloques). El rastro (decay)
    // agrega el degradado largo detras.
    int pC = (base - 1 + 16) % 16;
    int pD = (base - 2 + 16) % 16;
    int brC = 170 + (int)(85.0f * frac);   // 170..255 (sigue a la punta)
    int brD = 90  + (int)(80.0f * frac);   // 90..170
    if (brC > uBit.display.image.getPixelValue(RING[pC][0], RING[pC][1]))
        uBit.display.image.setPixelValue(RING[pC][0], RING[pC][1], brC);
    if (brD > uBit.display.image.getPixelValue(RING[pD][0], RING[pD][1]))
        uBit.display.image.setPixelValue(RING[pD][0], RING[pD][1], brD);
}

void animarFlechas(int vueltas)
{
    static float ang = 0.0f;   // angulo actual (0..16), continuo en bucle
    static bool iniciado = false;
    if (!iniciado) {
        ang = (float)(uBit.random(1600)) / 100.0f;   // arranque al azar
        iniciado = true;
    }

    for (int v = 0; v < vueltas; v++) {
        for (int f = 0; f < 400; f++) {              // ~6.4s por pasada
            if (frameRastro(80)) return;             // rastro + abortar serial

            // 4 flechas espaciadas en el circulo (90 grados)
            pintarFlecha(ang);
            pintarFlecha(ang + 4.0f);
            pintarFlecha(ang + 8.0f);
            pintarFlecha(ang + 12.0f);

            // velocidad: ~0.10/16 por frame -> ~2.7s por vuelta
            ang += 0.10f;
            if (ang >= 16.0f) ang -= 16.0f;

            uBit.sleep(16);
        }
    }
    // NO limpia al final: bucle continuo (estado estatico, sin saltos)
}
