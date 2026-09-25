/**
 * bench_transiciones.cpp - BENCH DE HOST: las transiciones viejas vs. las
 * eficientes.
 *
 * Compila el Codigo/ REAL de Transiciones/ contra el shim del micro:bit y lo
 * contrasta con TransicionesOpt.cpp.
 *
 * Lo que mas importa medir aca NO es la CPU (ya sabemos que es ruido): es la
 * VENTANA MUERTA, el tiempo que la placa tarda en enterarse de un comando
 * mientras una transicion corre. Y eso se mide injecting un comando por USB en
 * un instante arbitrario de la transicion y cronometrando hasta que aparece.
 *
 * Uso:  ./bench_transiciones
 */
#include "mock/MicroBit.h"
#include "Animaciones/Sistema/Sistema.h"
#include "Animaciones/Sistema/Transiciones/Transiciones.h"

#include <stdio.h>
#include <time.h>

void mostrarTransicionOpt(int idx, EmocionActual destino);
// Las tres transiciones eficientes (el prototipo de referencia).
void transicionMorfosisOpt(EmocionActual destino);
void transicionCortinaOpt(EmocionActual destino);
void transicionFundidoOpt(EmocionActual destino);
// Las tres transiciones VIEJAS, de git, para el A/B.
void transicionMorfosisVieja(EmocionActual destino);
void transicionCortinaVieja(EmocionActual destino);
void transicionFundidoVieja(EmocionActual destino);

typedef void (*UnaTrans)(EmocionActual);
static UnaTrans g_vieja[3]  = { transicionMorfosisVieja,
                                 transicionCortinaVieja,
                                 transicionFundidoVieja };
static UnaTrans g_nueva[3]  = { transicionMorfosisOpt,
                                 transicionCortinaOpt,
                                 transicionFundidoOpt };

// ---------------------------------------------------------------------------
// Estado del shim + lo que el Codigo real espera del runtime
// ---------------------------------------------------------------------------
MicroBit uBit;
BenchCounters bench;

unsigned long bench_inyectarEnMs   = 0;
bool         bench_inyectado      = false;
unsigned long bench_inyectadoEnMs  = 0;
unsigned long bench_latenciaMs     = 0;
unsigned long bench_peorLatenciaMs = 0;
unsigned long bench_ultimaRevisionMs = 0;
unsigned long bench_revisiones     = 0;

EmocionActual emocionActual = EM_ALEGRIA;

void bench_reset()
{
    memset(&bench, 0, sizeof(bench));
    bench_inyectado = false;
    bench_inyectadoEnMs = 0;
    bench_latenciaMs = 0;
    bench_peorLatenciaMs = 0;
    bench_ultimaRevisionMs = 0;
    bench_revisiones = 0;
    bench.randomValor = 0;
}

void bench_tick_cycles(double cycles) { bench.cycles += (unsigned long)cycles; }

void bench_notificar_frame()
{
    if (bench.frames > 0) {
        unsigned long gap = bench.nowMs - bench.ultimoFrameMs;
        if (gap > bench.peorGapMs) bench.peorGapMs = gap;
    }
    bench.ultimoFrameMs = bench.nowMs;
    bench.frames++;
}

void bench_inyectar_si_toca()
{
    if (bench_inyectarEnMs == 0 || bench_inyectado) return;
    if (bench.nowMs < bench_inyectarEnMs) return;
    bench_inyectado = true;
    bench_inyectadoEnMs = bench.nowMs;
    uBit.serial.pendiente = "SAD\n";
    uBit.serial.hayComando = true;
}

static ManagedString DELIMITADOR("\n");

bool revisarSerial()
{
    bench_revisiones++;
    ManagedString linea = uBit.serial.readUntil(DELIMITADOR, ASYNC);
    if (linea.length() > 0) {
        bench_latenciaMs = bench.nowMs - bench_inyectadoEnMs;
        if (bench_latenciaMs > bench_peorLatenciaMs) bench_peorLatenciaMs = bench_latenciaMs;
        return true;
    }
    bench_ultimaRevisionMs = bench.nowMs;
    return false;
}

bool bleColaSacar(ManagedString &linea) { (void)linea; return false; }
void procesarComando(ManagedString cmd) { (void)cmd; }
void demoAutomatica() {}
void bleEnviar(ManagedString texto) { (void)texto; }

// ---------------------------------------------------------------------------
// Stubs de las 8 caras. El bench no mide quantos pixeles tiene cada cara, pero
// SI necesita caras realistas: Morfosis lee la pantalla y empareja pixeles, y
// con una cara vacia no habria nada que emparejar y el trabajo medido seria
// fictitious. Cada cara tiene entre 7 y 9 pixeles, como las de verdad.
// ---------------------------------------------------------------------------
static const uint8_t OJOS[2][2] = { {1,1}, {3,1} };

