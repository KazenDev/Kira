/**
 * HablarFastidio.cpp - La boca del FASTIDIO que HABLA (habla apretado)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME (la boca): un tramo del tic por frame, el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ANTES esta boca era una cadena de fiber_sleep() y el bucle principal se
 * comia 992 ms SIN mirar el serial (medido: 974 ms de ventana sorda), la
 * cuarta peor. Y pegaba mas de lo que parece, porque Principal.cpp le da
 * PRIORIDAD a TALK sobre la animacion de la emocion: mientras la IA habla,
 * esta boca es TODO lo que corre en el hilo principal.
 *
 * Ahora animarBocaFastidio() es una FUNCION DE FRAME: un frame (~16 ms) del
 * tic y vuelve. El estado vive en systemTime(), asi que el comando de la IA se
 * nota en el frame siguiente. Mismo patron que el resto.
 *
 * ── LA CARACTERISTICA DE ESTA BOCA: LA AMPLITUD, NO EL RITMO ─────────────
 * Los 3 LEDs pulsan entre 60 y 240: NUNCA se apagan del todo y NUNCA llegan a
 * 255. Eso es "hablar entre dientes, sin ganas", el "pfff". Y es lo unico que
 * distingue a esta boca de las demas, porque en el ritmo es la MAS RAPIDA:
 *
 *     Fastidio  248 ms -> 4,0 silabas/s    amplitud  60..240
 *     Alegria   280 ms -> 3,6 silabas/s    amplitud    0..255
 *     Triste    300 ms -> 3,3 silabas/s    amplitud    0..255
 *     Cansado   460 ms -> 2,2 silabas/s    amplitud    0..200
 *
 * O sea: el fastidio esta en la amplitud, no en el ritmo. Es coherente con que
 * el fastidio es una emocion de arousal BAJO (mientras que el cansancio, que si
 * es lento de verdad, es la de menor ritmo de todas).
 *
 * Sobre la amplitud, un matiz honesto: la literatura sobre mouths dice que lo
 * que mueve el arousal es la FORMA de la boca (abierta vs. cerrada, con
 * dientes visibles), no el brillo: "una boca abierta hace sentir mas valencia y
 * mas arousal" y "las expresiones de boca abierta capturan mas atencion
 * automatica temprana". En una matriz de 5x5 con 3 pixeles en fila NO hay
 * forma de abrir la boca, solo brillo. Asi que la amplitud es el unico eje
 * expresivo disponible, y aca se usa en rango estrecho a proposito. Que eso
 * se lea como "sin ganas" y no como "cara apagada" es cosa del ojo, no de la
 * tabla: por eso la amplitud minima es 60 y no 0. Con 0, la boca se cerraria
 * de verdad y se perderia el gesto de la "pfff".
 *
 * ── POR QUE LA BOCA NO NECESITA SU PROPIO revisarSerial() ───────────────
 * El bucle principal ya hace, en este orden:
 *     revisarSerial();
 *     atenderBotonesEscucha();
 *     if (modoHablar) animarBocaFastidio();
 * Con la boca devolviendo cada 16 ms, el chequeo del puerto queda a 60 Hz.
 *
 * ── LA FIBRA DE LOS OJOS NO SE TOCA ──────────────────────────────────────
 * Ya esta bien: fiber_sleep() cede la CPU, rompe el loop en cuanto modoHablar
 * es false, se libera sola y no procesa comandos. La separacion de pixeles se
 * mantiene: la boca toca la fila 3, la fibra los ojos (0,1)(4,1) y las cejas.
 */
#include "HablarFastidio.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Fastidio.cpp)
static const uint8_t CEJA_IZQ_IN[2] = {1, 0};   // borde interno (tiembla)
static const uint8_t CEJA_DER_IN[2] = {3, 0};

