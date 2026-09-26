/**
 * HablarEnojado.cpp - La boca del ENOJADO que HABLA (el gruñido)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME (la boca): un tramo del tic por frame, el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ANTES esta boca era una cadena de fiber_sleep() y el bucle principal se
 * comia 680 ms SIN mirar el serial (medido: 665 ms de ventana sorda), la mas
 * corta de las ocho.
 *
 * Ahora animarBocaEnojada() es una FUNCION DE FRAME: un frame (~16 ms) del tic
 * y vuelve. El estado vive en systemTime(), asi que el comando de la IA se nota
 * en el frame siguiente. Ultima de las ocho bocas en migrarse.
 *
 * ── EL RITMAS: EL MAS RAPIDO DE LAS OCHO BOCAS ──────────────────────────
 * Con un tic de 170 ms son 5,9 silabas/s, el techo del rango normal de habla
 * (medido entre 3,3 y 5,9). Y encaja con el principio de arousal: "las
 * emociones con arousal alto, como el enfado y la alegria, se relacionan con
 * una velocidad de habla mas rapida", y el estudio lo confirma: "la velocidad
 * en la tristeza fue significativamente mas lenta que en el enfado y la
 * alegria".
 *
 * Un matiz honesto: esa convencion NO es unanime. Hay un estudio de habla
 * espontanea en portugues que encontro el enfado MAS LENTO que el neutral, y
 * otro hallo que en habla clara reducir el ritmo aumenta el juicio de enfado.
 * Los dos acknowledge que contradicen la mayoria de la literatura. Asi que
 * esto es "consistente con la convencion dominante", no "demostrado".
 *
 * Ademas, con 5,9 syl/s esta en el TECHO de lo plausible: no tiene para donde
 * subir sin caer en la caricatura. Si quisieras mas enfado, el camino no seria
 * un tic mas corto sino MAS AMPLITUD o una pausa antes de la palabra.
 *
 * ── LA BOCA ES UN SOLO PIXEL: la mandibula ──────────────────────────────
 * (2,4) es el hueco de los dientes. Se abre y se cierra = la mandibula que
 * aprieta. Los 6 pixeles de la boca quedan siempre encendidos (dientes
 * apretados), igual que en el original.
 *
 * OJO al testear esto: (2,4) lo escribe TAMBIEN la emocion en reposo, que
 * tiene su propia "mandibula aprieta" pulsando el mismo pixel. Asi que por
 * serial no se puede separar un resto de TALK del movimiento de la emocion
 * (ver prueba_talk.py, caso ENOJADO).
 *
 * ── POR QUE LA BOCA NO NECESITA SU PROPIO revisarSerial() ───────────────
 * El bucle principal ya hace, en este orden:
 *     revisarSerial();
 *     atenderBotonesEscucha();
 *     if (modoHablar) animarBocaEnojada();
 * Con la boca devolviendo cada 16 ms, el chequeo del puerto queda a 60 Hz.
 *
 * ── LA FIBRA DE LOS OJOS NO SE TOCA ──────────────────────────────────────
 * Ya esta bien: fiber_sleep() cede la CPU, rompe el loop en cuanto modoHablar
 * es false, se libera sola y no procesa comandos. La separacion de pixeles se
 * mantiene: la boca toca (2,4), la fibra los ojos y las cejas.
 */
#include "HablarEnojado.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Enojado.cpp)
static const uint8_t CEJA_IZQ[2] = {0, 0};
static const uint8_t CEJA_IZQ_F[2] = {1, 0};   // interior: se frunce al hablar
static const uint8_t CEJA_DER[2] = {4, 0};
static const uint8_t CEJA_DER_F[2] = {3, 0};

static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// La mandibula: el hueco de los dientes, (2,4)
static const uint8_t MANDIBULA[2] = {2, 4};

// Los 6 pixeles de la boca con dientes: SIEMPRE encendidos mientras habla
static const uint8_t BOCA[6][2] = {
    {1,3}, {2,3}, {3,3}, {0,4}, {1,4}, {3,4}
};

// ---------------------------------------------------------------------------
// EL TIC COMO TABLA
//
// Un gruñido: la mandibula baja de a poco, queda, sube y queda.
// 30 + 60 + 30 + 50 = 170 ms. OJO: abrir y cerrar duran LO MISMO (30 ms), al
// reves de Sorprendido, donde la apertura era un 33% mas rapida. Eso tambien
// tiene sentido: el gruñido es simetrico, no un gaspo.
// ---------------------------------------------------------------------------
enum Tramo
{
    T_ABRE = 0,
    T_ABIERTA,
    T_CIERRA,
    T_CERRADA
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  tramo;
};

