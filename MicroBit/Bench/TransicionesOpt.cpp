/**
 * TransicionesOpt.cpp - PROTOTIPO de las transiciones eficientes.
 *
 * NO es la firmware: es la referencia de como deberian verse. Los tres
 * originales duran ~2,0 s y son BLOQUEANTES y SORDOS (no llaman a
 * revisarSerial() en ningun frame), asi que cada cambio de emocion deja la
 * placa muda 2 segundos. Con la IA hablando en una feria, eso se nota.
 *
 * El arreglo es el MISMO patron de frame que se le aplico a Alegria, pero
 * con una diferencia importante: las transiciones se llaman desde
 * procesarComando(), o sea DENTRO del despachador. No se pueden volver
 * funciones de frame sin tocar el bucle principal, asi que la version
 * eficiente se queda sincrona y lo unico que hace es CHECAR EL SERIAL en cada
 * frame y abortar si llego algo. Con eso la ventana sorda baja de ~2,0 s a
 * un frame, sin mover una sola pieza de la arquitectura.
 *
 * Ademas se van divisiones enteras del bucle interno, que en un Cortex-M0+
 * son llamadas a __aeabi_idiv (el M0+ no tiene divisor por hardware).
 */
#include "mock/MicroBit.h"
#include "Animaciones/Sistema/Sistema.h"
#include "Animaciones/Sistema/Transiciones/Transiciones.h"

#include <stdio.h>
#include <math.h>

// ---------------------------------------------------------------------------
// Quantum real del PWM del display (NRF52LedMatrix.h + NRF52LedMatrix.cpp:112)
//
//   timerPeriod = 16 000 000 / (60 Hz * 5 filas) = 53333
//   quantum     = (timerPeriod * brillo) / (256 * 255) = 0.8169 * brillo
//
// O sea: el brillo SUBE de a saltos de ~1,22 unidades. Dos brillos enteros
// seguidos muy seguido dan el MISMO quantum -> la llamada a setBrightness no
// cambia un solo ciclo del PWM. Y setBrightness() en el M0+ hace una division
// entera por software. Con un deadband se evitan esas llamadas.
// ---------------------------------------------------------------------------
#define BRILLO_QUANTUM(br)   ((int)(((53333L * (br)) / 65280L)))
#define BRILLO_MIN_CAMBIO    2      // 2 unidades = ~1.6 quantums: se ve

// ---------------------------------------------------------------------------
// MORFOSIS eficiente
// ---------------------------------------------------------------------------
struct PxOpt { int x, y; };
#define MAX_PX 25
#define FRAMES 125

static int leerPantalla(PxOpt out[MAX_PX])
{
    int n = 0;
    for (int y = 0; y < 5 && n < MAX_PX; y++)
        for (int x = 0; x < 5 && n < MAX_PX; x++)
            if (uBit.display.image.getPixelValue(x, y) > 100) {
                out[n].x = x; out[n].y = y; n++;
            }
    return n;
}

static int dist2(const PxOpt& a, const PxOpt& b)
{
    int dx = a.x - b.x, dy = a.y - b.y;
    return dx * dx + dy * dy;
}

void transicionMorfosisOpt(EmocionActual destino)
{
    PxOpt A[MAX_PX], B[MAX_PX];
    int nA = leerPantalla(A);

    uBit.display.image.clear();
    dibujarCaraDestino(destino);
    int nB = leerPantalla(B);

    uBit.display.image.clear();
    for (int i = 0; i < nA; i++)
        uBit.display.image.setPixelValue(A[i].x, A[i].y, 255);

    int parejaA[MAX_PX];
    bool usadoB[MAX_PX] = { false };
    for (int i = 0; i < nA; i++) {
        int mejor = -1, mejorD = 1000;
        for (int j = 0; j < nB; j++) {
            if (usadoB[j]) continue;
            int d = dist2(A[i], B[j]);
            if (d < mejorD) { mejorD = d; mejor = j; }
        }
        parejaA[i] = mejor;
        if (mejor >= 0) usadoB[mejor] = true;
    }

    int nParejas = 0;
    for (int i = 0; i < nA; i++) if (parejaA[i] >= 0) nParejas++;

    const int dur = FRAMES / 2;      // 62: constante de compilacion
    const int medio = dur / 2;       // 31

    // ── LO NUEVO: los retardos se calculan UNA vez ──────────────────────
    // En el original, `retardo` se recalcula dentro del bucle de frames:
    //     (FRAMES/2) * orden / nParejas
    // nParejas y orden NO cambian entre frames, asi que esa division es
    // invariante del bucle. Y como nParejas es un valor de runtime (no una
    // constante de compilacion), GCC no puede convertirla en multiplicacion:
    // emite una llamada a __aeabi_idiv. En un Cortex-M0+ eso es software puro.
    // Con el array, la division ocurre nA veces en total y no nA*126.
    int retardo[MAX_PX];
    {
        int orden = 0;
        for (int i = 0; i < nA; i++) {
            if (parejaA[i] < 0) { retardo[i] = 0; continue; }
            retardo[i] = nParejas > 0 ? (dur * orden) / nParejas : 0;
            orden++;
        }
    }

    for (int f = 0; f <= FRAMES; f++) {
        // ── LO NUEVO: un frame y se vuelve. Si la IA mando algo, se aborta
        //    aqui mismo en vez de esperar 2 s. ───────────────────────────
        if (revisarSerial()) return;

        uBit.display.image.clear();

        int orden = 0;
        for (int i = 0; i < nA; i++) {
            if (parejaA[i] < 0) continue;
            int j = parejaA[i];
            int t = f - retardo[i];        // division por nParejas: 0, ahora

            if (t < 0)
                uBit.display.image.setPixelValue(A[i].x, A[i].y, 255);
            else if (t >= dur)
                uBit.display.image.setPixelValue(B[j].x, B[j].y, 255);
            else {
                int px = A[i].x + ((B[j].x - A[i].x) * t) / dur;
                int py = A[i].y + ((B[j].y - A[i].y) * t) / dur;
                int br = (t <= medio) ? 60 + (195 * t) / medio
                                      : 255 - (150 * (t - medio)) / (dur - medio);
                uBit.display.image.setPixelValue(px, py, br);
            }
            orden++;
        }

        for (int i = 0; i < nA; i++) {
            if (parejaA[i] >= 0) continue;
            int br = 255 - (255 * f) / dur;
            if (br > 0) uBit.display.image.setPixelValue(A[i].x, A[i].y, br);
        }

        for (int j = 0; j < nB; j++) {
            if (usadoB[j]) continue;
            int t = f - dur;
            if (t < 0) continue;
            int br = (255 * t) / dur;
            uBit.display.image.setPixelValue(B[j].x, B[j].y, br);
        }

        uBit.sleep(16);
    }

    dibujarCaraDestino(destino);
}

