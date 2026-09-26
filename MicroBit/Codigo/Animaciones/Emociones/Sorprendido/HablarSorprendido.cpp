/**
 * HablarSorprendido.cpp - La boca del SORPRENDIDO que HABLA ("OH OH")
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME (la boca): un tramo del tic por frame, el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ANTES esta boca era una cadena de fiber_sleep() y el bucle principal se
 * comia 705 ms SIN mirar el serial (medido: 690 ms de ventana sorda).
 *
 * Ahora animarBocaSorprendida() es una FUNCION DE FRAME: un frame (~16 ms) del
 * tic y vuelve. El estado vive en systemTime(), asi que el comando de la IA se
 * nota en el frame siguiente. Mismo patron que el resto.
 *
 * ── EL RITMAS: LA MAS RAPIDA DE LAS OCHO BOCAS ──────────────────────────
 * Con un tic de 235 ms son 4,3 silabas/s, mas rapida que el fastidio (4,0) y
 * que la alegria (3,6). Y tiene todo el sentido: la sorpresa es una
 * exclamacion ("OH OH OH"), y las exclamaciones son cortas y rapidas. El
 * contraste con su propia emocion en reposo es el que hace el efecto: la cara
 * de Sorprendido tiene el ciclo MAS LENTO de las ocho (2,6 s) y los ojos se
 * dilatan y quedan mirando fijo, pero en cuanto habla la boca hace gaspos
 * rapidos. Cuerpo quieto, boca acelerada.
 *
 * ── LA APERTURA ES MAS RAPIDA QUE EL CIERRE (y no es casualidad) ─────────
 * Abrir son 3 escalones de 15 ms (45 ms) y cerrar son 4 (60 ms): un 33% mas
 * rapido. Eso es el principio clasico de animacion, "anticipacion lenta, accion
 * rapida, recuperacion lenta": el momento de la accion "ocurre en una
 * fraccion de segundo, a menudo solo 1 o 2 frames". Y en el caso de un gaspo,
 * la inspiracion es el pico y la espiracion es la vuelta. Asi que el codigo
 * original ya lo tenia bien: la apertura rapida, el cierre con mas pasos.
 * No se toco.
 *
 * ── LA BOCA ES UN DIAMANTE QUE CRECE POR NIVELES ────────────────────────
 * El nivel 0 es solo el centro (2,3), que queda encendido siempre. Despues:
 *   nivel 1 = + arriba (2,2)
 *   nivel 2 = + los dos lados (1,3)(3,3)
 *   nivel 3 = + abajo (2,4)
 * Son los mismos 4 niveles que usa la emocion en reposo (Sorprendido.cpp), con
 * lo que la "o" de la sorpresa y el "wow" de su animacion comparten la
 * geometria.
 *
 * ── POR QUE LA BOCA NO NECESITA SU PROPIO revisarSerial() ───────────────
 * El bucle principal ya hace, en este orden:
 *     revisarSerial();
 *     atenderBotonesEscucha();
 *     if (modoHablar) animarBocaSorprendida();
 * Con la boca devolviendo cada 16 ms, el chequeo del puerto queda a 60 Hz.
 *
 * ── LA FIBRA DE LOS OJOS NO SE TOCA ──────────────────────────────────────
 * Ya esta bien: fiber_sleep() cede la CPU, rompe el loop en cuanto modoHablar
 * es false, se libera sola y no procesa comandos. La separacion de pixeles se
 * mantiene: la boca toca (2,2)(1,3)(3,3)(2,4), la fibra los ojos (1,1)(3,1).
 */
#include "HablarSorprendido.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Sorprendido.cpp)
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// El centro de la "o": NUNCA se apaga, lo dibuja dibujarCaraSorprendida()
static const uint8_t O_CENTRO[2] = {2, 3};

// El diamante que crece con el nivel
static const uint8_t O_DIAMANTE[4][2] = {
    {2, 2}, {1, 3}, {3, 3}, {2, 4}
};

// Que pixeles enciende cada nivel del diamante:
//   1 = arriba, 2 = arriba+lados, 3 = todo
#define NIVEL_ARRIBA 0x1
#define NIVEL_LADOS  0x2
#define NIVEL_ABAJO  0x4

static const uint8_t MASCARA[4] = { 0x0, NIVEL_ARRIBA, NIVEL_ARRIBA | NIVEL_LADOS,
                                     NIVEL_ARRIBA | NIVEL_LADOS | NIVEL_ABAJO };

