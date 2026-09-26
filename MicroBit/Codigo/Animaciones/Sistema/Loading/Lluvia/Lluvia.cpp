/**
 * Lluvia.cpp - Patron de carga 3: LLUVIA CON TORMENTA 🌧️⚡ (v2)
 *
 * v2 (remodelada): adios a las gotas que saltaban de pixel en pixel a
 * 20ms. Ahora cada gota tiene posicion FLOAT y se mueve a 60fps (16ms):
 *   - SUB-PIXEL: la gota cruza entre 2 pixeles y el brillo se REPARTE
 *     (interpolacion) -> cae suave como seda, sin saltos
 *   - DIRECCION LEGIBLE: cabeza brillante + estela arriba (90/35) -> se
 *     ve claramente que la lluvia CAE
 *   - VELOCIDAD NATURAL E IMPREDECIBLE: cada gota tiene su propia
 *     velocidad al azar (lluvia fina lenta vs gotas gordas rapidas x1.8),
 *     spawns escalonados al azar (0.4-2.3s), densidad moderada (5 gotas)
 *   - SALPICADURA: al tocar el piso destella en 3 pixeles (x, x+/-1)
 *   - TORMENTA: cada 7-12s un RAYO: flash de cielo (pantalla a 170) ->
 *     rayo en ZIGZAG que baja -> fade -> RETUMBO (un pico y que el rastro
 *     lo apague solo, ~350 ms de cola) -> la lluvia se intensifica (todas
 *     respawnean pronto). El rayo se EXENTA del lote de 250 ms porque es un
 *     golpe de un disparo: ver la nota larga de rayo() mas abajo, que explica
 *     por que antes nunca se veia entero.
 *   - BUCLE CONTINUO: estado static + sin clear (la lluvia sigue)
 */
#include "Lluvia.h"
#include "../LoadingBase.h"
#include "../Loading.h"    // modoLoading: el rayo aborta si A/B abre la escucha
#include "../../Sistema.h" // revisarSerial (via frameRastro) + atenderBotonesEscucha

// --- EL TRUENO ------------------------------------------------------------
//
// El pico NO se sube de 55 (el valor que ya estaba): la cola del rayo acaba
// en 40, asi que el retumbo tiene que seguir BAJANDO, y subirlo seria un salto
// de brillo en vez de un trueno. Lo que cambia es la forma, no la energia.
//
// La caida es la del rastro (87% por frame = -2,1 dB, la misma familia que la
// cola larga de Cometa) y dura 24 frames, que es lo que lleva 55 a apagar:
// 55*0,87^24 = 1,9. El eco visible pasa de 3 frames (48 ms) a 19 (304 ms).
#define BRILLO_TRUENO  55
#define CAIDA_TRUENO  87
#define FRAMES_TRUENO 24

static const int N_GOTAS = 5;
static float gx[N_GOTAS];      // columna 0-4
static float gy[N_GOTAS];      // posicion y continua (-0.6..4.2)
static float gv[N_GOTAS];      // velocidad px/frame (0.040..0.090)
static int   gTimer[N_GOTAS];  // espera de spawn o salpicadura (frames)
static int   gFase[N_GOTAS];   // 0=espera, 1=cayendo, 2=salpicando
static bool  iniciado = false;

// Primera vez: repartir gotas con delays escalonados + azar
static void iniciarGotas()
{
    for (int i = 0; i < N_GOTAS; i++) {
        gFase[i] = 0;
        gTimer[i] = 20 + i * 45 + uBit.random(70);
        gx[i] = uBit.random(5);
    }
    iniciado = true;
}

// Render de una gota cayendo: cabeza interpolada entre 2 pixeles + estela
static void pintarGota(int i)
{
    if (gy[i] < 0.0f) return;        // aun fuera de pantalla (evita frac negativo)
    int y0 = (int)gy[i];             // piso
    float frac = gy[i] - y0;         // 0..1: cuanto avanzo al pixel de abajo
    int x = (int)gx[i];

    if (y0 >= 0 && y0 <= 4)
        uBit.display.image.setPixelValue(x, y0, (int)(255.0f * (1.0f - frac)));
    if (y0 + 1 >= 0 && y0 + 1 <= 4)
        uBit.display.image.setPixelValue(x, y0 + 1, (int)(255.0f * frac));

    // estela arriba: marca la direccion de caida
    if (y0 - 1 >= 0) uBit.display.image.setPixelValue(x, y0 - 1, 90);
    if (y0 - 2 >= 0) uBit.display.image.setPixelValue(x, y0 - 2, 35);
}