static const uint8_t OJO_IZQ[2] = {0, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Los 3 LEDs de la boca recta (los que pulsan al hablar)
static const uint8_t BOCA[3][2] = {
    {1, 3}, {2, 3}, {3, 3}
};

// La amplitud de la "pfff": nunca 0, nunca 255. Ver el encabezado.
#define BRILLO_MIN 60
#define BRILLO_MAX 240

// ---------------------------------------------------------------------------
// EL TIC COMO TABLA
//
// Un tic de habla fastidiada: sube tenue, queda, baja, queda. Al ser un
// oscilador libre (los 4 tics por pasada van pegados, sin pausa), el ciclo es
// exactamente un tic: 54 + 70 + 54 + 70 = 248 ms.
// ---------------------------------------------------------------------------
enum Tramo
{
    T_SUBE = 0,   // sube, sin ganas
    T_QUIETO,     // prendido al tope de la amplitud
    T_BAJA,       // baja
    T_APAGADO      // al piso de la amplitud, nunca del todo apagado
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  tramo;
};

static const Segmento TIC[] = {
    {  0,  54, T_SUBE    },   // 54 ms
    { 54, 124, T_QUIETO  },   // 70 ms
    {124, 178, T_BAJA    },   // 54 ms
    {178, 248, T_APAGADO },   // 70 ms
};
static const int NTRAMOS = sizeof(TIC) / sizeof(TIC[0]);
static const unsigned short TIC_MS = 248;

static unsigned long faseBase = 0;
static int ultimoGesto = -1;

// El brillo de los 3 LEDs en este instante del tramo.
static int valorEn(unsigned char tramo, float p)
{
    switch (tramo)
    {
        case T_SUBE:    return BRILLO_MIN + (int)((BRILLO_MAX - BRILLO_MIN) * p / 0.2177f);
        case T_QUIETO:  return BRILLO_MAX;
        case T_BAJA:    return BRILLO_MAX - (int)((BRILLO_MAX - BRILLO_MIN) * (p - 0.5f) / 0.2177f);
        default:        return BRILLO_MIN;   // nunca 0: la boca nunca se cierra
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeos DUROS de ojos en paralelo (SIN CAMBIOS: ya cede la CPU,
// rompe el loop sola y no procesa comandos)
// 80% ambos ojos, 10% guino izq, 10% guino der.
// ---------------------------------------------------------------------------

static void fiberOjosFastidio(void)
{
    while (modoHablar) {
        int r = uBit.random(100);
        bool cierroIzq = true, cierroDer = true;
        if (r >= 90)      { cierroIzq = false; }
        else if (r >= 80) { cierroDer = false; }

        // Cierran de golpe, un instante, abren
        if (cierroIzq) uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 0);
        if (cierroDer) uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 0);
        fiber_sleep(140);
        uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
        uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);

        // A veces (1/3) la ceja tiembla junto al parpadeo
        if (uBit.random(3) == 0) {
            uBit.display.image.setPixelValue(CEJA_IZQ_IN[0], CEJA_IZQ_IN[1], 80);
            uBit.display.image.setPixelValue(CEJA_DER_IN[0], CEJA_DER_IN[1], 80);
            fiber_sleep(60);
            uBit.display.image.setPixelValue(CEJA_IZQ_IN[0], CEJA_IZQ_IN[1], 255);
            uBit.display.image.setPixelValue(CEJA_DER_IN[0], CEJA_DER_IN[1], 255);
        }

        int espera = 1500 + uBit.random(2500);          // 1.5s a 4s
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }
    release_fiber();
}

// ---------------------------------------------------------------------------
// La cara base para hablar: la misma cara fastidiada
// ---------------------------------------------------------------------------
static void dibujarCaraFastidio()
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(CEJA_IZQ_IN[0], CEJA_IZQ_IN[1], 255);
    uBit.display.image.setPixelValue(CEJA_DER_IN[0], CEJA_DER_IN[1], 255);
    uBit.display.image.setPixelValue(0, 0, 255);
    uBit.display.image.setPixelValue(4, 0, 255);
    uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
    uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 3; i++)
        uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con fastidio activo): prepara la cara y lanza la fibra de ojos
void iniciarHablarFastidio()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraFastidio();
    uBit.serial.send("TALK-FAS\n");   // debug: confirmar el dispatch
    faseBase = uBit.systemTime();     // el tic arranca ahora
    ultimoGesto = -1;                // fuerza la primera escritura
    create_fiber(fiberOjosFastidio);
}

// UN frame de la boca apretada. La llama el bucle principal ~60 veces por
// segundo mientras modoHablar siga activo.
void animarBocaFastidio()
{
    unsigned long t = (uBit.systemTime() - faseBase) % TIC_MS;

    // Localiza el tramo (4 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &TIC[NTRAMOS - 1];
    for (int i = 0; i < NTRAMOS; i++) {
        if (t >= TIC[i].desde && t < TIC[i].hasta) { seg = &TIC[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    int v = valorEn(seg->tramo, p);
    if (v != ultimoGesto) {
        for (int i = 0; i < 3; i++)
            uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], (uint8_t)v);
        ultimoGesto = v;
    }

    // fiber_sleep() y uBit.sleep() son la MISMA llamada (CodalDevice.cpp:30
    // -> fiber_sleep). Cede la CPU a la fibra de los ojos, al replicador del
    // LED y al stack de BLE. 16 ms = el techo del display (60 Hz).
    fiber_sleep(16);
}
