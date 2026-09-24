/**
 * Metronomo.cpp - Implementacion del METRONOMO 🎵⏱️
 *
 * Tres piezas:
 *   1. FIBRA DE PULSOS (fibraMetro): duerme 1 ms y cuando llega la hora
 *      absoluta del pulso dispara el CLICK (parlante) y el FLASH visual.
 *      El visual no lo pinta la fibra: solo deja la marca de tiempo; el
 *      bucle principal (metroFrame) dibuja el pendulo a 60fps.
 *   2. RENDER (metroFrame): pendulo en la fila del medio con easing
 *      SINUSOIDAL (rapido al centro, lento en los extremos, como un
 *      pendulo REAL: Galileo 1602, isocronismo de las pequenas
 *      oscilaciones), cola de cometa en la direccion del vaiven,
 *      contador de compas arriba y destello en el acento.
 *   3. BOTONES A/B (en metroFrame): ajuste suelto de tempo sin PC.
 *
 * EL CLICK: expresion de sonido de CODAL (el mismo formato de 72 chars
 * que usa MakeCode: onda, volumen y frecuencia de inicio -> fin, etc).
 * Receta del tic agradable: onda SENO (no cuadrada: esa es de alarma),
 * frecuencia que CAE a la curva y volumen que muere dentro del pulso
 * -> suena a "pluck" de metronomo mecanico, no a bip de microondas.
 * El acento es mas agudo y mas fuerte (tic-tic-tac de verdad).
 */
#include "Metronomo.h"
#include "../Sistema.h"                       // emocionActual (enum EmocionActual)
#include "../Transiciones/Transiciones.h"     // hacerTransicion() (A+B = stop)
#include "../Loading/Loading.h"               // detenerLoading() (limpieza A+B)
#include "../Voz/Voz.h"                       // detenerVoz()
#include "../Escuchar/Escuchar.h"             // escucharDetener()
#include "../Emociones/Emociones.h"           // modoHablar / detenerHablar()
#include <stdio.h>
#include <math.h>

bool modoMetro = false;

// ---------------------------------------------------------------------------
// Estado interno (privado del modulo)
// ---------------------------------------------------------------------------
static int     metroBpm     = 90;    // tempo actual
static int     metroAcentos = 4;     // acento cada N pulsos (0 = parejo)
static int     intervaloMs  = 667;   // 60000 / bpm
static int     numeroPulso  = 0;     // cuenta TOTAL de pulsos (para el vaiven)
static uint32_t proximoPulso = 0;    // hora absoluta del proximo click (ms)

// Lo escribe la fibra en cada pulso; lo lee el render (60fps):
static volatile uint32_t flashPulso  = 0;   // hora del pulso en curso
static volatile int      flashIndice = 0;   // numero de pulso en curso
static volatile int      flashAcento = 0;   // 1 = este pulso lleva acento
static volatile bool     pulsoActivo = false;

// GENERACION de la fibra: cada iniciar/detener la incrementa. La fibra
// compara SU generacion con la global y muere sola si cambiaron. Sin
// esto, un METRO:X:Y seguido de otro podria dejar DOS fibras vivas
// (la vieja despierta despues del cambio y ve modoMetro == true otra vez).
static volatile int generacion = 0;

// Botones: estado anterior (para detectar flanco, no repeticion)
static bool prevA = false;
static bool prevB = false;

// ---------------------------------------------------------------------------
// Parametros del sonido (expresion de sonido de CODAL, 72 chars)
// ---------------------------------------------------------------------------
// ACENTO (tic agudo del compas 1): seno 1568 Hz (Sol 6) cayendo a 200
#define ACENTO_VOLUMEN   1000    // 0..1023
#define ACENTO_FRECUENCIA 1568   // Hz inicial (agudo pero musical)
#define ACENTO_FIN_FREQ   200    // Hz final (la caida hace el "tac")
#define ACENTO_DURACION    90    // ms

// PULSO NORMAL: seno 1046 Hz (Do 6), mas suave y un pelito mas corto
#define NORMAL_VOLUMEN    800
#define NORMAL_FRECUENCIA 1046
#define NORMAL_FIN_FREQ    150
#define NORMAL_DURACION     80

/**
 * Construye la expresion de sonido de un click (string de 72 chars con
 * campos de ancho fijo, ver SoundExpressions.cpp de CODAL):
 *   [0]      onda: 0 = SENO (musical, no de alarma)
 *   [1-4]    volumen inicial 0..1023
 *   [5-8]    frecuencia inicial Hz
 *   [9-12]   duracion ms
 *   [13-14]  forma: 02 = interpolacion CURVA (cae rapido, cola suave)
 *   [18-21]  frecuencia final (el tono CAE -> cuerpo percusivo)
 *   [26-29]  volumen final 0 (decae a silencio dentro de la duracion)
 *   [30-71]  pasos, fx y aleatoriedad: todo en cero
 */