static void caraBase()
{
    uBit.display.image.clear();
    for (int i = 0; i < 2; i++)
        uBit.display.image.setPixelValue(OJOS[i][0], OJOS[i][1], 255);
}

void mostrarCaraAlegria()     { caraBase(); const uint8_t b[5][2]={{0,3},{4,3},{1,4},{2,4},{3,4}};
                                 for (int i=0;i<5;i++) uBit.display.image.setPixelValue(b[i][0],b[i][1],255); }
void mostrarCaraTriste()      { caraBase(); const uint8_t b[5][2]={{0,3},{4,3},{1,4},{2,4},{3,4}};
                                 for (int i=0;i<5;i++) uBit.display.image.setPixelValue(b[i][0],b[i][1],255); }
void mostrarCaraEnojado()     { caraBase(); const uint8_t b[7][2]={{0,1},{4,1},{1,4},{2,4},{3,4},{1,0},{3,0}};
                                 for (int i=0;i<7;i++) uBit.display.image.setPixelValue(b[i][0],b[i][1],255); }
void mostrarCaraSorprendido() { caraBase(); const uint8_t b[7][2]={{0,3},{1,3},{2,3},{3,3},{4,3},{0,4},{4,4}};
                                 for (int i=0;i<7;i++) uBit.display.image.setPixelValue(b[i][0],b[i][1],255); }
void mostrarCaraNeutral()     { caraBase(); const uint8_t b[5][2]={{1,3},{2,3},{3,3},{1,4},{3,4}};
                                 for (int i=0;i<5;i++) uBit.display.image.setPixelValue(b[i][0],b[i][1],255); }
void mostrarCaraFastidio()    { caraBase(); const uint8_t b[5][2]={{1,3},{2,3},{3,3},{1,4},{3,4}};
                                 for (int i=0;i<5;i++) uBit.display.image.setPixelValue(b[i][0],b[i][1],255); }
void mostrarCaraMiedo()       { caraBase(); const uint8_t b[7][2]={{1,3},{2,3},{3,3},{1,4},{2,4},{3,4},{2,0}};
                                 for (int i=0;i<7;i++) uBit.display.image.setPixelValue(b[i][0],b[i][1],255); }
void mostrarCaraCansado()     { caraBase(); const uint8_t b[5][2]={{1,3},{2,3},{3,3},{1,4},{3,4}};
                                 for (int i=0;i<5;i++) uBit.display.image.setPixelValue(b[i][0],b[i][1],255); }

// ---------------------------------------------------------------------------
// Medicion
// ---------------------------------------------------------------------------
struct Resultado
{
    unsigned long ms, frames, revisiones, latencia;
    double        cpuMs, hostMs;
    unsigned long setPixel, clearB, setBright, sleeps;
};

// Coste de una division entera por software en un Cortex-M0+ sin divisor
// hardware: ARM documenta "fewer than 45 cycles" para la version de tiempo
// real, y la normal es mas lenta para cocientes tipicos. Se usa 50.
#define CICLOS_DIVISION_M0  50

// El director real (mostrarTransicion) ya no sirve para el A/B: ahora el
// codigo de la firmware es el eficiente. Se mide cada transicion por su
// nombre, vieja contra nueva.
static Resultado medirUna(UnaTrans fn, unsigned long inyectarEnMs)
{
    bench_reset();
    bench_inyectarEnMs = inyectarEnMs;

    // reloj de CPU del host: proxy. En x86 la division es por hardware, asi
    // que la ganancia real en la micro:bit es MAYOR (alli es software).
    struct timespec c0, c1;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c0);

    uBit.display.image.clear();
    mostrarCaraAlegria();          // cara de partida
    fn(EM_SORPRENDIDO);

    // Tras la transicion, el bucle principal vuelve a mirar el serial.
    revisarSerial();
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &c1);

    Resultado r;
    r.ms = bench.nowMs;
    r.frames = bench.frames;
    r.revisiones = bench_revisiones;
    r.latencia = bench_latenciaMs;
    r.cpuMs = (double)bench.cycles / (BENCH_CPU_HZ / 1000.0);
    r.hostMs = (c1.tv_sec - c0.tv_sec) * 1000.0 + (c1.tv_nsec - c0.tv_nsec) / 1e6;
    r.setPixel = bench.setPixel;
    r.clearB = bench.bytesCleared;
    r.setBright = bench.setBrightness;
    r.sleeps = bench.sleeps;
    return r;
}

static void fila(const char *nombre, const char *a, const char *b)
{
    printf("  %-24s %15s %15s\n", nombre, a, b);
}

