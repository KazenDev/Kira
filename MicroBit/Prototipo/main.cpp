/**
 * main.cpp - PROTOTIPO A/B: la alegria de verdad vs. la eficiente
 *
 * Este NO es la firmware de Kira. Es un banco de pruebas visual: compila el
 * Codigo/Animaciones/Emociones/Alegria/Alegria.cpp REAL, sin tocarlo, y lo
 * contrasta contra una version eficiente de la misma animacion.
 *
 * Que se ve, sin tocar nada:
 *   - Las dos versiones se alternan solas, un ciclo completo cada una
 *     (6.148 s), para que veas la MISMA cara dos veces seguidas.
 *   - Durante la version ACTUAL se enciende el pixel (0,0) (esquina arriba
 *     a la izquierda, que la cara no usa). Durante la eficiente, no.
 *   - En cada cambio de modo parpadean los ojos: 2 = empieza ACTUAL,
 *     1 = empieza EFICIENTE.
 *
 * Con los botones:
 *   - A  simula que la IA manda otra emocion. La cara pasa a TRISTE 1,2 s y
 *      vuelve. El retraso entre apretar y ver el cambio es la LATENCIA: en
 *      la version actual puede llegar a 1,3 s, en la eficiente es un frame.
 *   - B  cambia de modo al instante.
 *
 * Por serial USB sale una linea en cada cambio, para no perderse.
 */
#include "MicroBit.h"
// CMake agrega al include path cada carpeta que tenga un .h, asi que el stub
// se incluye desde source/ y no desde el relativo de Alegria.cpp.
#include "Animaciones/Sistema/Sistema.h"
#include "Animaciones/Emociones/Alegria/Alegria.h"

#include <math.h>

MicroBit uBit;
EmocionActual emocionActual = EM_ALEGRIA;

// El ciclo real de la alegria, medido con el bench: 6148 ms.
static const unsigned long CICLO_MS = 6148;

enum Modo { MODO_ACTUAL, MODO_OPT };
static Modo modo = MODO_ACTUAL;

// Inyeccion del "comando de la IA": la pone el boton A, la consume
// revisarSerial(). Toda la latencia sale de este unico mecanismo.
static volatile bool comandoLlego = false;
static volatile bool comandoTomado = false;

// ---------------------------------------------------------------------------
// La version eficiente. Mismo ciclo, misma cara, estado en el reloj.
// ---------------------------------------------------------------------------
enum Curva { C_PLANO = 0, C_RESP, C_RESP2, C_PARPADEO, C_GUINO };

struct Segmento { unsigned short desde, hasta; unsigned char curva; };

static const Segmento CICLO[] = {
    {   0, 1294, C_RESP2  },
    {1294, 2374, C_PARPADEO},
    {2374, 2624, C_PLANO  },
    {2624, 3704, C_PARPADEO},
    {3704, 3904, C_PLANO  },
    {3904, 5198, C_RESP2  },
    {5198, 6148, C_GUINO  },
};
static const int NCICLOS = sizeof(CICLO) / sizeof(CICLO[0]);

static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};
static const uint8_t BOCA[5][2] = {{0,3},{4,3},{1,4},{2,4},{3,4}};

static bool optBaseDibujada = false;
static int  optUltimoIzq = -1, optUltimoDer = -1, optUltimoBrillo = -1;
// Desplaza el reloj del ciclo, para poder reencuadrarlo cuando vuelve de la
// cara triste sin tener que dormir 6 s para "terminar" el ciclo anterior.
static unsigned long optFaseBase = 0;

static void reencuadrarCicloOpt()
{
    optFaseBase = uBit.systemTime();
    optBaseDibujada = false;
}

static float cierreOjo(unsigned char curva, float p)
{
    switch (curva) {
        case C_PARPADEO:
            // Los DOS ojos a la vez. El original los cerraba en serie: el
            // derecho arrancaba 240 ms despues del izquierdo.
            if (p < 0.22f) return p / 0.22f;
            if (p > 0.78f) return (1.0f - p) / 0.22f;
            return 1.0f;
        case C_GUINO:
            if (p < 0.36f) return p / 0.36f;
            if (p > 0.80f) return (1.0f - p) / 0.20f;
            return 1.0f;
        default: return 0.0f;
    }
}

static int brilloGlobal(unsigned char curva, float p)
{
    float s = 0.5f - 0.5f * cosf(6.2831853f * p);
    float rango = (curva == C_RESP) ? 1.0f : 2.0f;
    return 60 + (int)(s * 30.0f * rango);
}

static void optBaseUnaVez()
{
    if (optBaseDibujada) return;
    optBaseDibujada = true;
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
    uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 5; i++)
        uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 255);
    optUltimoIzq = optUltimoDer = 255;
    optUltimoBrillo = 90;
}

