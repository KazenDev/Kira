/**
 * Alegria.cpp - La emocion ALEGRIA (en reposo)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME: una llamada = un frame (~60 fps), el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Antes esta animacion era una SECUENCIA de ~170 sleep() con revisarSerial()
 * entre fases. Eso traia dos problemas que se ven en la placa:
 *
 *   1) LATENCIA: el progreso vivia DENTRO de los sleep(), asi que la animacion
 *      no tenia forma de saber en que punto estaba. La fase "respirar" son
 *      1.29 s de 52 sleep() seguidos SIN un solo revisarSerial() adentro: si
 *      la IA mandaba un comando ahi, la cara no se enteraba hasta 1,3 s
 *      despues. Medido con Bench/bench_alegria.cpp.
 *
 *   2) FLUIDEZ: el brillo subia en 13 escalones de 5 unidades cada 22 ms (una
 *      escalera visible) y el "parpadeo" cerraba el ojo izquierdo, esperaba
 *      240 ms y DESPUES cerraba el derecho: 1,08 s para un parpadeo.
 *
 * Ahora el estado vive en systemTime() y el sleep(16) es solo "esperar al
 * proximo frame". La animacion es una FUNCION PURA DEL TIEMPO: en cualquier
 * instante se puede preguntar como debe verse la cara sin haber ejecutado nada
 * antes. Eso hace que interrumpir sea gratis (el frame siguiente ya calcula el
 * estado nuevo) y que la curva sea continua en vez de una escalera.
 *
 * Mismo patron que ya usa el metronomo (metroFrame()): por eso el cambio se
 * siente consistente con el resto de la firmware y no es una excepcion.
 *
 * LA CARA SE DIBUJA UNA VEZ. Antes se repintaba en cada pasada (un clear() de
 * 50 bytes + 7 pixeles, 6 veces por segundo) y eso era lo que hacia que la
 * animacion se "auto-reparara" si otra cosa habia escrito en la pantalla. Ahora
 * eso se conserva con caraIntacta(): se leen los 7 pixeles de la cara (7
 * lecturas de un byte, ~1 us) y si no coinciden con lo que escribimos, se
 * repinta. Cuesta nada y deja el comportamiento viejo intacto.
 *
 * El ciclo completo son 6148 ms: el MISMO de antes, para que el cambio sea
 * solo de fluidez y no de guion.
 */
#include "Alegria.h"
// Ruta relativa: Alegria.cpp esta en Animaciones/Emociones/Alegria/
// Sistema.h esta en Animaciones/Sistema/ -> ../../Sistema/Sistema.h
#include "../../Sistema/Sistema.h"
// cosf() para las curvas. Se incluye explicito aunque CODAL ya arrastre math.h
// por otros lados: depender de eso es justo lo que rompio el bench de host.
#include <math.h>

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Sonrisa COMPLETA: esquinas (0,3),(4,3) + curva inferior (1,4),(2,4),(3,4)
static const uint8_t BOCA[5][2] = {
    {0,3}, {4,3}, {1,4}, {2,4}, {3,4}
};

// ---------------------------------------------------------------------------
// EL CICLO COMO TABLA
//
// El guion son 7 filas de datos, no 7 funciones que se llaman en orden.
// Agregar una fase nueva es agregar una fila. Los tiempos son los del guion
// original para que el cambio sea solo de fluidez.
// ---------------------------------------------------------------------------
enum Curva
{
    C_PLANO = 0,   // todo quieto
    C_RESP,        // respiro: brillo 60->120->60, un ciclo
    C_RESP2,       // respiro doble
    C_PARPADEO,    // los DOS ojos a la vez
    C_GUINO        // solo el ojo izquierdo
};

struct Segmento
{
    unsigned short desde;    // ms dentro del ciclo
    unsigned short hasta;
    unsigned char  curva;
};

static const Segmento CICLO[] = {
    {   0, 1294, C_RESP2    },  // respira (2 veces, como antes)
    {1294, 2374, C_PARPADEO  },  // parpadeo
    {2374, 2624, C_PLANO    },  // pausa
    {2624, 3704, C_PARPADEO  },  // parpadeo dos veces
    {3704, 3904, C_PLANO    },  // pausa
    {3904, 5198, C_RESP2    },  // respira otra vez
    {5198, 6148, C_GUINO    },  // guino
};
static const int NCICLOS = sizeof(CICLO) / sizeof(CICLO[0]);
static const unsigned short CICLO_MS = 6148;

// ---------------------------------------------------------------------------
// Estado: el reloj mas lo ultimo que escribimos (para no escribir de mas)
// Son 5 variables: la tabla vive en FLASH, no en RAM.
// ---------------------------------------------------------------------------
static unsigned long faseBase = 0;      // systemTime() en el inicio del ciclo
static bool          baseDibujada = false;
static int           ultimoOjoIzq = -1;
static int           ultimoOjoDer = -1;
static int           ultimoBrillo = -1;

// ---------------------------------------------------------------------------
// Las curvas
// ---------------------------------------------------------------------------

// Cierre de un ojo dentro de su segmento: 0 = abierto, 1 = cerrado.
static float cierreOjo(unsigned char curva, float p)
{
    switch (curva)
    {
        case C_PARPADEO:
            // Cierra en el primer 22% del segmento y abre en el ultimo 22%,
            // y los DOS ojos usan el MISMO numero: a la vez. El original los
            // cerraba en serie (el derecho arrancaba 240 ms despues del
            // izquierdo) y por eso un parpadeo tardaba mas de un segundo.
            if (p < 0.22f) return p / 0.22f;
            if (p > 0.78f) return (1.0f - p) / 0.22f;
            return 1.0f;

        case C_GUINO:
            if (p < 0.36f) return p / 0.36f;
            if (p > 0.80f) return (1.0f - p) / 0.20f;
            return 1.0f;

        default:
            return 0.0f;
    }
}

