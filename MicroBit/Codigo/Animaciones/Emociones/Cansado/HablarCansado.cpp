/**
 * HablarCansado.cpp - La boca del CANSADO que HABLA (lento, con sueno)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME (la boca): un tramo del tic por frame, el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ANTES esta boca era una cadena de fiber_sleep() y el bucle principal se
 * comia 1380 ms SIN mirar el serial: la ventana sorda mas larga del proyecto
 * (medido 1350 ms). Y pegaba mas de lo que parece, porque Principal.cpp le
 * da PRIORIDAD a TALK sobre la animacion de la emocion: mientras la IA habla,
 * esta boca es TODO lo que corre en el hilo principal.
 *
 * Ahora animarBocaCansado() es una FUNCION DE FRAME: un frame (~16 ms) del
 * tic y vuelve. El estado vive en systemTime(), asi que el comando de la IA
 * se nota en el frame siguiente. Mismo patron que las 8 emociones y que las
 * bocas de Alegria y Triste.
 *
 * ── POR QUE ESTA BOCA ES LA MAS LENTA (y eso es lo correcto) ───────────
 * El tic dura 460 ms, el mas largo de las ocho bocas, contra 280 ms del de
 * Alegria. Traducido a ritmo de habla:
 *
 *     Alegria  280 ms -> 3,6 silabas/s   (ritmo normal)
 *     Triste   300 ms -> 3,3 silabas/s   (el piso del rango)
 *     Cansado  460 ms -> 2,2 silabas/s   (claramente lento)
 *
 * La referencia: "una velocidad de habla tipica del ingles es de 4 silabas por
 * segundo", con un rango medido de 3,3 a 5,9. O sea que el "siii... ya
 * voy..." de esta boca esta por DEBAJO del rango normal, que es exactamente lo
 * que tiene que pasar. Ademas el brillo tope es 200 en vez de 255, o sea que
 * ademas de lenta es tenue: las dos cosas van en el mismo sentido.
 *
 * ── PENDIENTE DE TU OJO (esta a una linea) ──────────────────────────────
 * La literatura del habla lenta dice que no es solo "mas lento": es "articular
 * lentamente con un MAYOR NUMERO DE PAUSAS e hiperarticulacion". Esta boca
 * tiene la primera parte (las silabas lentas) pero no las otras dos: los tres
 * tics van PEGADOS, sin una pausa entre frase y frase, y con amplitud
 * REDUCIDA en vez de hiperarticulada. O sea que se lee como "habla lento de
 * corrido", no como "habla con pausas de sueno".
 *
 * PAUSA_ENTRE_TICS_MS esta en 0, o sea el comportamiento de siempre. Si
 * queres que la figura se tome un respiro entre tics, subilo a ~250: el ciclo
 * pasa de 1380 a 1880 ms, el ritmo baja a 1,6 silabas/s, y aparecen las pausas
 * que la fuente dice que son la mitad del efecto. No se cambio por decision
 * propia: es diseno del personaje.
 *
 * ── POR QUE LA BOCA NO NECESITA SU PROPIO revisarSerial() ───────────────
 * El bucle principal ya hace, en este orden:
 *     revisarSerial();
 *     atenderBotonesEscucha();
 *     if (modoHablar) animarBocaCansado();
 * Con la boca devolviendo cada 16 ms, el chequeo del puerto queda a 60 Hz.
 * Un checkpoint EXTRA adentro seria trabajo redundante.
 *
 * ── LA FIBRA DE LOS OJOS NO SE TOCA ──────────────────────────────────────
 * Ya esta bien: fiber_sleep() cede la CPU, rompe el loop en cuanto modoHablar
 * es false, se libera sola y no procesa comandos. La separacion de pixeles se
 * mantiene: la boca toca la fila 3, la fibra los ojos (1,1) y (3,1).
 */
#include "HablarCansado.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Cansado.cpp)
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Los 3 LEDs de la boca chica (los que hablan lento)
static const uint8_t BOCA[3][2] = {
    {1, 3}, {2, 3}, {3, 3}
};

// El brillo tope de la boca de Cansado: 200, no 255. Es la version "apagada"
// con sueno, y va en el mismo sentido que el tic lento.
#define BRILLO_MAX 200

// Ver la nota del encabezado: 0 = tics pegados (como esta siempre).
#define PAUSA_ENTRE_TICS_MS 0
#define TICS_POR_PASADA 3

// ---------------------------------------------------------------------------
// EL TIC COMO TABLA
//
// Un tic de habla cansada: los dientes suben TENUES (a 200, no a 255), quedan,
// bajan y hay una pausa. 90 + 140 + 90 + 140 = 460 ms.
// ---------------------------------------------------------------------------
enum Tramo
{
    T_SUBE = 0,   // sube, tenue
    T_QUIETO,     // prendido
    T_BAJA,       // baja
    T_APAGADO     // apagado, la pausa
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  tramo;
};

