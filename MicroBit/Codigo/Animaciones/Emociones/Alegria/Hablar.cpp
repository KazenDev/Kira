/**
 * Hablar.cpp - La boca que HABLA (lip-sync sutil) ASINCRONO con FIBERS
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME (la boca): un tic por frame, el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ANTES esta boca era una cadena de fiber_sleep() y el bucle principal se
 * comia 840 ms SIN mirar el serial (medido: 820 ms de ventana sorda). Y eso
 * pega mas de lo que parece, porque Principal.cpp le da PRIORIDAD a TALK
 * sobre la animacion de la emocion: mientras la IA habla, esto es TODO lo
 * que corre en el hilo principal. O sea que la ventana sorda mas larga del
 * proyecto no era una emocion en reposo, era la boca.
 *
 * Ahora animarBocaHablando() es una FUNCION DE FRAME: muestra UN frame
 * (~16 ms) del tic y vuelve. El estado vive en systemTime(), asi que el
 * comando de la IA se nota en el frame siguiente. Mismo patron que las 8
 * emociones de reposo y que el metronomo.
 *
 * ── POR QUE LA BOCA NO NECESITA SU PROPIO revisarSerial() ───────────────
 * El bucle principal ya hace, en este orden:
 *
 *     revisarSerial();
 *     atenderBotonesEscucha();
 *     if (modoHablar) animarBocaHablando();
 *
 * Con la boca devolviendo cada 16 ms, el chequeo del puerto queda a 60 Hz.
 * No hace falta un checkpoint EXTRA adentro: seria trabajo redundante. Si
 * algun dia esta funcion se llamara desde otro lado, hay que acordarse de
 * revisar el puerto (o llamarla solo desde Principal, que es lo unico que
 * hace hoy).
 *
 * ── LA FIBRA DE LOS OJOS NO SE TOCA ──────────────────────────────────────
 * La fibra que parpadea los ojos ya esta bien: usa fiber_sleep() (que cede la
 * CPU), rompe el loop en cuanto modoHablar es false, y se libera sola. Y no
 * procesa comandos, asi que no es un cuello de botella. Ademas la
 * separacion de pixeles se respeta: la boca toca la FILA 3, la fibra la
 * FILA 1. No se pisan.
 *
 * Lo mismo aplica a uBit.random() en la fibra: aca el estado being a fiber
 * (un loop con su propio ritmo) es lo NORMAL, no hace falta que sea funcion
 * del reloj. Lo que si necesitaba ser funcion del reloj era la boca, que
 * corre en el hilo principal.
 */
#include "Hablar.h"
#include "Alegria.h"

// Estado global (definido aqui, declarado extern en Hablar.h)
bool modoHablar = false;

// ---------------------------------------------------------------------------
// Posiciones (mismas que Alegria.cpp)
//
// Esquinas de la sonrisa + curva inferior = las MEJILLAS (se quedan fijas
static const uint8_t MEJILLA[5][2] = {
    {0,3}, {4,3}, {1,4}, {2,4}, {3,4}
};

// La linea de DIENTES: solo parpadea esto al hablar
static const uint8_t DIENTES[3][2] = {
    {1,3}, {2,3}, {3,3}
};

// ---------------------------------------------------------------------------
// EL TIC COMO TABLA
//
// Un tic de habla: los dientes aparecen, se quedan, desaparecen, y hay una
// pausa. 60 + 80 + 60 + 80 = 280 ms, los mismos tiempos de siempre.
// ---------------------------------------------------------------------------
enum Tramo
{
    T_SUBE = 0,   // fade in de los dientes
    T_QUIETO,     // dientes a la vista
    T_BAJA,       // fade out
    T_APAGADO     // pausa sin dientes
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  tramo;
};

static const Segmento TIC[] = {
    {  0,  60, T_SUBE    },
    { 60, 140, T_QUIETO  },
    {140, 200, T_BAJA    },
    {200, 280, T_APAGADO },
};
static const int NTRAMOS = sizeof(TIC) / sizeof(TIC[0]);
static const unsigned short TIC_MS = 280;

