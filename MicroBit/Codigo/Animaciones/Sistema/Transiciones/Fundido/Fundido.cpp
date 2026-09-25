/**
 * Fundido.cpp - Transicion FUNDIDO 🌫️ (60 FPS, 126 frames = ~2.0s)
 *
 * La cara actual se apaga SUAVEMENTE a negro (brillo global 90->0),
 * se dibuja la cara nueva y el brillo sube de 0->90: la nueva cara
 * "aparece" de la oscuridad.
 *
 * Usa el brillo GLOBAL (setBrightness) con pasos de 16ms = 60 FPS.
 */
#include "Fundido.h"

#define FRAMES_BR 62      // frames por mitad (63 x 2 = 126 frames)

// ---------------------------------------------------------------------------
// El QUANTUM REAL del PWM del display
//
//   NRF52LedMatrix.h:  NRF52_LED_MATRIX_CLOCK_FREQUENCY = 16000000
//                      NRF52_LED_MATRIX_FREQUENCY       = 60
//   NRF52LedMatrix.cpp:112  timerPeriod = 16 000 000 / (60 * 5) = 53333
//   NRF52LedMatrix.cpp:113  quantum = (timerPeriod * brillo) / (256*255)
//
// O sea: quantum = 0.8169 * brillo. El brillo global SUBE de a saltos de
// ~1,22 unidades, asi que dos brillos enteros seguidos muy seguido dan el
// MISMO quantum: la llamada a setBrightness() no cambia ni un ciclo del PWM.
// Y en el M0+ cada setBrightness() es una division entera por software.
//
// Con el deadband, de 126 llamadas bajan a 113. Las 13 que se ahorran no se
// veian. Mismo resultado visual, menos trabajo.
// ---------------------------------------------------------------------------
#define BRILLO_QUANTUM(b)    ((int)(((53333L * (b)) / 65280L)))

// Si se aborta a mitad de un fade, el brillo global puede quedar en cualquier
// lado (incluso en 0, con la cara invisible). Se restaura antes de salir: la
// cara tiene que quedar legible pase lo que pase.
static void restaurarBrillo()
{
    uBit.display.setBrightness(90);
}

// ---------------------------------------------------------------------------
// El FUNDIDO
// ---------------------------------------------------------------------------
void transicionFundido(EmocionActual destino)
{
    int ultimoQuantum = -1;

    // 1) Apago la cara actual: brillo 90 -> 0 (fade out)
    for (int s = 0; s <= FRAMES_BR; s++) {
        if (revisarSerial()) { restaurarBrillo(); return; }

        int b = 90 - (90 * s) / FRAMES_BR;
        int q = BRILLO_QUANTUM(b);
        if (q != ultimoQuantum) {
            uBit.display.setBrightness(b);
            ultimoQuantum = q;
        }
        uBit.sleep(16);   // 60 FPS
    }
    if (revisarSerial()) { restaurarBrillo(); return; }

    // 2) Dibujo la cara nueva (a brillo 0 queda invisible)
    uBit.display.image.clear();
    dibujarCaraDestino(destino);

    // 3) La cara aparece: brillo 0 -> 90 (fade in)
    ultimoQuantum = -1;
    for (int s = 0; s <= FRAMES_BR; s++) {
        if (revisarSerial()) { restaurarBrillo(); return; }

        int b = (90 * s) / FRAMES_BR;
        int q = BRILLO_QUANTUM(b);
        if (q != ultimoQuantum) {
            uBit.display.setBrightness(b);
            ultimoQuantum = q;
        }
        uBit.sleep(16);
    }
    if (revisarSerial()) { restaurarBrillo(); return; }

    // Final limpio: dibujo una vez mas con el brillo de la emocion
    dibujarCaraDestino(destino);
}
