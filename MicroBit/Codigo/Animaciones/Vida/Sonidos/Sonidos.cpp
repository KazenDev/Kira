/**
 * Sonidos.cpp - Toca los sonidos del catalogo desde su propia fibra
 */
#include "Sonidos.h"
#include "MicroBit.h"
#include "../Nucleo/Catalogo.h"
#include "../Nucleo/SonidoId.h"
#include "../../Sistema/Escuchar/Escuchar.h"
#include "../../Sistema/Metronomo/Metronomo.h"

extern MicroBit uBit;

#define GESTO_ENFRIAR_MS 6000

static bool silenciado = false;
// Al maximo: el parlante del micro:bit es chiquito y en una feria hay ruido
static int volumen = 255;
static bool fibraViva = false;

// El pedido pendiente (uno solo: si llegan dos seguidos gana el ultimo)
static volatile uint8_t pedido = S_NADA;
static uint32_t finSonando = 0;      // cuando termina lo que esta sonando
static uint32_t ultimoGesto = 0;

static char codigo[MAX_CODIGO];

// Duracion REAL de un sonido codificado: la suma del campo de duracion
// [9-12] de cada segmento (las melodias duran 5 s: el oido no tiene que
// escucharlas como si fuera gente)
static uint32_t duracionCodigo(const char *c, int n)
{
    uint32_t total = 0;
    for (int i = 0; i + CHARS_NOTA <= n; i += CHARS_NOTA + 1) {
        uint32_t d = 0;
        for (int k = 9; k < 13; k++) d = d * 10 + (uint32_t)(c[i + k] - '0');
        total += d;
    }
    return total;
}

static float azar01()
{
    return (float)uBit.random(10000) / 10000.0f;
}

static bool permitido()
{
    return !silenciado && !modoEscuchar && !modoMetro;
}

static void fibraSonidos()
{
    while (1) {
        uint8_t id = pedido;
        if (id != S_NADA) {
            pedido = S_NADA;
            if (permitido()) {
                int n = catalogoCodificar(id, azar01(), azar01(), azar01(),
                                          codigo, sizeof(codigo));
                if (n > 0) {
                    uint32_t ahora = uBit.systemTime();
                    // Si algo sigue sonando, se corta (play() esperaria a
                    // que termine). El corte tarda un buffer de audio.
                    if ((int32_t)(finSonando - ahora) > 0)
                        uBit.audio.soundExpressions.stop();
                    uBit.audio.requestActivation();
                    uBit.audio.setSpeakerEnabled(true);
                    uBit.audio.setVolume(volumen);
                    uBit.audio.soundExpressions.playAsync(ManagedString(codigo));
                    finSonando = uBit.systemTime() + duracionCodigo(codigo, n) + 150;
                }
            }
        }
        fiber_sleep(10);
    }
}

void sonidosIniciar()
{
    if (fibraViva) return;
    fibraViva = true;
    create_fiber(fibraSonidos);
}

void sonidoTocar(uint8_t id)
{
    if (id == S_NADA || id >= NUM_SONIDOS) return;
    if (!permitido()) return;
    if (!fibraViva) sonidosIniciar();
    pedido = id;
}

void sonidoGesto(uint8_t id)
{
    uint32_t ahora = uBit.systemTime();
    if (ultimoGesto != 0 && ahora - ultimoGesto < GESTO_ENFRIAR_MS) return;
    ultimoGesto = ahora;
    sonidoTocar(id);
}

void sonidoCallar()
{
    pedido = S_NADA;
    uBit.audio.soundExpressions.stop();
    finSonando = 0;
}

void sonidoSilenciar(bool silencio)
{
    silenciado = silencio;
    if (silencio) sonidoCallar();
}

bool sonidoSilenciado()
{
    return silenciado;
}

bool sonidoSonando()
{
    return pedido != S_NADA || (int32_t)(finSonando - uBit.systemTime()) > 0;
}

void sonidoVolumen(int v)
{
    if (v < 0) v = 0;
    if (v > 255) v = 255;
    volumen = v;
}