// Brillo global del respiro: un seno entero, continuo, en el mismo rango
// 60..120 que usaba el original. Se evalua por frame, asi que no hay
// escalones: el valor de cada frame es el que corresponde a ese instante.
static int brilloGlobal(unsigned char curva, float p)
{
    float s = 0.5f - 0.5f * cosf(6.2831853f * p);
    float rango = (curva == C_RESP) ? 1.0f : 2.0f;
    return 60 + (int)(s * 30.0f * rango);
}

// ---------------------------------------------------------------------------
// La cara base
// ---------------------------------------------------------------------------

// Dibuja la cara en reposo (ojos abiertos + sonrisa) y la marca como nuestra.
// NO toca el reloj del ciclo: si se repinta porque otra cosa escribio en la
// pantalla, la animacion sigue donde estaba.
static void dibujarBase()
{
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], 255);
    uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 5; i++)
        uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 255);

    ultimoOjoIzq = 255;
    ultimoOjoDer = 255;
    // No pisamos el brillo: se lee el que haya (puede venir de una transicion).
    // brightness es protected en CODAL, asi que se pregunta con el getter.
    ultimoBrillo = uBit.display.getBrightness();
    baseDibujada = true;
}

// ¿La cara sigue siendo la nuestra? Son 7 lecturas de un byte (~1 us), asi que
// se puede pagar en cada frame. Antes esto lo resolvia repintando la cara
// entera en cada pasada; ahora se comprueba y solo se repinta si hace falta,
// que es lo que pasaba cuando un loading se detenia o un comando desconocido
// imprimia "?" en la pantalla.
static bool caraIntacta()
{
    if (uBit.display.image.getPixelValue(OJO_IZQ[0], OJO_IZQ[1]) != ultimoOjoIzq)
        return false;
    if (uBit.display.image.getPixelValue(OJO_DER[0], OJO_DER[1]) != ultimoOjoDer)
        return false;
    for (int i = 0; i < 5; i++)
        if (uBit.display.image.getPixelValue(BOCA[i][0], BOCA[i][1]) != 255)
            return false;
    return true;
}

// ---------------------------------------------------------------------------
// El primer frame, para las TRANSICIONES y para CALLA.
// Dibuja la cara en reposo y ancla el ciclo en este instante: la respiracion
// arranca desde aca, no desde un tiempo de compilacion que en la placa no existe.
// ---------------------------------------------------------------------------
void mostrarCaraAlegria()
{
    uBit.display.setBrightness(90);
    dibujarBase();
    ultimoBrillo = 90;
    faseBase = uBit.systemTime();
}

// ---------------------------------------------------------------------------
// UN FRAME DE LA ANIMACION. La llama el bucle principal ~60 veces por segundo.
// ---------------------------------------------------------------------------
void animarAlegria()
{
    // Si llego un comando serial, NO dibuja encima: deja que el bucle
    // principal procese la nueva emocion en su siguiente pasada. Con el
    // patron de frame esto se nota en el frame siguiente (~16 ms), no en
    // 1,3 s como antes.
    if (revisarSerial()) return;

    // La primera vez (o si otra cosa toco la pantalla) se dibuja la base.
    if (!baseDibujada || !caraIntacta())
        dibujarBase();

    unsigned long ahora = uBit.systemTime();
    unsigned short t = (unsigned short)((ahora - faseBase) % CICLO_MS);

    // Localiza el segmento. Son 7 entradas: busqueda lineal es mas rapida que
    // un arbol y no gasta RAM.
    const Segmento *seg = &CICLO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++) {
        if (t >= CICLO[i].desde && t < CICLO[i].hasta) { seg = &CICLO[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    // --- Ojos: un solo numero de cierre para los dos (o solo el izquierdo,
    //     que es lo que hace el guino). ---
    int cierre = (int)(cierreOjo(seg->curva, p) * 255.0f);
    int ojoIzq = 255 - (seg->curva == C_GUINO ? cierre : 0);
    int ojoDer = 255 - cierre;

    if (ojoIzq != ultimoOjoIzq) {
        uBit.display.image.setPixelValue(OJO_IZQ[0], OJO_IZQ[1], (uint8_t)ojoIzq);
        ultimoOjoIzq = ojoIzq;
    }
    if (ojoDer != ultimoOjoDer) {
        uBit.display.image.setPixelValue(OJO_DER[0], OJO_DER[1], (uint8_t)ojoDer);
        ultimoOjoDer = ojoDer;
    }

    // --- Brillo global del respiro ---
    int brillo = (seg->curva == C_PLANO) ? 90 : brilloGlobal(seg->curva, p);

    // Zona muerta: el quantum del PWM es (timerPeriod*brillo)/(256*255), asi
    // que 1-2 unidades de brillo no cambian nada visible. Y setBrightness()
    // cuesta una division entera POR SOFTWARE en el M0+ (no tiene division por
    // hardware). No se paga esa division por un valor que nadie ve.
    if (brillo - ultimoBrillo >= 3 || ultimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        ultimoBrillo = brillo;
    }

    uBit.sleep(16);   // ~60 fps, el mismo presupuesto que metroFrame()
}