// Actualiza y pinta todas las gotas (un frame a 60fps)
static void actualizarGotas()
{
    for (int i = 0; i < N_GOTAS; i++) {
        if (gFase[i] == 0) {                         // esperando spawn
            if (gTimer[i] > 0) { gTimer[i]--; continue; }
            gFase[i] = 1;
            gy[i] = -0.6f;
            gx[i] = uBit.random(5);
            gv[i] = 0.040f + (float)uBit.random(50) / 1000.0f;
            if (uBit.random(7) == 0) gv[i] *= 1.8f;  // gota gorda rapida (14%)
            continue;
        }
        if (gFase[i] == 2) {                         // salpicando en el piso
            int x = (int)gx[i];
            uBit.display.image.setPixelValue(x, 4, 255);
            if (x > 0) uBit.display.image.setPixelValue(x - 1, 4, 180);
            if (x < 4) uBit.display.image.setPixelValue(x + 1, 4, 180);
            if (gTimer[i] > 0) { gTimer[i]--; continue; }
            gFase[i] = 0;
            gTimer[i] = 25 + uBit.random(120);       // 0.4-2.3s de espera
            continue;
        }
        // cayendo
        gy[i] += gv[i];
        if (gy[i] > 4.2f) {                          // llego al piso
            gFase[i] = 2;
            gTimer[i] = 3;                           // salpicadura breve
            continue;
        }
        pintarGota(i);
    }
}

// Un frame del rayo: rastro + puerto + botones.
//
// Es frameRastro() mas A/B, y esa es la condicion para que el rayo pueda
// exentarse del lote: si no atendiera los botones, exentarse costaria A/B
// muertos durante los ~500 ms del golpe, que es justo lo que el lote existe
// para evitar. Con esto, exentarse no cuesta nada.
//
// El chequeo de !modoLoading es OBLIGATORIO y no es redundante: si A/B abre
// la escucha manual, detenerLoading() apaga el loading, pero el rayo seguia
// en vuelo y se pondria a pintar encima del aro de la escucha. Sin esta linea
// el boton funciona y al mismo tiempo la animacion lo pisa.
static bool frameRayo(int porciento)
{
    bool corte = frameRastro(porciento);
    atenderBotonesEscucha();
    if (!modoLoading) return true;     // A/B abrio la escucha: el rayo se va
    return corte;
}

// ---------------------------------------------------------------------------
// MARCADORES DEL RAYO (3 lineas de telemetria, una vez cada 7-12 s)
//
// El rayo era el unico trozo de la firmware que no se podia observar: el
// replica LED manda 1 bit por pixel, asi que de un rayo no se ve el BRILLO, y
// la latencia de un comando no lo delata (con el replica encendido el piso es
// de 134 ms, mas grande que cualquier efecto que uno quiera medir). Encima,
// durante el rayo la pantalla se apaga y la replica manda MENOS, asi que la
// latencia medida DENTRO del rayo salia mas baja que fuera: el propio
// instrumento se movia en contra. Por eso estos marcadores.
//
//   RAYO:INI  entro al rayo
//   RAYO:P4   llego al retumbo (paso 4)
//   RAYO:FIN  llego al final, paso 5 incluido
//
// El que importa es RAYO:FIN == RAYO:INI. Con el corte del lote de antes, ese
// numero daba 0 sobre 400 rayos y en la placa NO SE LLEGABA NUNCA (el
// retumbo no era intermitente, no existia). Con la exencion, los tres tienen
// que dar el mismo numero. Se lee asi:
//
//   LOAD3, 240 s, contar RAYO:INI / RAYO:P4 / RAYO:FIN
//
// Cuesta ~30 bytes cada 7-12 s, o sea nada, y no estorba: van por el mismo
// send que el resto de la telemetria y el replica ya los puede perder sin
// problema (ver por que en ReplicaLed.cpp).
// ---------------------------------------------------------------------------
static void marcarRayo(const char *que)
{
    // UN solo send, no tres. Tres envios seguidos son tres transacciones de
    // UART y cada una puede fallar sola si el UART esta ocupado (el ACK de un
    // comando, que tiene prioridad). Con la app mandando comandos cada 95 ms
    // eso no es teorico: se perdia el "RAYO:" y el "P4" por separado, y el
    // host veia "RAYO:\n", que no matchea ningun marcador. Medido: 19 INI y
    // 0 P4, o sea P4 e INI contaban cosas distintas. Armar el texto y
    // mandarlo de una vez elimina esa clase de problema.
    char buf[16];
    int n = 0;
    for (const char *p = "RAYO:"; *p; p++) buf[n++] = *p;
    for (const char *p = que; *p; p++) buf[n++] = *p;
    buf[n++] = '\n';

    // REINTENTA si el UART esta ocupado, y hace falta: el P4 y el FIN se
    // emiten justo despues de un frameRayo(), que puede haber mandado el ACK
    // de un comando serial en ese mismo instante. Sin reintento el send se
    // pierde entero: medido, 23 RAYO:INI y 0 RAYO:FIN con la app mandando un
    // comando cada 95 ms. El INI entraba y el P4 no JUSTO por eso: el INI va
    // al principio del rayo, donde el UART suele estar libre. O sea: no era la
    // placa la que se comia el rayo, era la instrumentacion la que se comia
    // la palabra. Mismo remedio que en Grabar.cpp.
    for (int intento = 0; intento < 3; intento++) {
        if (uBit.serial.send((uint8_t *)buf, n) != DEVICE_SERIAL_IN_USE)
            return;
        uBit.sleep(2);
    }
}

