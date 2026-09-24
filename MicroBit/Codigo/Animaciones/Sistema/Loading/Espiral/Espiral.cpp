/**
 * Espiral.cpp - Patron de carga 1: ESPIRAL 🌀 (v2, doble sentido)
 *
 * v2 (remodelada): en vez del camino fijo de 25 puntos, el punto viaja por
 * una espiral PARAMETRICA real:
 *   - Lenta: ~20ms por frame, ciclo completo (2 espirales) = ~3.2s
 *   - Espiral matematica: radio que oscila (borde <-> centro) + angulo que
 *     barre suavemente -> se dibuja SOLA con su rastro de luz
 *   - DOBLE SENTIDO: espirala hacia adentro y afuera girando en un sentido
 *     (CW), y al llegar al borde exterior invierte el giro y espirala hacia
 *     el OTRO lado (CCW), tambien adentro y afuera. La transicion es
 *     continua (sin saltos): el punto se voltea en el borde y en el centro
 *     la posicion es la misma sin importar el angulo.
 *   - Bucle continuo: contador static que nunca se reinicia + NO limpia la
 *     pantalla al final (el rastro se desvanece solo, como el cometa v3).
 *
 * Geometria: centro (2,2), radio maximo 2.2 (alcanza casi las esquinas).
 *   r(t) = R * |cos(pi t / (N/2))|   ->  borde, centro, borde, centro...
 *   a(t) = 2PI * (1 - cos(2pi t / N))->  barre 0 -> 4PI -> 0 (ida y vuelta)
 * Cuando r va hacia el centro el angulo CRECE (giro CW); al pasar el centro
 * sigue creciendo mientras r sube (espiral continua hacia afuera); al
 * llegar al borde el angulo DECRECE (giro CCW) y el patron se refleja.
 */
#include "Espiral.h"
#include "../LoadingBase.h"
#include <math.h>

void animarEspiral(int ciclos)
{
    static int t = 0;              // frame continuo entre pasadas del bucle
    const int N = 160;             // frames por ciclo completo (2 espirales)
    const double R = 2.2;           // radio maximo
    const double KPI = 3.14159265;  // (no usar PI: CODAL ya la define como macro)

    for (int c = 0; c < ciclos; c++) {
        for (int f = 0; f < N; f++) {
            if (frameRastro(82)) return;        // rastro largo + abortar si manda algo

            int tt = t % N;                     // t acotado: sin overflow ni perdida de precision
            double fase = 2.0 * KPI * tt / N;
            double r = R * fabs(cos(KPI * tt / (N / 2)));  // borde<->centro oscilando
            double ang = 2.0 * KPI * (1.0 - cos(fase));   // barre ida y vuelta suave

            int px = (int)(2.0 + r * cos(ang) + 0.5);
            int py = (int)(2.0 + r * sin(ang) + 0.5);
            if (px < 0) px = 0;  if (px > 4) px = 4;
            if (py < 0) py = 0;  if (py > 4) py = 4;

            uBit.display.image.setPixelValue(px, py, 255);
            uBit.sleep(20);
            t++;
        }
    }
    // NO limpia al final: el rastro se desvanece solo (bucle continuo)
}
