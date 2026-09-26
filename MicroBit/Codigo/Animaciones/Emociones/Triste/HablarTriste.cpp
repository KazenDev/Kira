/**
 * HablarTriste.cpp - La boca de la TRISTEZA que HABLA (lip-sync triste)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME (la boca): un tramo del tic por frame, el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ANTES esta boca era una cadena de fiber_sleep() y el bucle principal se
 * comia 900 ms SIN mirar el serial (medido: 880 ms de ventana sorda). Y eso
 * pega mas de lo que parece, porque Principal.cpp le da PRIORIDAD a TALK
 * sobre la animacion de la emocion: mientras la IA habla, esta boca es TODO
 * lo que corre en el hilo principal.
 *
 * Ahora animarBocaTriste() es una FUNCION DE FRAME: muestra UN frame (~16 ms)
 * del tic y vuelve. El estado vive en systemTime(), asi que el comando de la
 * IA se nota en el frame siguiente. Mismo patron que las 8 emociones y que la
 * boca de Alegria.
 *
 * ── POR QUE LA BOCA NO NECESITA SU PROPIO revisarSerial() ───────────────
 * El bucle principal ya hace, en este orden:
 *
 *     revisarSerial();
 *     atenderBotonesEscucha();
 *     if (modoHablar) animarBocaTriste();
 *
 * Con la boca devolviendo cada 16 ms, el chequeo del puerto queda a 60 Hz.
 * Un checkpoint EXTRA adentro seria trabajo redundante.
 *
 * ── POR QUE LA BOCA ABRE HACIA ABAJO (y no se toca) ─────────────────────
 * Es la mandibula la que dirige el movimiento: "la mandibula baja en las
 * vocales abiertas y en las sílabas acentuadas", y "el movimiento real de la
 * mandibula tiene peso y seguimiento". En una matriz de 5x5 el unico modo de
 * que la mandibula baje es que el pixel central del frown deje su fila y
 * aparezca el de abajo: eso es (2,3) apagandose mientras (2,4) se enciende.
 * Y el ritmo lento con hold es la "curva musical": keyear la mandibula en
 * cada tic rapido es justamente la "boca de maquina de escribir" que las
 * guias markean como amateur.
 *
 * OJO: (2,4) NO es parte de la cara en reposo de Triste (su BOCA es
 * (0,4)(4,4)(1,3)(2,3)(3,3)). Es un pixel que solo existe mientras habla.
 * El tic lo deja siempre en 0 al cerrarse, y si TALK se corta a mitad, el
 * caraIntacta() de Triste.cpp lo detecta y lo repinta en el frame siguiente.
 *
 * ── LA FIBRA DE LOS OJOS NO SE TOCA ──────────────────────────────────────
 * Ya esta bien: usa fiber_sleep() (cede la CPU), rompe el loop en cuanto
 * modoHablar es false, se libera sola, y no procesa comandos. La separacion
 * de pixeles se mantiene: la boca toca (2,3) y (2,4), la fibra los ojos
 * (1,1) y (3,1).
 */
#include "HablarTriste.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Triste.cpp)
//
// La boca triste cerrada (frown): esquinas (0,4)(4,4) + medio (1,3)(2,3)(3,3)
static const uint8_t BOCA[5][2] = {
    {0,4}, {4,4}, {1,3}, {2,3}, {3,3}
};

// La mandibula: el centro del frown (2,3) y el pixel de abajo (2,4).
static const uint8_t LABIO[2] = {2, 3};
static const uint8_t BARBILLA[2] = {2, 4};

// Los ojos de la fibra (fila 1). La boca solo toca la columna 2 de las filas
// 3 y 4: no se pisan.
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// ---------------------------------------------------------------------------
// EL TIC COMO TABLA
//
// Un "wah" triste: la mandibula baja (el labio se apaga y aparece el de
// abajo), queda abierta un momento, y vuelve al frown.
// 60 + 90 + 60 + 90 = 300 ms, los mismos tiempos de siempre.
// ---------------------------------------------------------------------------
enum Tramo
{
    T_BAJA = 0,   // la mandibula cae
    T_ABIERTA,    // abierta un momento
    T_SUBE,       // vuelve al frown
    T_CERRADA     // pausa en el frown
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  tramo;
};