// ---------------------------------------------------------------------------
// CORTINA eficiente
// ---------------------------------------------------------------------------
#define FRAMES_COL 21

static void moverColumnasOpt(int c1, int c2, bool cerrar)
{
    // Si las dos columnas son la misma (la central), el original escribia los
    // 5 pixeles DOS veces por frame. Con un solo paso.
    if (c1 == c2) {
        for (int s = 0; s <= FRAMES_COL; s++) {
            if (revisarSerial()) return;
            int br = cerrar ? (255 * (FRAMES_COL - s)) / FRAMES_COL
                            : (255 * s) / FRAMES_COL;
            for (int y = 0; y < 5; y++)
                uBit.display.image.setPixelValue(c1, y, br);
            uBit.sleep(16);
        }
        return;
    }
    for (int s = 0; s <= FRAMES_COL; s++) {
        if (revisarSerial()) return;
        int br = cerrar ? (255 * (FRAMES_COL - s)) / FRAMES_COL
                        : (255 * s) / FRAMES_COL;
        for (int y = 0; y < 5; y++) {
            uBit.display.image.setPixelValue(c1, y, br);
            uBit.display.image.setPixelValue(c2, y, br);
        }
        uBit.sleep(16);
    }
}

void transicionCortinaOpt(EmocionActual destino)
{
    moverColumnasOpt(0, 4, true);
    moverColumnasOpt(1, 3, true);
    moverColumnasOpt(2, 2, true);
    if (revisarSerial()) return;

    uBit.display.image.clear();
    dibujarCaraDestino(destino);

    moverColumnasOpt(2, 2, false);
    moverColumnasOpt(1, 3, false);
    moverColumnasOpt(0, 4, false);
    if (revisarSerial()) return;

    dibujarCaraDestino(destino);
}

// ---------------------------------------------------------------------------
// FUNDIDO eficiente
// ---------------------------------------------------------------------------
#define FRAMES_BR 62

void transicionFundidoOpt(EmocionActual destino)
{
    int ultimoQ = -1;

    for (int s = 0; s <= FRAMES_BR; s++) {
        if (revisarSerial()) return;
        int b = 90 - (90 * s) / FRAMES_BR;
        // Solo se llama a setBrightness si el QUANTUM del PWM cambia de
        // verdad. En el Fundido original hacen 126 llamadas para ~90 cambios
        // visibles, y cada una es una division entera por software.
        int q = BRILLO_QUANTUM(b);
        if (q != ultimoQ) {
            uBit.display.setBrightness(b);
            ultimoQ = q;
        }
        uBit.sleep(16);
    }
    if (revisarSerial()) return;

    uBit.display.image.clear();
    dibujarCaraDestino(destino);

    ultimoQ = -1;
    for (int s = 0; s <= FRAMES_BR; s++) {
        if (revisarSerial()) return;
        int b = (90 * s) / FRAMES_BR;
        int q = BRILLO_QUANTUM(b);
        if (q != ultimoQ) {
            uBit.display.setBrightness(b);
            ultimoQ = q;
        }
        uBit.sleep(16);
    }
    if (revisarSerial()) return;

    dibujarCaraDestino(destino);
}

// ---------------------------------------------------------------------------
// El director, con las SameNames para que el bench pueda A/B
// ---------------------------------------------------------------------------
void mostrarTransicionOpt(int idx, EmocionActual destino)
{
    uBit.serial.printf("TRANS:%d\n", idx);
    switch (idx) {
        case 0: transicionMorfosisOpt(destino); break;
        case 1: transicionCortinaOpt(destino); break;
        case 2: transicionFundidoOpt(destino); break;
        default: dibujarCaraDestino(destino); break;
    }
}
