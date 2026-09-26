/**
 * bench_bocas.cpp - BENCH DE HOST de las bocas de TALK (los Hablar*.cpp).
 *
 * Las 8 bocas se evaporated del analisis de las emociones: usan fiber_sleep()
 * en vez de uBit.sleep(), asi que un grep de "sleep" no las encuentra. Y en
 * Principal.cpp el TALK tiene PRIORIDAD sobre la animacion de la emocion, asi
 * que mientras la IA habla, la boca es TODO lo que corre en el hilo principal.
 *
 * Corre el Codigo/ REAL de las 8 bocas. Para la que se este migrando, el
 * bench la compara contra la version anterior (sacada con git show).
 *
 * Lo que mide:
 *   1) Cuanto dura UNA llamada a la boca (una pasada del bucle principal).
 *   2) La ventana sorda: un comando que llega en el instante mas ciego.
 *   3) Cuantos checkpoints de serial tiene.
 *
 * Para correrla:  sh Bench/run_bocas.sh
 */
#include "mock/MicroBit.h"
#include "Animaciones/Sistema/Sistema.h"

// Las 8 bocas de TALK (codigo real, sin tocar).
void animarBocaHablando();
void animarBocaTriste();
void animarBocaEnojada();
void animarBocaSorprendida();
void animarBocaNeutral();
void animarBocaFastidio();
void animarBocaMiedo();
void animarBocaCansado();

// Las versiones ANTERIORES de las bocas ya migradas, para el A/B.
void animarBocaHablandoVieja();
void animarBocaTristeVieja();
void animarBocaCansadoVieja();
void animarBocaMiedoVieja();
void animarBocaFastidioVieja();
void animarBocaNeutralVieja();
void animarBocaSorprendidaVieja();
void animarBocaEnojadaVieja();
// OJO: en la firmware real TODAS las bocas comparten el MISMO modoHablar
// (viene de Alegria/Hablar.h). Asique la version vieja de Alegria define su
// propia bandera (modoHablarVieja, para no chocar) y la de Triste usa la real.
extern bool modoHablarVieja;

// El estado que los Hablar comparten. SOLO Alegria/Hablar.cpp lo define.
extern bool modoHablar;

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

void bench_tick_cycles(double c) { bench.cycles += (unsigned long)c; }