// La fase arranca cuando la IA manda TALK.
static unsigned long faseBase = 0;
static int ultimoDientes = -1;

// Los ojos de la fibra (fila 1). La boca solo toca la fila 3: no se pisan.
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// El valor de los dientes en este instante del tramo.
static int dientesEn(unsigned char tramo, float p)
{
    switch (tramo)
    {
        case T_SUBE:    return (int)(255 * p / 0.2143f);            // 60 ms
        case T_QUIETO:  return 255;                                  // 80 ms
        case T_BAJA:    return 255 - (int)(255 * (p - 0.5f) / 0.2143f);
        default:        return 0;                                   // 80 ms
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeo IMPREDECIBLE de los ojos mientras modoHablar siga activo
// (SIN CAMBIOS: ya cede la CPU, rompe el loop sola y no procesa comandos)
// ---------------------------------------------------------------------------

static void cerrarOjos(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = 255 - (255 * s) / steps;
        if (izq) uBit.display.image.setPixelValue(1, 1, b);
        if (der) uBit.display.image.setPixelValue(3, 1, b);
        fiber_sleep(delayMs);
    }
}

static void abrirOjos(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = (255 * s) / steps;
        if (izq) uBit.display.image.setPixelValue(1, 1, b);
        if (der) uBit.display.image.setPixelValue(3, 1, b);
        fiber_sleep(delayMs);
    }
}

static void fiberParpadeo(void)
{
    while (modoHablar) {
        // ~75% ambos ojos, ~12.5% guino izquierdo, ~12.5% guino derecho
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 88)      { izq = false; }
        else if (r >= 75) { der = false; }

        cerrarOjos(izq, der, 4, 30);
        fiber_sleep(100);
        abrirOjos(izq, der, 4, 30);

        int espera = 1500 + uBit.random(3500);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }
    release_fiber();
}

// ---------------------------------------------------------------------------
// Dibuja la cara base para hablar: sonrisa COMPLETA fija + ojos
// ---------------------------------------------------------------------------
static void dibujarCara()
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(1, 1, 255);
    uBit.display.image.setPixelValue(3, 1, 255);
    for (int i = 0; i < 5; i++)
        uBit.display.image.setPixelValue(MEJILLA[i][0], MEJILLA[i][1], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK: prepara la cara y lanza la fibra de parpadeo impredecible
void iniciarHablar()
{
    // Si ya estamos hablando, NO crear otra fibra (si no, dos fibras
    // competirian parpadeando los mismos ojos para siempre)
    if (modoHablar) return;

    modoHablar = true;
    dibujarCara();
    faseBase = uBit.systemTime();   // el tic arranca ahora
    ultimoDientes = -1;             // fuerza la primera escritura
    create_fiber(fiberParpadeo);    // los ojos corren EN PARALELO
}

// Otra emocion: apaga la bandera; la fibra muere en su siguiente ciclo
void detenerHablar()
{
    modoHablar = false;
}

// UN frame de "hablar": la linea de dientes, y vuelve. La llama el bucle
// principal ~60 veces por segundo mientras modoHablar siga activo.
void animarBocaHablando()
{
    unsigned long t = (uBit.systemTime() - faseBase) % TIC_MS;

    // Localiza el tramo (4 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &TIC[NTRAMOS - 1];
    for (int i = 0; i < NTRAMOS; i++) {
        if (t >= TIC[i].desde && t < TIC[i].hasta) { seg = &TIC[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    int v = dientesEn(seg->tramo, p);
    if (v != ultimoDientes) {
        for (int i = 0; i < 3; i++)
            uBit.display.image.setPixelValue(DIENTES[i][0], DIENTES[i][1], (uint8_t)v);
        ultimoDientes = v;
    }

    // fiber_sleep() y uBit.sleep() son la MISMA llamada (CodalDevice.cpp:30
    // -> fiber_sleep). Cede la CPU a la fibra de los ojos, al replicador del
    // LED y al stack de BLE. 16 ms = el techo del display (60 Hz).
    fiber_sleep(16);
}
