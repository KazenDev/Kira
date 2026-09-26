/**
 * HablarMiedo.cpp - La boca del MIEDO que HABLA (tartamudea)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME (la boca): un tramo del guion por frame, el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ANTES esta boca era una cadena de fiber_sleep() y el bucle principal se
 * comia 1180 ms SIN mirar el serial (medido: 1165 ms de ventana sorda), la
 * segunda peor del proyecto. Y pegaba mas de lo que parece, porque
 * Principal.cpp le da PRIORIDAD a TALK sobre la animacion de la emocion:
 * mientras la IA habla, esta boca es TODO lo que corre en el hilo principal.
 *
 * Ahora animarBocaMiedo() es una FUNCION DE FRAME: un frame (~16 ms) del
 * guion y vuelve. El estado vive en systemTime(), asi que el comando de la IA
 * se nota en el frame siguiente. Mismo patron que el resto.
 *
 * ── EL GUION: LAS TRES ALTERACIONES DEL TARTAMUDEO ───────────────────────
 * Esta es la unica boca cuyo ritmo NO es un oscilador libre: es un guion de
 * 5 pasos que se repite. Y la literatura de la tartamudeo define exactamente
 * tres alteraciones del flujo: "el flujo natural del habla se interrumpe por
 * BLOQUEOS, REPETICIONES o PROLONGACIONES de silabas". El guion tiene las
 * tres, y en ese orden:
 *
 *     repeticiones -> bloqueo -> prolongacion
 *     t-t            150 ms      t......miedo
 *
 *   - las dos REPETICIONES son los tics cortos de 50 ms (240 ms cada uno):
 *     el "t-t" de repetir la palabra.
 *   - el BLOQUEO es la pausa de 150 ms. Y OJO con un detalle que queda fino:
 *     durante el bloqueo la boca queda ABIERTA a 255, no cerrada. Es el
 *     intento de decir la palabra antes de que se trabe: el personaje ya esta
 *     abriendo la boca para el sonido que no sale. Asi que el bloqueo NO es
 *     una pausa en negro, es el pico de la frase.
 *   - la PROLONGACION es el tic de 120 ms de hold (310 ms), que es el mas
 *     largo de todo el guion y va AL FINAL: la palabra que por fin sale
 *     trabada ("...miedo").
 *
 * O sea que el guion ya era el correcto. No se le cambian los tiempos.
 *
 * ── POR QUE LA BOCA NO NECESITA SU PROPIO revisarSerial() ───────────────
 * El bucle principal ya hace, en este orden:
 *     revisarSerial();
 *     atenderBotonesEscucha();
 *     if (modoHablar) animarBocaMiedo();
 * Con la boca devolviendo cada 16 ms, el chequeo del puerto queda a 60 Hz.
 *
 * ── LA FIBRA DE LOS OJOS NO SE TOCA ──────────────────────────────────────
 * Ya esta bien: fiber_sleep() cede la CPU, rompe el loop en cuanto modoHablar
 * es false, se libera sola, y no procesa comandos. La separacion de pixeles se
 * mantiene: la boca toca las filas 2 a 4, la fibra los ojos y las pupilas
 * (0,1)(4,1)(1,1)(3,1).
 */
#include "HablarMiedo.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Miedo.cpp)
static const uint8_t OJO_IZQ[2] = {0, 1};
static const uint8_t OJO_DER[2] = {4, 1};
static const uint8_t PUPILA_IZQ[2] = {1, 1};
static const uint8_t PUPILA_DER[2] = {3, 1};

// La boca GRANDE abierta (la que tartamudea): 8 pixeles
static const uint8_t BOCA[8][2] = {
    {1, 2}, {2, 2}, {3, 2},
    {1, 3}, {3, 3},
    {1, 4}, {2, 4}, {3, 4}
};

// ---------------------------------------------------------------------------
// EL GUION COMO TABLA
//
// Las tres alteraciones del tartamudeo, en orden, con los tiempos exactos que
// tenia la version con 5 llamadas a ticBocaMiedo():
//
//   tic A  cierre 60 + hold 50 + apertura 60 + descanso 70  = 240
//   tic B  cierre 60 + hold 50 + apertura 60 + descanso 70  = 240
//   bloqueo                                            150
//   tic C  cierre 60 + hold 50 + apertura 60 + descanso 70  = 240
//   tic D  cierre 60 + hold 120 + apertura 60 + descanso 70 = 310
//                                                        ------
//                                                          1180 ms
// ---------------------------------------------------------------------------
enum Tramo
{
    T_CIERRA = 0,  // el grito se traga
    T_CERRADO,     // trabado, boca cerrada
    T_ABRE,        // el grito vuelve
    T_DESCANSO,    // entre tics, boca abierta
    T_BLOQUEO      // la pausa trabada: boca ABIERTA (el intento de la palabra)
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  tramo;
};