static const Segmento TIC[] = {
    {  0,  30, T_ABRE   },   // 30 ms
    { 30,  90, T_ABIERTA },   // 60 ms
    { 90, 120, T_CIERRA  },   // 30 ms
    {120, 170, T_CERRADA },   // 50 ms
};
static const int NTRAMOS = sizeof(TIC) / sizeof(TIC[0]);
static const unsigned short TIC_MS = 170;

static unsigned long faseBase = 0;
static int ultimoGrunido = -1;

// El brillo de la mandibula en este instante del tramo.
static int valorEn(unsigned char tramo, float p)
{
    switch (tramo)
    {
        case T_ABRE:    return (int)(255 * p / 0.1765f);
        case T_ABIERTA:  return 255;
        case T_CIERRA:  return 255 - (int)(255 * p / 0.1765f);
        default:         return 0;
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeos en paralelo (SIN CAMBIOS: ya cede la CPU, rompe el loop
// sola y no procesa comandos)
// ---------------------------------------------------------------------------

static void cerrarOjosEnojados(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = 255 - (255 * s) / steps;
        if (izq) uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], b);
        if (der) uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], b);
        fiber_sleep(delayMs);
    }
}

static void abrirOjosEnojados(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = (255 * s) / steps;
        if (izq) uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], b);
        if (der) uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], b);
        fiber_sleep(delayMs);
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeo BRUSCO e impredecible (SIN CAMBIOS: ya cede la CPU, rompe
// el loop sola y no procesa comandos).
// El enojado casi no parpadea: esperas cortas de 0.8 a 2.5 s.
// ---------------------------------------------------------------------------

static void fiberParpadeoEnojado(void)
{
    while (modoHablar) {
        // ~80% ambos, ~10% guino izquierdo, ~10% guino derecho
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 90)      { izq = false; }
        else if (r >= 80) { der = false; }

        cerrarOjosEnojados(izq, der, 2, 20);   // ~40 ms
        fiber_sleep(70);                         // cerrado ~70 ms
        abrirOjosEnojados(izq, der, 2, 20);    // ~40 ms

        int espera = 800 + uBit.random(1700);    // 0.8 s a 2.5 s
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }
    release_fiber();
}

// ---------------------------------------------------------------------------
// La cara base para hablar: cejas fruncidas + ojos + boca con dientes
// ---------------------------------------------------------------------------
static void dibujarCaraEnojada()
{
    uBit.display.setBrightness(95);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(CEJA_IZQ[0], CEJA_IZQ[1], 255);
    uBit.display.image.setPixelValue(CEJA_DER[0], CEJA_DER[1], 255);
    uBit.display.image.setPixelValue(CEJA_IZQ_F[0], CEJA_IZQ_F[1], 255);
    uBit.display.image.setPixelValue(CEJA_DER_F[0], CEJA_DER_F[1], 255);
    uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
    uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 6; i++)
        uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con enojo activo): prepara la cara y lanza la fibra de parpadeo
void iniciarHablarEnojado()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraEnojada();
    uBit.serial.send("TALK-ANG\n");   // debug: confirmar el dispatch
    faseBase = uBit.systemTime();     // el tic arranca ahora
    ultimoGrunido = -1;              // fuerza la primera escritura
    create_fiber(fiberParpadeoEnojado);
}

// UN frame del gruñido. La llama el bucle principal ~60 veces por segundo
// mientras modoHablar siga activo.
void animarBocaEnojada()
{
    unsigned long t = (uBit.systemTime() - faseBase) % TIC_MS;

    // Localiza el tramo (4 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &TIC[NTRAMOS - 1];
    for (int i = 0; i < NTRAMOS; i++) {
        if (t >= TIC[i].desde && t < TIC[i].hasta) { seg = &TIC[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    int v = valorEn(seg->tramo, p);
    if (v != ultimoGrunido) {
        uBit.display.image.setPixelValue(MANDIBULA[0], MANDIBULA[1], (uint8_t)v);
        ultimoGrunido = v;
    }

    // fiber_sleep() y uBit.sleep() son la MISMA llamada (CodalDevice.cpp:30
    // -> fiber_sleep). Cede la CPU a la fibra de los ojos, al replicador del
    // LED y al stack de BLE. 16 ms = el techo del display (60 Hz).
    fiber_sleep(16);
}