static void comparar(const char *nombre, int idx)
{
    UnaTrans vieja = g_vieja[idx], nueva = g_nueva[idx];
    Resultado v = medirUna(vieja, 0);
    Resultado n = medirUna(nueva, 0);

    printf("\n=== %s (transicion %d) ===\n", nombre, idx);
    printf("  %-24s %15s %15s\n", "metrica", "VIEJA", "EFICIENTE");
    printf("  %-24s %15s %15s\n", "------------------------", "---------------", "---------------");

    char a[32], b[32];
    snprintf(a, sizeof a, "%lu ms", v.ms);  snprintf(b, sizeof b, "%lu ms", n.ms);
    fila("duracion", a, b);
    snprintf(a, sizeof a, "%lu", v.frames); snprintf(b, sizeof b, "%lu", n.frames);
    fila("frames dibujados", a, b);
    snprintf(a, sizeof a, "%lu", v.setPixel); snprintf(b, sizeof b, "%lu", n.setPixel);
    fila("escrituras al framebuffer", a, b);
    snprintf(a, sizeof a, "%lu", v.clearB);  snprintf(b, sizeof b, "%lu", n.clearB);
    fila("bytes clear()", a, b);
    snprintf(a, sizeof a, "%lu", v.setBright); snprintf(b, sizeof b, "%lu", n.setBright);
    fila("setBrightness", a, b);
    snprintf(a, sizeof a, "%lu", v.revisiones); snprintf(b, sizeof b, "%lu", n.revisiones);
    fila("chequeos de serial", a, b);
    snprintf(a, sizeof a, "%.3f ms", v.cpuMs); snprintf(b, sizeof b, "%.3f ms", n.cpuMs);
    fila("CPU modelo M0+", a, b);
    snprintf(a, sizeof a, "%.3f ms", v.hostMs); snprintf(b, sizeof b, "%.3f ms", n.hostMs);
    fila("CPU real del host", a, b);

    // Ventana muerta: el peor caso de un comando que llega en el momento mas
    // ciego de la transicion. Se barre el reloj de la transicion injecting
    // el comando en instantes arbitrarios.
    unsigned long peorV = 0, peorN = 0;
    for (unsigned long t = 20; t < v.ms; t += 17) {
        Resultado x = medirUna(vieja, t);
        if (x.latencia > peorV) peorV = x.latencia;
        Resultado y = medirUna(nueva, t);
        if (y.latencia > peorN) peorN = y.latencia;
    }
    snprintf(a, sizeof a, "%lu ms", peorV);
    snprintf(b, sizeof b, "%lu ms", peorN);
    fila("PEOR latencia", a, b);
    printf("  %-24s %15s %15s\n", "  (fracciones de 1 frame)",
           (peorV < 34 ? "1 frame" : "durante toda la transicion"),
           (peorN < 34 ? "1 frame" : "durante toda la transicion"));
    (void)idx;
}

int main()
{
    printf("micro:bit bench - Transiciones  (host, modelo Cortex-M0+ @64MHz)\n");
    printf("el display refresca a 60 Hz (NRF52_LED_MATRIX_FREQUENCY), asi que\n");
    printf("16 ms por frame ya es el maximo util: no tiene sentido ir mas rapido.\n");

    comparar("MORFOSIS", 0);
    comparar("CORTINA",  1);
    comparar("FUNDIDO",  2);

    // ─────────────────────────────────────────────────────────────────
    // DIVISIONES ENTERAS: lo que se MIDIO, no lo que se suponia
    // ─────────────────────────────────────────────────────────────────
    printf("\n=== divisiones enteras por software (__aeabi_idiv en M0+) ===\n");
    printf("  Hipotesis inicial: Morfosis hace ~5 divisiones por pixel por\n");
    printf("  frame, y como el M0+ no tiene divisor por hardware cada una\n");
    printf("  seria una llamada a __aeabi_idiv (~50 ciclos). Sonaba a\n");
    printf("  ~4000 llamadas por transicion.\n\n");
    printf("  MEDIDO en el .obj real de la transicion: 0 llamadas.\n");
    printf("  MEDIDO en TODA la firmware compilada:      0 llamadas.\n\n");
    printf("  Razon: con -O2, GCC puede PROBAR el rango de los divisendos\n");
    printf("  (t viene de f-retardo, con f en [0,125] y retardo en [0,62])\n");
    printf("  y usa una constante magica que cabe en 32 bits, convirtiendo la\n");
    printf("  division en multiplicacion+shift. Sin UMULL no puede hacerlo\n");
    printf("  para divisendos de rango desconocido (ver div_codegen_arm.c:\n");
    printf("  ahi SI aparece la llamada), pero en el codigo real nunca hace\n");
    printf("  falta el rango.\n\n");
    printf("  CONCLUSION: precalcular el retardo de Morfosis NO ahorra nada\n");
    printf("  medible. Queda como codigo mas limpio, no como otimizacion.\n");
    printf("  El unico / que SI sale caro es un divisor de runtime, y el\n");
    printf("  codigo no tiene ninguno en el bucle de frames.\n");

    return 0;
}
