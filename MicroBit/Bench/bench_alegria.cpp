/**
 * bench_alegria.cpp - BENCH DE HOST: Alegria vieja vs. Alegria con frame
 *
 * Compila DOS versiones REALES de la animacion contra el shim del micro:bit:
 *
 *   AlegriaVieja.cpp  - la anterior, sacada con git show del commit previo al
 *                       patron de frame. Secuencia de ~170 sleep() con el
 *                       progreso DENTRO de los sleep.
 *   ../Codigo/.../Alegria.cpp - la que va en la placa. Patron de frame: una
 *                       llamada = un frame, el estado en systemTime().
 *
 * Ninguna de las dos es una reescritura para el bench: es el codigo que se
 * flashea, byte a byte.
 *
 * Mide lo que se ve en la placa:
 *   1) TIEMPO DE PARADA  - ms que tarda una pasada del ciclo.
 *   2) REACTIVIDAD       - ms desde que llega un comando serial hasta que la
 *                          animacion se entera. Lo que siente la persona.
 *   3) FLUIDEZ           - cambios visibles por segundo y el hueco maximo sin
 *                          cambio (una "animacion a 60fps" con pasos de 40 ms
 *                          es una escalera).
 *   4) TRABAJO           - escrituras al framebuffer, clear(), divisiones
 *                          enteras de setBrightness y heap.
 *
 * Uso:  ./bench_alegria [ciclo_ms]
 */
#include "mock/MicroBit.h"
#include "Animaciones/Emociones/Alegria/Alegria.h"
#include "Animaciones/Sistema/Sistema.h"

#include <stdio.h>

// La version anterior (solo bench).
void animarAlegriaVieja();
void mostrarCaraAlegriaVieja();

// ---------------------------------------------------------------------------
// Estado global del shim + los simbolos que el Codigo real espera del runtime
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

// El delimitador estatico: modela el fix de Sistema.cpp (sin malloc por llamada).
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
// Medicion
// ---------------------------------------------------------------------------
typedef void (*AnimFn)();

// La version nueva es una FUNCION DE FRAME: no "termina" nunca, el bucle la
// llama. La vieja es una secuencia: una llamada = un ciclo completo.

struct Resultado
{
    unsigned long ms;
    double        cpuMs;
    unsigned long frames, gapMax, revisiones, heap;
    unsigned long setPixel, bytesCleared, setBright, sleeps;
};

static Resultado medir(AnimFn fn, unsigned long objetivoMs, bool esFrame)
{
    bench_reset();
    bench_inyectarEnMs = 0;

    unsigned long t0 = bench.nowMs;
    if (esFrame) {
        while (bench.nowMs - t0 < objetivoMs) fn();
    } else {
        fn();
    }
    unsigned long ms = bench.nowMs - t0;

    Resultado r;
    r.ms = ms;
    r.cpuMs = (double)bench.cycles / (BENCH_CPU_HZ / 1000.0);
    r.frames = bench.frames;
    r.gapMax = bench.peorGapMs;
    r.revisiones = bench_revisiones;
    r.heap = bench.heapAllocs;
    r.setPixel = bench.setPixel;
    r.bytesCleared = bench.bytesCleared;
    r.setBright = bench.setBrightness;
    r.sleeps = bench.sleeps;
    return r;
}

static unsigned long medir_latencia(AnimFn fn, unsigned long objetivoMs,
                                    bool esFrame, unsigned long inyectarEnMs)
{
    bench_reset();
    bench_inyectarEnMs = inyectarEnMs;
    unsigned long t0 = bench.nowMs;
    if (esFrame) { while (bench.nowMs - t0 < objetivoMs) fn(); }
    else fn();
    return bench_peorLatenciaMs;
}

static void fila(const char *nombre, const char *a, const char *b)
{
    printf("  %-26s %13s %13s\n", nombre, a, b);
}

static void tabla(unsigned long objetivoMs)
{
    Resultado vieja = medir(animarAlegriaVieja, objetivoMs, false);
    Resultado nueva = medir(animarAlegria,     objetivoMs, true);

    printf("  %-26s %13s %13s\n", "metrica", "VIEJA", "NUEVA (frame)");
    printf("  %-26s %13s %13s\n", "--------------------------", "-------------", "-------------");

    char a[32], b[32];
    snprintf(a, sizeof a, "%lu ms", vieja.ms);   snprintf(b, sizeof b, "%lu ms", nueva.ms);
    fila("ciclo completo", a, b);
    snprintf(a, sizeof a, "%.2f ms", vieja.cpuMs); snprintf(b, sizeof b, "%.2f ms", nueva.cpuMs);
    fila("CPU awake (modelo)", a, b);
    snprintf(a, sizeof a, "%.3f%%", 100.0*vieja.cpuMs/vieja.ms);
    snprintf(b, sizeof b, "%.3f%%", 100.0*nueva.cpuMs/nueva.ms);
    fila("CPU awake (%% del ciclo)", a, b);
    snprintf(a, sizeof a, "%lu", vieja.frames);  snprintf(b, sizeof b, "%lu", nueva.frames);
    fila("cambios visibles", a, b);
    snprintf(a, sizeof a, "%.1f", vieja.frames*1000.0/vieja.ms);
    snprintf(b, sizeof b, "%.1f", nueva.frames*1000.0/nueva.ms);
    fila("fps visual", a, b);
    snprintf(a, sizeof a, "%lu ms", vieja.gapMax); snprintf(b, sizeof b, "%lu ms", nueva.gapMax);
    fila("gap maximo sin cambio", a, b);
    snprintf(a, sizeof a, "%lu", vieja.revisiones); snprintf(b, sizeof b, "%lu", nueva.revisiones);
    fila("puntos de interrupcion", a, b);

    printf("\n  -- trabajo por ciclo --\n");
    printf("  VIEJA : %lu escrituras, %lu bytes clear, %lu setBrightness, %lu sleep\n",
           vieja.setPixel, vieja.bytesCleared, vieja.setBright, vieja.sleeps);
    printf("  NUEVA : %lu escrituras, %lu bytes clear, %lu setBrightness, %lu sleep\n",
           nueva.setPixel, nueva.bytesCleared, nueva.setBright, nueva.sleeps);

    // Latencia: barrido de instantes de inyeccion.
    unsigned long peorV = 0, peorN = 0;
    for (unsigned long t = 60; t < objetivoMs; t += 23) {
        unsigned long lv = medir_latencia(animarAlegriaVieja, objetivoMs, false, t);
        unsigned long ln = medir_latencia(animarAlegria,     objetivoMs, true,  t);
        if (lv > peorV) peorV = lv;
        if (ln > peorN) peorN = ln;
    }
    printf("\n  -- reactividad (comando serial -> la animacion se entera) --\n");
    printf("  PEOR latencia VIEJA : %4lu ms\n", peorV);
    printf("  PEOR latencia NUEVA : %4lu ms\n", peorN);
    if (peorN == 0)
        printf("  la version con frame se entera SIEMPRE en el mismo frame\n");
}

int main(int argc, char **argv)
{
    unsigned long ciclo = (argc > 1) ? strtoul(argv[1], 0, 10) : 6148;
    printf("micro:bit bench - Alegria  (host, modelo Cortex-M0+ @64MHz)\n");
    printf("shim: framebuffer 10x5 real, coste por op deducido del fuente CODAL\n");
    printf("las DOS versiones son el codigo real: la vieja de git, la nueva la\n");
    printf("que se flashea. Ciclo medido: %lu ms\n\n", ciclo);
    tabla(ciclo);
    return 0;
}