// ---------------------------------------------------------------------------
// EL TIC COMO TABLA
//
// Un gaspo: el diamante abre en 3 escalones de 15 ms, queda abierto 70 ms,
// cierra en 4 escalones de 15 ms, y queda cerrado 60 ms. 45 + 70 + 60 + 60
// = 235 ms. La apertura es un 33% mas rapida que el cierre: la accion rapida,
// la recuperacion lenta.
// ---------------------------------------------------------------------------
enum Tramo
{
    T_ABRE = 0,   // 3 escalones: niveles 1, 2, 3
    T_QUIETO,     // abierto del todo
    T_CIERRA,     // 4 escalones: niveles 3, 2, 1, 0
    T_CERRADA     // solo el centro
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  tramo;
};

static const Segmento TIC[] = {
    {  0,  45, T_ABRE    },   // 45 ms
    { 45, 115, T_QUIETO  },   // 70 ms
    {115, 175, T_CIERRA  },   // 60 ms
    {175, 235, T_CERRADA },   // 60 ms
};
static const int NTRAMOS = sizeof(TIC) / sizeof(TIC[0]);
static const unsigned short TIC_MS = 235;

static unsigned long faseBase = 0;
static int ultimoNivel = -1;

// El nivel del diamante en este instante del tramo. La apertura va en 3 pasos
// y el cierre en 4: por eso el cierre dura mas para el mismo recorrido.
static int nivelEn(unsigned char tramo, float p)
{
    switch (tramo)
    {
        case T_ABRE:   return 1 + (int)(p * 3.0f);   // 1, 2, 3
        case T_QUIETO: return 3;
        case T_CIERRA: return 3 - (int)(p * 4.0f);   // 3, 2, 1, 0
        default:       return 0;
    }
}

// Dibuja el diamante en un nivel dado.
static void dibujarNivel(int nivel)
{
    uint8_t m = MASCARA[nivel];
    for (int i = 0; i < 4; i++)
        uBit.display.image.setPixelValue(O_DIAMANTE[i][0], O_DIAMANTE[i][1],
                                        (m & (1 << i)) ? 255 : 0);
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeo RARISIMO en paralelo (SIN CAMBIOS: ya cede la CPU, rompe el
// loop sola y no procesa comandos)
// ---------------------------------------------------------------------------

static void cerrarOjosSorprendidos(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = 255 - (255 * s) / steps;
        if (izq) uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], b);
        if (der) uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], b);
        fiber_sleep(delayMs);
    }
}

static void abrirOjosSorprendidos(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = (255 * s) / steps;
        if (izq) uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], b);
        if (der) uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], b);
        fiber_sleep(delayMs);
    }
}

static void fiberParpadeoSorprendido(void)
{
    while (modoHablar) {
        // ~75% ambos ojos, ~12% guino izquierdo, ~12% guino derecho
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 88)      { izq = false; }
        else if (r >= 75) { der = false; }

        cerrarOjosSorprendidos(izq, der, 2, 20);   // rapido: ~40 ms
        fiber_sleep(80);
        abrirOjosSorprendidos(izq, der, 2, 20);

        int espera = 4000 + uBit.random(4000);     // 4s a 8s: casi no parpadea
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }
    release_fiber();
}

// ---------------------------------------------------------------------------
// La cara base para hablar: ojos bien abiertos + boca "o"
// ---------------------------------------------------------------------------
static void dibujarCaraSorprendida()
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
    uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
    uBit.display.image.setPixelValue(O_CENTRO[0], O_CENTRO[1], 255);   // centro siempre
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con sorpresa activa): prepara la cara y lanza la fibra de parpadeo
void iniciarHablarSorprendido()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraSorprendida();
    uBit.serial.send("TALK-SUR\n");   // debug: confirmar el dispatch
    faseBase = uBit.systemTime();     // el tic arranca ahora
    ultimoNivel = -1;                // fuerza la primera escritura
    create_fiber(fiberParpadeoSorprendido);
}

// UN frame del gaspo. La llama el bucle principal ~60 veces por segundo
// mientras modoHablar siga activo.
void animarBocaSorprendida()
{
    unsigned long t = (uBit.systemTime() - faseBase) % TIC_MS;

    // Localiza el tramo (4 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &TIC[NTRAMOS - 1];
    for (int i = 0; i < NTRAMOS; i++) {
        if (t >= TIC[i].desde && t < TIC[i].hasta) { seg = &TIC[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    int nivel = nivelEn(seg->tramo, p);
    if (nivel != ultimoNivel) {
        dibujarNivel(nivel);
        ultimoNivel = nivel;
    }

    // fiber_sleep() y uBit.sleep() son la MISMA llamada (CodalDevice.cpp:30
    // -> fiber_sleep). Cede la CPU a la fibra del parpadeo, al replicador
    // del LED y al stack de BLE. 16 ms = el techo del display (60 Hz).
    fiber_sleep(16);
}
