/**
 * bench_emociones.cpp - BENCH DE HOST de las 8 emociones.
 *
 * Compila el Codigo/ REAL de cada emocion contra el shim del micro:bit y
 * mide lo mismo que en bench_alegria: tiempo de ciclo, fluidez, trabajo y
 * sobre todo la VENTANA MUERTA (cuanto tarda la animacion en enterarse de un
 * comando de la IA).
 *
 * Sirve para dos cosas:
 *   1) Rankear cual de las 8 es la peor (para migrarlas en orden util).
 *   2) Validar cada migracion al patron de frame, una por una.
 *
 * Uso:  ./bench_emociones [ciclo_max_ms]
 */
#include "mock/MicroBit.h"
#include "Animaciones/Sistema/Sistema.h"

// Las 8 emociones reales.
void animarAlegria();     void mostrarCaraAlegria();
void animarTriste();      void mostrarCaraTriste();
void animarEnojado();     void mostrarCaraEnojado();
void animarSorprendido(); void mostrarCaraSorprendido();
void animarNeutral();     void mostrarCaraNeutral();
void animarFastidio();    void mostrarCaraFastidio();
void animarMiedo();       void mostrarCaraMiedo();
void animarCansado();     void mostrarCaraCansado();

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
    uBit.serial.pendiente = "HAPPY\n";
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
void dibujarCaraDestino(EmocionActual d) { (void)d; }

// ---------------------------------------------------------------------------
// Medicion
// ---------------------------------------------------------------------------
typedef void (*AnimFn)();
typedef void (*CaraFn)();

struct Emocion
{
    const char *nombre;
    EmocionActual id;
    AnimFn       animar;
    CaraFn       cara;
    unsigned long cicloMs;   // el ciclo real, si la version es de frame
};

static Emocion EMOCIONES[8] = {
    // cicloMs = el ciclo real de la version con frame. Las que todavia son
    // secuencias lo dejan en 0: su ciclo no tiene tope fijo, asi que el bench
    // las corre hasta maxMs y las marca como tales.
    { "ALEGRIA",     EM_ALEGRIA,     animarAlegria,     mostrarCaraAlegria,     6148 },
    { "TRISTE",      EM_TRISTE,      animarTriste,      mostrarCaraTriste,      6760 },
    { "ENOJADO",     EM_ENOJADO,     animarEnojado,     mostrarCaraEnojado,     4568 },
    { "SORPRENDIDO", EM_SORPRENDIDO, animarSorprendido, mostrarCaraSorprendido, 2630 },
    { "NEUTRAL",     EM_NEUTRAL,     animarNeutral,     mostrarCaraNeutral,     3880 },
    { "FASTIDIO",    EM_FASTIDIO,    animarFastidio,    mostrarCaraFastidio,    1620 },
    { "MIEDO",       EM_MIEDO,       animarMiedo,       mostrarCaraMiedo,       2210 },
    { "CANSADO",     EM_CANSADO,     animarCansado,     mostrarCaraCansado,     4330 },
};

static bool g_esFrame;      // la version nueva es funcion de frame
static unsigned long g_objetivoMs;

static void correr(AnimFn fn)
{
    unsigned long t0 = bench.nowMs;
    if (g_esFrame) { while (bench.nowMs - t0 < g_objetivoMs) fn(); }
    else fn();
}

struct Resultado
{
    unsigned long ms, frames, revisiones, gapMax;
    unsigned long setPixel, clearB, setBright, sleeps;
    unsigned long peorLatencia;
    double        fps;
};

static Resultado medir(Emocion &e, unsigned long objetivoMs, bool esFrame)
{
    g_esFrame = esFrame;
    g_objetivoMs = objetivoMs;

    bench_reset();
    bench_inyectarEnMs = 0;

    emocionActual = e.id;
    uBit.display.image.clear();
    e.cara();                 // la cara en reposo de esta emocion
    correr(e.animar);

    Resultado r;
    r.ms = bench.nowMs;
    r.frames = bench.frames;
    r.revisiones = bench_revisiones;
    r.gapMax = bench.peorGapMs;
    r.setPixel = bench.setPixel;
    r.clearB = bench.bytesCleared;
    r.setBright = bench.setBrightness;
    r.sleeps = bench.sleeps;
    r.fps = r.ms ? (double)bench.frames * 1000.0 / r.ms : 0;
    r.peorLatencia = 0;
    return r;
}