static const Segmento TIC[] = {
    {  0,  60, T_BAJA    },
    { 60, 150, T_ABIERTA },
    {150, 210, T_SUBE    },
    {210, 300, T_CERRADA },
};
static const int NTRAMOS = sizeof(TIC) / sizeof(TIC[0]);
static const unsigned short TIC_MS = 300;

static unsigned long faseBase = 0;
static int ultimoLabio = -1;
static int ultimoBarbilla = -1;

// Los dos valores en este instante del tramo. Son complementarios: con la
// mandibula abajo el labio esta apagado y la barbilla prendida, y al revés.
static void valoresTriste(unsigned char tramo, float p, int &labio, int &barbilla)
{
    switch (tramo)
    {
        case T_BAJA:     // 60 ms
            labio    = 255 - (int)(255 * p / 0.2f);
            barbilla = (int)(255 * p / 0.2f);
            break;
        case T_ABIERTA:  // 90 ms
            labio = 0; barbilla = 255;
            break;
        case T_SUBE:     // 60 ms
            labio    = (int)(255 * (p - 0.5f) / 0.2f);
            barbilla = 255 - (int)(255 * (p - 0.5f) / 0.2f);
            break;
        default:         // 90 ms
            labio = 255; barbilla = 0;
            break;
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeo PESADO e impredecible mientras modoHablar siga activo
// (SIN CAMBIOS: ya cede la CPU, rompe el loop sola y no procesa comandos)
// ---------------------------------------------------------------------------

static void cerrarOjosTristes(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = 255 - (255 * s) / steps;
        if (izq) uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], b);
        if (der) uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], b);
        fiber_sleep(delayMs);
    }
}

static void abrirOjosTristes(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int b = (255 * s) / steps;
        if (izq) uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], b);
        if (der) uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], b);
        fiber_sleep(delayMs);
    }
}

static void fiberParpadeoTriste(void)
{
    while (modoHablar) {
        // ~75% ambos ojos, ~12.5% guino izquierdo, ~12.5% guino derecho
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 88)      { izq = false; }
        else if (r >= 75) { der = false; }

        // Parpadeo PESADO: cerrar ~200ms, cerrado ~200ms, abrir ~200ms
        cerrarOjosTristes(izq, der, 5, 40);
        fiber_sleep(200);
        abrirOjosTristes(izq, der, 5, 40);

        // Espera ALEATORIA: 2s a 6s (triste = parpadea menos)
        int espera = 2000 + uBit.random(4000);
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }
    release_fiber();
}

// ---------------------------------------------------------------------------
// La cara base para hablar: frown completo + ojos
// ---------------------------------------------------------------------------
static void dibujarCaraTriste()
{
    uBit.display.setBrightness(80);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
    uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 5; i++)
        uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con tristeza activa): prepara la cara y lanza la fibra de ojos
void iniciarHablarTriste()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraTriste();
    uBit.serial.send("TALK-SAD\n");   // debug: confirmar el dispatch
    faseBase = uBit.systemTime();     // el tic arranca ahora
    ultimoLabio = ultimoBarbilla = -1;   // fuerza la primera escritura
    create_fiber(fiberParpadeoTriste);
}

// Una pasada del movimiento de dientes hablando (la llama el bucle principal)
void animarBocaTriste()
{
    unsigned long t = (uBit.systemTime() - faseBase) % TIC_MS;

    // Localiza el tramo (4 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &TIC[NTRAMOS - 1];
    for (int i = 0; i < NTRAMOS; i++) {
        if (t >= TIC[i].desde && t < TIC[i].hasta) { seg = &TIC[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    int labio, barbilla;
    valoresTriste(seg->tramo, p, labio, barbilla);

    if (labio != ultimoLabio) {
        uBit.display.image.setPixelValue(LABIO[0], LABIO[1], (uint8_t)labio);
        ultimoLabio = labio;
    }
    if (barbilla != ultimoBarbilla) {
        uBit.display.image.setPixelValue(BARBILLA[0], BARBILLA[1], (uint8_t)barbilla);
        ultimoBarbilla = barbilla;
    }

    // fiber_sleep() y uBit.sleep() son la MISMA llamada (CodalDevice.cpp:30
    // -> fiber_sleep). Cede la CPU a la fibra de los ojos, al replicador del
    // LED y al stack de BLE. 16 ms = el techo del display (60 Hz).
    fiber_sleep(16);
}