static const Segmento GUION[] = {
    {   0,  60, T_CIERRA    },   // tic A
    {  60, 110, T_CERRADO    },
    { 110, 170, T_ABRE       },
    { 170, 240, T_DESCANSO   },
    { 240, 300, T_CIERRA    },   // tic B
    { 300, 350, T_CERRADO    },
    { 350, 410, T_ABRE       },
    { 410, 480, T_DESCANSO   },
    { 480, 630, T_BLOQUEO    },   // <- la pausa trabada, boca a 255
    { 630, 690, T_CIERRA    },   // tic C
    { 690, 740, T_CERRADO    },
    { 740, 800, T_ABRE       },
    { 800, 870, T_DESCANSO   },
    { 870, 930, T_CIERRA    },   // tic D
    { 930,1050, T_CERRADO    },   // <- la prolongacion (hold de 120 ms)
    {1050,1110, T_ABRE       },
    {1110,1180, T_DESCANSO   },
};
static const int NTRAMOS = sizeof(GUION) / sizeof(GUION[0]);
static const unsigned short CICLO_MS = 1180;

static unsigned long faseBase = 0;
static int ultimoGrito = -1;

// El brillo de la boca en este instante del tramo.
static int valorEn(unsigned char tramo, float p)
{
    switch (tramo)
    {
        case T_CIERRA:   return 255 - (int)(255 * p);                 // 60 ms
        case T_CERRADO:  return 0;
        case T_ABRE:     return (int)(255 * p);                       // 60 ms
        case T_DESCANSO: return 255;                                 // 70 ms
        default:         return 255;   // T_BLOQUEO: el intento queda abierto
    }
}

// ---------------------------------------------------------------------------
// FIBRA: ojos del miedo EN PARALELO (SIN CAMBIOS: ya cede la CPU, rompe el
// loop sola y no procesa comandos)
// 60% parpadeo rapidisimo, 25% mirada al centro, 15% temblor de cuerpo.
// ---------------------------------------------------------------------------

static void fiberOjosMiedo(void)
{
    while (modoHablar) {
        int r = uBit.random(100);
        if (r < 60) {
            for (int i = 0; i < 2 && modoHablar; i++) {       // parpadeo rapidisimo
                uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 0);
                uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 0);
                fiber_sleep(50);
                uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
                uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
                fiber_sleep(80);
            }
        }
        else if (r < 85) {
            // Mirada al centro (la amenaza) y vuelve
            uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 0);
            uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 0);
            uBit.display.image.setPixelValue(PUPILA_IZQ[0], PUPILA_IZQ[1], 255);
            uBit.display.image.setPixelValue(PUPILA_DER[0], PUPILA_DER[1], 255);
            fiber_sleep(200);
            uBit.display.image.setPixelValue(PUPILA_IZQ[0], PUPILA_IZQ[1], 0);
            uBit.display.image.setPixelValue(PUPILA_DER[0], PUPILA_DER[1], 0);
            uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
            uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
        }
        else {
            // Temblor de cuerpo (el brillo vibra un momento)
            for (int i = 0; i < 5 && modoHablar; i++) {
                uBit.display.setBrightness(85 + uBit.random(70));
                fiber_sleep(30);
            }
            uBit.display.setBrightness(100);
        }

        int espera = 500 + uBit.random(1500);              // 0.5s a 2s
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }
    release_fiber();
}

// ---------------------------------------------------------------------------
// La cara base para hablar: la misma cara del grito
// ---------------------------------------------------------------------------
static void dibujarCaraMiedo()
{
    uBit.display.setBrightness(100);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(1, 0, 255);   // cejas
    uBit.display.image.setPixelValue(3, 0, 255);
    uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
    uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 8; i++)
        uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con miedo activo): prepara la cara y lanza la fibra de ojos
void iniciarHablarMiedo()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraMiedo();
    uBit.serial.send("TALK-MIE\n");   // debug: confirmar el dispatch
    faseBase = uBit.systemTime();     // el guion arranca ahora
    ultimoGrito = -1;                // fuerza la primera escritura
    create_fiber(fiberOjosMiedo);
}

// UN frame del tartamudeo. La llama el bucle principal ~60 veces por segundo.
void animarBocaMiedo()
{
    unsigned long t = (uBit.systemTime() - faseBase) % CICLO_MS;

    // Localiza el tramo (17 entradas: busqueda lineal, ~70 ciclos por frame,
    // o sea 0,04 ms en un ciclo de 1180. La tabla vive en FLASH).
    const Segmento *seg = &GUION[NTRAMOS - 1];
    for (int i = 0; i < NTRAMOS; i++) {
        if (t >= GUION[i].desde && t < GUION[i].hasta) { seg = &GUION[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    int v = valorEn(seg->tramo, p);
    if (v != ultimoGrito) {
        for (int i = 0; i < 8; i++)
            uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], (uint8_t)v);
        ultimoGrito = v;
    }

    // fiber_sleep() y uBit.sleep() son la MISMA llamada (CodalDevice.cpp:30
    // -> fiber_sleep). Cede la CPU a la fibra de los ojos, al replicador del
    // LED y al stack de BLE. 16 ms = el techo del display (60 Hz).
    fiber_sleep(16);
}