// ---------------------------------------------------------------------------
// EL RAYO, Y POR QUE NO SE CORTA A LA MITAD
//
// El lote de 250 ms (LoadingBase.h) existe porque los 10 patrones corren su
// ciclo ENTERO en una llamada y los botones A/B quedaban muertos 6,4 s. Le
// hace un favor a un patron continuo y un daño a un golpe dramatico: un rayo
// no se puede partir en dos.
//
// Y no es teoria. Bench/sim_rayo.py modela los timers EXACTOS (el LFSR de
// uBit.random, CodalCompat.cpp:109, y el corte del lote) y da, sobre 400
// rayos con el firmware de antes:
//
//     39%  se corta en flash/rayo/fade  -> no se ve NINGUN rayo
//      6%  se corta justo antes del pico
//      5%  pico + 0 de 3 ecos
//     50%  pico + 1 de 3 ecos
//      0%  retumbo completo            <- NUNCA, en ninguno de los 400
//
// O sea que el "RETUMBO (3 pulsos como el eco del trueno)" que anunciaba el
// comentario de la cabecera jamas se vio entero, y casi 4 de cada 10
// relampagos no se veian siquiera. La segunda corrida de la simulacion, con
// random(300) uniforme en vez del LFSR, da 36/6/7/52/0: el numero no depende
// de que realizacion del LFSR global este en juego en ese momento.
//
// Y el corte del lote no hacia nada por el sleep(120): el sleep va DESPUES del
// frameRastro que chequea, asi que se ejecutaba igual y encima costaba 7,5
// frames de puerto sordo, la ventana mas larga de la firmware.
//
// El arreglo es de las dos clases: exento el rayo del lote (esta funcion) y
// hecho el retumbo con el rastro en vez de a mano (abajo). Con el flag en un
// unico sitio y este wrapper como unica salida, no hay forma de que se olvide
// restaurarlo en uno de los muchos `return` de rayoInterno().
// ---------------------------------------------------------------------------
static void rayoInterno()
{
    int xs[5];
    xs[0] = uBit.random(5);                          // el rayo baja en zigzag
    for (int r = 1; r < 5; r++) {
        xs[r] = xs[r - 1] + uBit.random(3) - 1;
        if (xs[r] < 0) xs[r] = 0;
        if (xs[r] > 4) xs[r] = 4;
    }
    marcarRayo("INI");
    // 1) FLASH: el cielo entero se ilumina
    for (int f = 0; f < 2; f++) {
        if (frameRayo(92)) return;
        for (int x = 0; x < 5; x++)
            for (int y = 0; y < 5; y++)
                uBit.display.image.setPixelValue(x, y, 170);
        uBit.sleep(16);
    }
    // 2) RAYO: el zigzag queda crispado sobre el cielo que se apaga
    for (int f = 0; f < 2; f++) {
        if (frameRayo(85)) return;
        for (int r = 0; r < 5; r++)
            uBit.display.image.setPixelValue(xs[r], r, 255);
        uBit.sleep(16);
    }
    // 3) FADE del rayo
    for (int f = 0; f < 3; f++) {
        if (frameRayo(80)) return;
        for (int r = 0; r < 5; r++)
            uBit.display.image.setPixelValue(xs[r], r, 160 - f * 40);
        uBit.sleep(16);
    }
    marcarRayo("P4");
    // 4) RETUMBO DEL TRUENO: un pico y que el rastro lo apague solo
    //
    // QUE HACIA ANTES, y por que estaba mal en tres cosas a la vez:
    //
    //  a) uBit.sleep(120) x3. Eran 7,5 frames sin mirar el puerto, cada
    //     7-12 s mientras llueve. Y el lote no los evitaba: el sleep va
    //     DESPUES del frameRastro que chequea, asi que se ejecutaba igual y
    //     encima le sobraba lote para dormir a gusto.
    //
    //  b) TRES ECOS ESCRITOS A MANO (55-15p) que pisaban el rastro. Eso es
    //     una caida lineal de tres escalones, hecha a mano, en el unico
    //     sistema que ya tiene caida exponencial hecha (decaerRastro). O
    //     sea: a mano, y peor, donde el sistema ya tenia la respuesta.
    //
    //  c) CADA ECO DURABA UN FRAME (16 ms). El ojo necesita del orden de
    //     50-100 ms para registrar un destello tenue, asi que a 55 de brillo
    //     un pico de un frame no es "sutil": no se ve. Y como el lote lo
    //     cortaba en el primer frame de esta seccion, en la practica no se
    //     veia ninguno de los tres.
    //
    // QUE HACE AHORA. Un pico de 55 (el MISMO de antes, y a proposito: la cola
    // del rayo acaba en 40, el retumbo tiene que seguir bajando) y 24 frames
    // de caida por el mismo decaerRastro del resto de los patrones. Tres
    // cosas cambian a la vez y las tres son para mejor: la ventana sorda
    // desaparece (cada frame chequea), la forma es exponencial de verdad en
    // vez de tres escalones, y el pico dura lo suficiente para que se vea.
    if (frameRayo(CAIDA_TRUENO)) return;
    for (int x = 0; x < 5; x++)
        for (int y = 0; y < 5; y++)
            uBit.display.image.setPixelValue(x, y, BRILLO_TRUENO);
    uBit.sleep(16);
    for (int f = 0; f < FRAMES_TRUENO; f++) {
        if (frameRayo(CAIDA_TRUENO)) return;
        uBit.sleep(16);
    }
    // 5) LA LLUVIA SE INTENSIFICA: todas respawnean pronto
    for (int i = 0; i < N_GOTAS; i++) {
        gFase[i] = 0;
        gTimer[i] = 5 + uBit.random(40);
    }
    // Solo se llega aca si NINGUN paso hizo return: o sea, si el rayo entero
    // llego al final. Los `return` de arriba se lo saltan, que es el punto.
    marcarRayo("FIN");
}

// El rayo, sin corte por lote. El wrapper existe para que loteExento SIEMPRE
// se restaure: rayoInterno() tiene un `return` en cada uno de sus 4 pasos, y
// si el flag se quedara levantado, el loading se quedaria sin cortar para
// siempre (los 6,4 s de A/B muertos, otra vez, y para siempre).
static void rayo()
{
    loteExento = true;
    rayoInterno();
    loteExento = false;
}

void animarLluvia(int ciclos)
{
    if (!iniciado) iniciarGotas();
    static int proximoRayo = 420 + uBit.random(300);   // 1er rayo a los 7-12s

    for (int c = 0; c < ciclos; c++) {
        for (int f = 0; f < 400; f++) {                // ~6.4s por ciclo
            if (frameRastro(78)) return;               // rastro + abortar si manda algo
            actualizarGotas();
            if (proximoRayo <= 0) {
                proximoRayo = 420 + uBit.random(300);  // proximo rayo en 7-12s
                rayo();                                 // (reseteado ANTES: si aborta, no se repite)
            } else {
                proximoRayo--;
            }
            uBit.sleep(16);
        }
    }
    // NO limpia al final: la lluvia sigue (bucle continuo)
}