static void animarAlegriaOpt()
{
    if (revisarSerial()) return;

    unsigned long ahora = uBit.systemTime();
    unsigned short t = (unsigned short)((ahora - optFaseBase) % CICLO_MS);

    const Segmento *seg = &CICLO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++)
        if (t >= CICLO[i].desde && t < CICLO[i].hasta) { seg = &CICLO[i]; break; }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    optBaseUnaVez();

    int cierre = (int)(cierreOjo(seg->curva, p) * 255.0f);
    int ojoIzq = 255 - (seg->curva == C_GUINO ? cierre : 0);
    int ojoDer = 255 - cierre;

    if (ojoIzq != optUltimoIzq) {
        uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], (uint8_t)ojoIzq);
        optUltimoIzq = ojoIzq;
    }
    if (ojoDer != optUltimoDer) {
        uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], (uint8_t)ojoDer);
        optUltimoDer = ojoDer;
    }

    int brillo = (seg->curva == C_PLANO) ? 90 : brilloGlobal(seg->curva, p);
    // Zona muerta: el quantum del PWM no distingue 1-2 unidades de brillo, y
    // setBrightness() cuesta una division entera por software en el M0+.
    if (brillo - optUltimoBrillo >= 3 || optUltimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        optUltimoBrillo = brillo;
    }

    uBit.sleep(16);   // ~60 fps
}

// ---------------------------------------------------------------------------
// La cara triste, para el test de latencia con el boton A
// ---------------------------------------------------------------------------
static void dibujarTriste()
{
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(1, 1, 255);
    uBit.display.image.setPixelValue(3, 1, 255);
    // Ceja de tristeza + boca invertida
    uBit.display.image.setPixelValue(1, 0, 255);
    uBit.display.image.setPixelValue(3, 0, 255);
    uBit.display.image.setPixelValue(0, 1, 255);
    uBit.display.image.setPixelValue(4, 1, 255);
    uBit.display.image.setPixelValue(1, 3, 255);
    uBit.display.image.setPixelValue(2, 4, 255);
    uBit.display.image.setPixelValue(3, 3, 255);
}

// La firma del cambio de modo: N parpadeos de ojos (2 = actual, 1 = opt).
static void firmarModo(int parpadeos)
{
    uBit.display.setBrightness(90);
    for (int i = 0; i < parpadeos; i++) {
        uBit.display.image.setPixelValue(1, 1, 0);
        uBit.display.image.setPixelValue(3, 1, 0);
        uBit.sleep(90);
        uBit.display.image.setPixelValue(1, 1, 255);
        uBit.display.image.setPixelValue(3, 1, 255);
        uBit.sleep(90);
    }
    uBit.sleep(300);
}

// El pixel (0,0) marca que estas viendo la version ACTUAL.
static void indicador()
{
    uBit.display.image.setPixelValue(0, 0, modo == MODO_ACTUAL ? 200 : 0);
}

// ---------------------------------------------------------------------------
// La ruta de comandos del prototipo: solo el boton A inyecta el "comando".
// El retardo entre el pulso y el consumo es EXACTAMENTE lo que hay que ver.
// ---------------------------------------------------------------------------
static bool aPrev = false, bPrev = false;

bool revisarSerial()
{
    bool a = uBit.buttonA.isPressed();
    bool b = uBit.buttonB.isPressed();
    bool pulsoA = a && !aPrev;
    bool pulsoB = b && !bPrev;
    aPrev = a;
    bPrev = b;

    if (pulsoA) comandoLlego = true;
    if (pulsoB) {
        modo = (modo == MODO_ACTUAL) ? MODO_OPT : MODO_ACTUAL;
        reencuadrarCicloOpt();   // que la version opt vuelva a pintar la base
    }

    if (comandoLlego) {
        comandoLlego = false;
        comandoTomado = true;
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------
int main()
{
    uBit.init();
    uBit.display.setBrightness(90);

    unsigned long ultimoCambio = uBit.systemTime();
    optFaseBase = ultimoCambio;
    uBit.serial.send("PROTOTIPO A/B: 2 parpadeos=ACTUAL, 1=EFICIENTE\n");
    uBit.serial.send("A = comando de la IA (test de latencia). B = cambiar de modo.\n");

    while (1) {
        // Cambio de modo automatico: un ciclo completo cada version.
        if (uBit.systemTime() - ultimoCambio >= CICLO_MS) {
            ultimoCambio += CICLO_MS;
            modo = (modo == MODO_ACTUAL) ? MODO_OPT : MODO_ACTUAL;
            reencuadrarCicloOpt();
            firmarModo(modo == MODO_ACTUAL ? 2 : 1);
            uBit.serial.send(modo == MODO_ACTUAL ? "MODO: ACTUAL\n" : "MODO: EFICIENTE\n");
        }

        if (modo == MODO_ACTUAL) {
            animarAlegria();
        } else {
            animarAlegriaOpt();
        }

        if (comandoTomado) {
            comandoTomado = false;
            dibujarTriste();
            uBit.sleep(1200);
            reencuadrarCicloOpt();
            // El comando interrumpio el ciclo: el reloj de modo tiene que
            // arrancar de cero, o el cambio de modo se dispara de inmediato.
            ultimoCambio = uBit.systemTime();
        }

        indicador();
    }
}