static const Segmento TIC[] = {
    {  0,  90, T_SUBE    },   // 90 ms
    { 90, 230, T_QUIETO  },   // 140 ms
    {230, 320, T_BAJA    },   // 90 ms
    {320, 460, T_APAGADO },   // 140 ms
};
static const int NTRAMOS = sizeof(TIC) / sizeof(TIC[0]);
static const unsigned short TIC_MS = 460;

// El ciclo completo son los tics pegados (o con pausa, si se cambia la
// constante de arriba). Con PAUSA_ENTRE_TICS_MS = 0 da 1380 ms, que es
// exactamente lo que duraba la version con 3 tics seguido.
#define CICLO_MS  (TIC_MS * TICS_POR_PASADA + PAUSA_ENTRE_TICS_MS * (TICS_POR_PASADA - 1))

static unsigned long faseBase = 0;
static int ultimoBoca = -1;

// El brillo de los dientes en este instante del tramo.
static int valorEn(unsigned char tramo, float p)
{
    switch (tramo)
    {
        case T_SUBE:    return (int)(BRILLO_MAX * p / 0.1957f);        // 90 ms
        case T_QUIETO:  return BRILLO_MAX;                             // 140 ms
        case T_BAJA:    return BRILLO_MAX - (int)(BRILLO_MAX * (p - 0.5f) / 0.1957f);
        default:        return 0;                                      // 140 ms
    }
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeos PESADOS de ojos en paralelo (SIN CAMBIOS: ya cede la CPU,
// rompe el loop sola, no procesa comandos)
// ---------------------------------------------------------------------------

static void fiberOjosCansado(void)
{
    while (modoHablar) {
        for (int s = 0; s <= 5 && modoHablar; s++) {          // cerrar lento
            int v = 255 - 255 * s / 5;
            uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], v);
            uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], v);
            fiber_sleep(45);
        }
        fiber_sleep(350);                                       // cerrados un buen rato

        for (int s = 5; s >= 0 && modoHablar; s--) {          // abrir lento
            int v = 255 - 255 * s / 5;
            uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], v);
            uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], v);
            fiber_sleep(45);
        }

        int espera = 3000 + uBit.random(4000);                // 3s a 7s
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }
    release_fiber();
}

// ---------------------------------------------------------------------------
// La cara base para hablar: la misma cara cansada (apagada)
// ---------------------------------------------------------------------------
static void dibujarCaraCansado()
{
    uBit.display.setBrightness(75);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
    uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 3; i++)
        uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con cansado activo): prepara la cara y lanza la fibra de ojos
void iniciarHablarCansado()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraCansado();
    uBit.serial.send("TALK-CAN\n");   // debug: confirmar el dispatch
    faseBase = uBit.systemTime();     // el ciclo arranca ahora
    ultimoBoca = -1;                 // fuerza la primera escritura
    create_fiber(fiberOjosCansado);
}

// UN frame de la boca hablando lento. La llama el bucle principal ~60 veces
// por segundo mientras modoHablar siga activo.
void animarBocaCansado()
{
    unsigned long t = (uBit.systemTime() - faseBase) % CICLO_MS;

    // Donde estamos dentro del ciclo de tics, y si estamos en una pausa.
    unsigned int tic = t / TIC_MS;
    unsigned int dentro = t % TIC_MS;
    unsigned int inicioTic = tic * TIC_MS;
    unsigned int inicioPausa = inicioTic + TIC_MS;

    if (PAUSA_ENTRE_TICS_MS > 0 && t >= inicioPausa &&
        t < inicioPausa + PAUSA_ENTRE_TICS_MS) {
        // En la pausa entre tics: la boca cerrada del todo.
        if (ultimoBoca != 0) {
            for (int i = 0; i < 3; i++)
                uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 0);
            ultimoBoca = 0;
        }
        fiber_sleep(16);
        return;
    }

    // Localiza el tramo dentro del tic (4 entradas: busqueda lineal).
    const Segmento *seg = &TIC[NTRAMOS - 1];
    for (int i = 0; i < NTRAMOS; i++) {
        if (dentro >= TIC[i].desde && dentro < TIC[i].hasta) { seg = &TIC[i]; break; }
    }
    float p = (float)(dentro - seg->desde) / (float)(seg->hasta - seg->desde);

    int v = valorEn(seg->tramo, p);
    if (v != ultimoBoca) {
        for (int i = 0; i < 3; i++)
            uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], (uint8_t)v);
        ultimoBoca = v;
    }

    // fiber_sleep() y uBit.sleep() son la MISMA llamada (CodalDevice.cpp:30
    // -> fiber_sleep). Cede la CPU a la fibra de los ojos, al replicador del
    // LED y al stack de BLE. 16 ms = el techo del display (60 Hz).
    fiber_sleep(16);
}
