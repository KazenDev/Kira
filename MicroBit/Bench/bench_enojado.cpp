/**
 * bench_enojado.cpp - BENCH DE HOST: Enojado vieja vs. Enojado con patron de frame.
 *
 * Las dos versiones son codigo REAL: la vieja sale de git show del commit
 * previo al patron de frame (con la funcion renombrada), la nueva es la que
 * se flashea.
 *
 * Enojado es el caso MECANICO: no tiene azar, todo sale de la tabla de
 * segmentos y de la fase. Por eso es la de referencia para las
 * cinco que quedan sin azar.
 *
 * Uso:  ./bench_enojado
 */
#include "mock/MicroBit.h"
#include "Animaciones/Sistema/Sistema.h"
#include "Animaciones/Emociones/Enojado/Enojado.h"

#include <stdio.h>

void animarEnojadoVieja();     // la anterior, de git

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

EmocionActual emocionActual = EM_TRISTE;

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
    uBit.serial.pendiente = "HAPPY\n";
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
void dibujarCaraDestino(EmocionActual d) { (void)d; }

// ---------------------------------------------------------------------------
typedef void (*AnimFn)();

struct Resultado
{
    unsigned long ms, frames, revisiones, gapMax, setPixel, clearB, setBright, sleeps;
    double cpuMs, fps;
};

static Resultado medir(AnimFn fn, unsigned long objetivoMs, bool esFrame)
{
    bench_reset();
    bench_inyectarEnMs = 0;

    uBit.display.image.clear();
    mostrarCaraEnojado();

    unsigned long t0 = bench.nowMs;
    if (esFrame) { while (bench.nowMs - t0 < objetivoMs) fn(); }
    else fn();
    unsigned long ms = bench.nowMs - t0;

    Resultado r;
    r.ms = ms;
    r.frames = bench.frames;
    r.revisiones = bench_revisiones;
    r.gapMax = bench.peorGapMs;
    r.setPixel = bench.setPixel;
    r.clearB = bench.bytesCleared;
    r.setBright = bench.setBrightness;
    r.sleeps = bench.sleeps;
    r.cpuMs = (double)bench.cycles / (BENCH_CPU_HZ / 1000.0);
    r.fps = ms ? (double)bench.frames * 1000.0 / ms : 0;
    return r;
}

static unsigned long medir_latencia(AnimFn fn, unsigned long objetivoMs,
                                    bool esFrame, unsigned long inyectar)
{
    bench_reset();
    bench_inyectarEnMs = inyectar;
    uBit.display.image.clear();
    mostrarCaraEnojado();
    unsigned long t0 = bench.nowMs;
    if (esFrame) { while (bench.nowMs - t0 < objetivoMs) fn(); }
    else fn();
    return bench_latenciaMs;
}

static void fila(const char *n, const char *a, const char *b)
{
    printf("  %-24s %13s %13s\n", n, a, b);
}

int main()
{
    const unsigned long CICLO_NUEVO = 4568;
    const unsigned long CICLO_VIEJO = 4568;

    printf("micro:bit bench - Enojado (host, modelo Cortex-M0+ @64MHz)\n");
    printf("las dos versiones son codigo real (la vieja sale de git show)\n\n");

    Resultado v = medir(animarEnojadoVieja, CICLO_VIEJO, false);
    Resultado n = medir(animarEnojado,      CICLO_NUEVO, true);

    printf("  %-24s %13s %13s\n", "metrica", "VIEJA", "NUEVA (frame)");
    printf("  %-24s %13s %13s\n", "------------------------", "-------------", "-------------");
    char a[24], b[24];
    snprintf(a, sizeof a, "%lu ms", v.ms);    snprintf(b, sizeof b, "%lu ms", n.ms);
    fila("ciclo", a, b);
    snprintf(a, sizeof a, "%.1f", v.fps);     snprintf(b, sizeof b, "%.1f", n.fps);
    fila("fps visual", a, b);
    snprintf(a, sizeof a, "%lu ms", v.gapMax); snprintf(b, sizeof b, "%lu ms", n.gapMax);
    fila("gap maximo sin cambio", a, b);
    snprintf(a, sizeof a, "%lu", v.revisiones); snprintf(b, sizeof b, "%lu", n.revisiones);
    fila("puntos de interrupcion", a, b);
    snprintf(a, sizeof a, "%lu", v.setPixel);  snprintf(b, sizeof b, "%lu", n.setPixel);
    fila("escrituras al framebuffer", a, b);
    snprintf(a, sizeof a, "%lu", v.clearB);    snprintf(b, sizeof b, "%lu", n.clearB);
    fila("bytes clear()", a, b);
    snprintf(a, sizeof a, "%lu", v.setBright); snprintf(b, sizeof b, "%lu", n.setBright);
    fila("setBrightness", a, b);
    snprintf(a, sizeof a, "%.2f ms", v.cpuMs); snprintf(b, sizeof b, "%.2f ms", n.cpuMs);
    fila("CPU modelo M0+", a, b);

    unsigned long peorV = 0, peorN = 0;
    for (unsigned long t = 30; t < CICLO_NUEVO; t += 29) {
        unsigned long x = medir_latencia(animarEnojadoVieja, CICLO_VIEJO, false, t);
        unsigned long y = medir_latencia(animarEnojado,      CICLO_NUEVO, true,  t);
        if (x > peorV) peorV = x;
        if (y > peorN) peorN = y;
    }
    printf("\n  -- reactividad --\n");
    snprintf(a, sizeof a, "%lu ms", peorV); snprintf(b, sizeof b, "%lu ms", peorN);
    fila("PEOR latencia", a, b);

    return 0;
}