static Resultado medir_latencia(Emocion &e, unsigned long objetivoMs,
                                bool esFrame, unsigned long inyectarEnMs)
{
    g_esFrame = esFrame;
    g_objetivoMs = objetivoMs;

    bench_reset();
    bench_inyectarEnMs = inyectarEnMs;

    emocionActual = e.id;
    uBit.display.image.clear();
    e.cara();
    correr(e.animar);
    revisarSerial();          // el bucle principal vuelve a mirar al salir

    Resultado r;
    r.peorLatencia = bench_latenciaMs;
    return r;
}

int main(int argc, char **argv)
{
    unsigned long maxMs = (argc > 1) ? strtoul(argv[1], 0, 10) : 12000;

    printf("micro:bit bench - las 8 emociones (codigo real, host, modelo M0+)\n");
    printf("'frame' = la version es funcion de frame (ya migrada). Las que no lo\n");
    printf("son son SECUENCIAS DE SLEEP: su ciclo no tiene un tope fijo, asi que\n");
    printf("se las corta al maxMs para poder compararlas.\n");
    printf("'VENTANA MUERTA' = peor caso de un comando que llega en el instante\n");
    printf("mas ciego del ciclo. Es lo que siente la persona.\n\n");

    printf("  %-12s %-7s %8s %7s %7s %8s %9s %9s\n",
           "emocion", "patron", "ciclo", "fps", "checks", "sleeps", "escrituras", "VENTANA MUERTA");
    printf("  %-12s %-7s %8s %7s %7s %8s %9s %9s\n",
           "------------", "-------", "--------", "-------", "-------",
           "--------", "---------", "-----------");

    unsigned long peorVentana = 0;
    const char *peorNombre = "";
    Emocion *peorEmocion = 0;

    for (int i = 0; i < 8; i++) {
        Emocion &e = EMOCIONES[i];

        // Alegría ya esta migrada; el resto todavia no. Se detecta por la
        // firma: la version migrada vuelve en ~16 ms, la secuencia no.
        // Se detecta si ya es funcion de frame mirando si una llamada corta
        // vuelve enseguida (<60 ms) en vez de quedarse dormida todo el ciclo.
        Resultado p = medir(e, 200, false);
        bool esFrame = e.cicloMs > 0 && p.ms < 60;
        unsigned long ciclo = esFrame ? e.cicloMs : maxMs;

        Resultado r = medir(e, ciclo, esFrame);

        // Barrido de inyeccion para la peor latencia.
        unsigned long peor = 0;
        unsigned long paso = r.ms / 40;
        if (paso < 5) paso = 5;
        for (unsigned long t = paso; t < r.ms; t += paso) {
            Resultado x = medir_latencia(e, esFrame ? ciclo : maxMs, esFrame, t);
            if (x.peorLatencia > peor) peor = x.peorLatencia;
        }

        char patron[8];
        snprintf(patron, sizeof patron, "%s", esFrame ? "frame" : "secuencia");
        printf("  %-12s %-7s %6lu ms %7.1f %7lu %8lu %9lu %7lu ms\n",
               e.nombre, patron, r.ms, r.fps, r.revisiones, r.sleeps,
               r.setPixel, peor);

        if (peor > peorVentana) { peorVentana = peor; peorNombre = e.nombre; peorEmocion = &e; }    }

    printf("\n  peor ventana muerta: %s, %lu ms\n", peorNombre, peorVentana);
    if (peorEmocion && peorVentana > 100)
        printf("  -> esa es la que conviene migrar primero\n");

    return 0;
}