static ManagedString expresionClick(int volumen, int frecIni, int frecFin, int duracionMs)
{
    char s[73];
    snprintf(s, sizeof(s),
        "0"         // [0] onda seno
        "%04d"      // [1-4]  volumen inicial
        "%04d"      // [5-8]  frecuencia inicial
        "%04d"      // [9-12] duracion (ms)
        "02"        // [13-14] forma: curva hacia la frecuencia final
        "000"       // [15-17] sin uso
        "%04d"      // [18-21] frecuencia final (tono que cae)
        "0000"      // [22-25] sin uso
        "0000"      // [26-29] volumen final (silencio)
        "0001"      // [30-33] pasos
        "00"        // [34-35] sin vibrato
        "0000"      // [36-39] param fx
        "0000"      // [40-43] pasos fx
        "0000000000000000000000000000",   // [44-71] sin aleatoriedad
        volumen, frecIni, duracionMs, frecFin);
    return ManagedString(s);
}

// ---------------------------------------------------------------------------
// La fibra de pulsos: CORAZON del metronomo (timing isocrono)
// ---------------------------------------------------------------------------
static void fibraMetro()
{
    const int miGeneracion = generacion;   // me registro en la generacion actual

    while (miGeneracion == generacion) {
        uint32_t ahora = uBit.systemTime();

        // ¿llego la hora del pulso? (comparacion wrap-safe)
        if ((int32_t)(ahora - proximoPulso) >= 0) {
            bool acento = (metroAcentos > 0) && (numeroPulso % metroAcentos == 0);

            // CLICK (no bloquea: playAsync deja que la fibra siga)
            uBit.audio.soundExpressions.playAsync(
                acento
                    ? expresionClick(ACENTO_VOLUMEN, ACENTO_FRECUENCIA, ACENTO_FIN_FREQ, ACENTO_DURACION)
                    : expresionClick(NORMAL_VOLUMEN, NORMAL_FRECUENCIA, NORMAL_FIN_FREQ, NORMAL_DURACION));

            // Marca para el RENDER (el pendulo lo dibuja el bucle principal)
            flashPulso  = ahora;
            flashIndice = numeroPulso;
            flashAcento = acento ? 1 : 0;
            pulsoActivo = true;

            numeroPulso++;
            // Anclaje ABSOLUTO: el proximo pulso es este + intervalo. Si un
            // frame se demoro, el proximo NO hereda el retraso (no drift).
            proximoPulso += intervaloMs;
            // Si quedamos TAN atras que el proximo pulso ya paso (la fibra
            // durmio demasiado), re-anclamos al futuro: no hacer rafagas.
            if ((int32_t)(ahora - proximoPulso) >= 0)
                proximoPulso = ahora + intervaloMs;
        }

        uBit.sleep(1);   // granularidad fina: jitter de ~1-2 ms por pulso
    }
    // generacion cambio (metroDetener o nuevo metroIniciar): la fibra muere
}

// ---------------------------------------------------------------------------
// metroIniciar: arranca (o cambia tempo en caliente, sin perder el pulso)
// ---------------------------------------------------------------------------
void metroIniciar(int bpm, int acento)
{
    if (bpm < 20)    bpm = 20;
    if (bpm > 250)   bpm = 250;
    if (acento < 0)  acento = 0;
    if (acento > 5)  acento = 5;   // la fila del contador tiene 5 LEDs

    bool yaCorria = modoMetro;

    metroBpm     = bpm;
    metroAcentos = acento;
    intervaloMs  = 60000 / bpm;

    if (yaCorria) {
        // Cambio de tempo EN CALIENTE: desde el proximo pulso vale lo nuevo
        // (el pulso en curso termina con su vaiven natural)
        return;   // la fibra vigente lee las nuevas variables solas
    }

    // Arranque desde cero
    numeroPulso  = 0;
    pulsoActivo  = false;
    prevA = prevB = false;
    proximoPulso = uBit.systemTime() + 500;   // medio segundo de aire antes del primer tic

    // Parlante LISTO de verdad: activa el pipeline de audio completo
    // (el sintetizador usa el mixer; sin esto en algunos arranques queda
    // en reposo y el click no se oye), parlante encendido y a fondo.
    uBit.audio.requestActivation();
    uBit.audio.setSpeakerEnabled(true);
    uBit.audio.setVolume(255);   // feria = hay ruido: maximo volumen

    generacion++;             // fibra nueva; cualquier fibra vieja muere sola
    modoMetro = true;
    create_fiber(fibraMetro);
}

// ---------------------------------------------------------------------------
// metroDetener: corta fibra + click en vuelo
// ---------------------------------------------------------------------------
void metroDetener()
{
    if (!modoMetro)
        return;
    generacion++;                  // la fibra muere en su proxima vuelta (<=1 ms)
    modoMetro = false;
    uBit.audio.soundExpressions.stop();   // calla un click que estuviera sonando
}