void bench_notificar_frame()
{
    if (bench.frames > 0) {
        unsigned long g = bench.nowMs - bench.ultimoFrameMs;
        if (g > bench.peorGapMs) bench.peorGapMs = g;
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
    ManagedString l = uBit.serial.readUntil(DELIMITADOR, ASYNC);
    if (l.length() > 0) {
        bench_latenciaMs = bench.nowMs - bench_inyectadoEnMs;
        if (bench_latenciaMs > bench_peorLatenciaMs) bench_peorLatenciaMs = bench_latenciaMs;
        return true;
    }
    bench_ultimaRevisionMs = bench.nowMs;
    return false;
}

bool bleColaSacar(ManagedString &l) { (void)l; return false; }
void procesarComando(ManagedString c) { (void)c; }
void demoAutomatica() {}
void bleEnviar(ManagedString t) { (void)t; }
void dibujarCaraDestino(EmocionActual d) { (void)d; }

// --- Las fibras ------------------------------------------------------------
// fiber_sleep() REALMENTE cede la CPU a la otra fibra (CodalFiber.cpp:322), y
// durante esos milisegundos el bucle principal NO puede mirar el serial. Eso
// es justo lo que hay que medir, asi que aca avanza el reloj virtual.
//
// Dato clave para el diseno: uBit.sleep(ms) ES fiber_sleep(ms)
// (CodalDevice.cpp:30), asi que la version nueva usa la misma llamada.
void fiber_sleep(unsigned long ms)
{
    bench.sleeps++;
    bench.sleptMs += ms;
    bench.nowMs += ms;
    bench_tick_cycles(BENCH_CYCLES_SLEEP);
    bench_inyectar_si_toca();
}

// La fibra de los parpadeos NO se corre en el bench: no procesa comandos, y
// correrla en paralelo exigiria un scheduler completo. Lo que importa es la
// funcion de la boca, que corre en el hilo principal.
void create_fiber(FiberFn fn) { (void)fn; bench.fibras++; }
void release_fiber() {}

// ---------------------------------------------------------------------------
typedef void (*BocaFn)();

struct Boca { const char *nombre; BocaFn fn; };

static Boca BOCAS[8] = {
    { "ALEGRIA",     animarBocaHablando     },
    { "TRISTE",      animarBocaTriste       },
    { "ENOJADO",     animarBocaEnojada      },
    { "SORPRENDIDO", animarBocaSorprendida  },
    { "NEUTRAL",     animarBocaNeutral      },
    { "FASTIDIO",    animarBocaFastidio     },
    { "MIEDO",       animarBocaMiedo        },
    { "CANSADO",     animarBocaCansado      },
};

// Corre la boca `fn` como el bucle principal lo haria, y devuelve el peor
// caso de un comando que llega en el instante mas ciego.
// Las bocas viejas NO comparten flag entre si en el bench: la de Alegria
// define el suyo (para no chocar con la nueva), y las de Triste y Cansado usan
// el modoHablar real, como en la firmware.
enum Vieja { V_ALEGRIA = 0, V_TRITE, V_CANSADO, V_MIEDO, V_FASTIDIO, V_NEUTRAL,
             V_SORPRENDIDO, V_ENOJADO };
static Vieja g_vieja = V_ALEGRIA;

static void medir(const char *titulo, BocaFn fn, bool usarFlagViejo)
{
    unsigned long pasada, checks, sorda;

    // (1) una pasada limpia: cuanto dura
    bench_reset();
    bench_inyectarEnMs = 0;
    if (usarFlagViejo) {
        if (g_vieja == V_ALEGRIA) modoHablarVieja = true;
        else                      modoHablar = true;
    } else {
        modoHablar = true;
    }
    fn();
    pasada = bench.nowMs;
    checks = bench_revisiones;

    // (2) el comando entra justo al empezar: ese es el peor caso cuando la
    //     funcion no tiene checkpoints
    bench_reset();
    bench_inyectarEnMs = 1;
    if (usarFlagViejo) {
        if (g_vieja == V_ALEGRIA) modoHablarVieja = true;
        else                      modoHablar = true;
    } else {
        modoHablar = true;
    }
    fn();
    revisarSerial();              // el bucle principal vuelve a mirar
    sorda = bench_latenciaMs;

    printf("  %-24s 1 pasada %6lu ms   checkpoints %lu   VENTANA MUERTA %5lu ms\n",
           titulo, pasada, checks, sorda);
}

int main()
{
    printf("micro:bit bench - las 8 bocas de TALK (codigo real, host, M0+)\n");
    printf("Principal.cpp le da PRIORIDAD a TALK sobre la emocion, asi que\n");
    printf("mientras la IA habla, esto es TODO lo que corre en el hilo principal.\n");

    printf("\n--- A/B de las bocas ya migradas (vieja de git vs. nueva) ---\n");
    g_vieja = V_ALEGRIA;
    medir("ALEGRIA VIEJA", animarBocaHablandoVieja, true);
    medir("ALEGRIA NUEVA (frame)", animarBocaHablando, false);
    g_vieja = V_TRITE;
    medir("TRISTE   VIEJA", animarBocaTristeVieja, true);
    g_vieja = V_CANSADO;
    medir("CANSADO  VIEJA", animarBocaCansadoVieja, true);
    medir("CANSADO  NUEVA (frame)", animarBocaCansado, false);
    g_vieja = V_MIEDO;
    medir("MIEDO    VIEJA", animarBocaMiedoVieja, true);
    medir("MIEDO    NUEVA (frame)", animarBocaMiedo, false);
    g_vieja = V_FASTIDIO;
    medir("FASTIDIO VIEJA", animarBocaFastidioVieja, true);
    medir("FASTIDIO NUEVA (frame)", animarBocaFastidio, false);
    g_vieja = V_NEUTRAL;
    medir("NEUTRAL  VIEJA", animarBocaNeutralVieja, true);
    medir("NEUTRAL  NUEVA (frame)", animarBocaNeutral, false);
    g_vieja = V_SORPRENDIDO;
    medir("SORPREND. VIEJA", animarBocaSorprendidaVieja, true);
    medir("SORPREND. NUEVA (frame)", animarBocaSorprendida, false);
    g_vieja = V_ENOJADO;
    medir("ENOJADO  VIEJA", animarBocaEnojadaVieja, true);
    medir("ENOJADO  NUEVA (frame)", animarBocaEnojada, false);
    g_vieja = V_ALEGRIA;
    medir("TRISTE   NUEVA (frame)", animarBocaTriste, false);

    printf("\n--- las 8 bocas, como estan ahora ---\n");
    for (int i = 0; i < 8; i++)
        medir(BOCAS[i].nombre, BOCAS[i].fn, false);

    return 0;
}