// ---------------------------------------------------------------------------
// Ajuste de tempo con botones (suelto, sin PC). Devuelve el cambio aplicado.
// ---------------------------------------------------------------------------
static void ajustarTempo(int delta)
{
    metroBpm += delta;
    if (metroBpm < 20)  metroBpm = 20;
    if (metroBpm > 250) metroBpm = 250;
    intervaloMs = 60000 / metroBpm;
    // Sin re-anclar: el proximo pulso sale con el intervalo nuevo
}

// ---------------------------------------------------------------------------
// metroFrame: UN frame del pendulo a 60fps + botones A/B
// ---------------------------------------------------------------------------
void metroFrame()
{
    if (!modoMetro) {
        uBit.sleep(50);
        return;
    }

    // ---------------- BOTONES (control suelto, sin PC) ----------------
    bool a = uBit.buttonA.isPressed();
    bool b = uBit.buttonB.isPressed();

    if (a && b) {
        // A+B = DETENER: limpieza completa y vuelta a la alegria (con
        // transicion), exactamente como el comando serial STOP.
        if (modoHablar)   detenerHablar();
        detenerLoading();
        detenerVoz();
        if (modoEscuchar) escucharDetener();
        metroDetener();
        hacerTransicion(EM_ALEGRIA);
        emocionActual = EM_ALEGRIA;
        return;   // el bucle principal ya renderiza la alegria
    }
    // Flanco (apretar, no mantener): A baja, B sube, de a 5 BPM
    if (a && !prevA && !b) ajustarTempo(-5);
    if (b && !prevB && !a) ajustarTempo(+5);
    prevA = a;
    prevB = b;

    // ---------------- RENDER DEL PENDULO (60 fps) ----------------
    uint32_t ahora = uBit.systemTime();

    // Sin pulso todavia (el medio segundo de aire inicial): un punto
    // central que RESPIRA (mismo lenguaje visual que el aro de voz)
    if (!pulsoActivo) {
        float t = ahora / 1000.0f;
        int br = (int)(90 + 60 * sinf(t * 3.1f));
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 5; x++)
                uBit.display.image.setPixelValue(x, y, (x == 2 && y == 2) ? br : 0);
        uBit.sleep(16);
        return;
    }

    // Progreso DENTRO del pulso actual: 0.0 -> 1.0
    float p = (float)(ahora - flashPulso) / (float)intervaloMs;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;

    // EASING SINUSOIDAL: la posicion de un pendulo REAL no avanza lineal,
    // va rapido al centro y se frena en los extremos (proyeccion del
    // movimiento circular: pos = (1 - cos(pi * p)) / 2). Es lo que hace
    // que el vaiven se VEA fluido y "de metronomo" y no de scanner.
    float ease = 0.5f - 0.5f * cosf(3.14159265f * p);

    // VAIVEN: un extremo al otro por pulso, alternando (pares ->, impares <-)
    float pos = (flashIndice % 2 == 0) ? (ease * 4.0f) : (4.0f - ease * 4.0f);

    // Direccion del movimiento (para la cola de cometa, que queda ATRAS)
    int dir = (flashIndice % 2 == 0) ? +1 : -1;

    // Destello del ACENTO (cae con curva cuadratica en ~160 ms): ilumina
    // las filas de arriba y abajo del pendulo para que el "1" SE VEA.
    float destello = 0.0f;
    if (flashAcento) {
        destello = 1.0f - (float)(ahora - flashPulso) / 160.0f;
        if (destello > 0.0f) destello = destello * destello;   // caida suave
        else destello = 0.0f;
    }

    int brilloAcento = (int)(120 * destello);

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            int br = 0;

            // Fila del medio: el TIP con halo + COLA de cometa. La cola se
            // estira HACIA ATRAS del movimiento (dir opuesto), como los
            // loadings con rastro: brillo que muere cuadraticamente.
            if (y == 2) {
                float d = (float)x - pos;
                float distancia = (d < 0) ? -d : d;
                float cerca = 1.0f - distancia / 1.15f;   // halo del tip
                if (cerca > 0.0f) {
                    if (cerca > 1.0f) cerca = 1.0f;
                    br = (int)(255 * cerca * cerca);
                }
                // LED de atras: brillo de cola (solo si quedo "detras")
                float detras = -(float)x * dir + pos * dir;   // >0 = esta atras
                if (detras > 0.6f && detras < 1.8f && br < 90)
                    br = 90;                                   // cola visible
            }

            // Destello del acento: filas 1 y 3 completas
            else if ((y == 1 || y == 3) && brilloAcento > 0) {
                br = brilloAcento;
            }

            // Fila de arriba: CONTADOR DE COMPAS (un LED por pulso del
            // compas: pasado tenue, el ACTUAL full, futuro apagado)
            else if (y == 0 && metroAcentos > 0 && x < metroAcentos) {
                int actual = (flashIndice - 1) % metroAcentos;   // pulso en curso
                if (x == actual)      br = 255;
                else if (x < actual)  br = 45;
            }

            uBit.display.image.setPixelValue(x, y, br);
        }
    }

    uBit.sleep(16);   // ~60 fps
}
