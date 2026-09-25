/**
 * Fastidio.cpp - La emocion FASTIDIO 😤 (en reposo) - "ya me canse de esto"
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME: una llamada = un frame (~60 fps), el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Antes era una SECUENCIA de ~35 sleep() con 5 checkpoints en 1,6 segundos. La
 * fase "respira fastidiada" son 700 ms SIN un solo revisarSerial(), y es la
 * mayor proporcion de un ciclo que la placa pasa sorda. Medido con
 * Bench/bench_emociones.cpp: 650 ms de peor latencia.
 *
 * Ahora el estado vive en systemTime(): la animacion es una funcion pura del
 * tiempo y el comando de la IA se nota en el frame siguiente. Sin azar: todo
 * sale de la tabla de segmentos y de la fase.
 *
 * ── LOS GESTOS YA ESTABAN BIEN, Y HAY RESPALDO ──────────────────────────
 * No se toco ni un tiempo:
 *
 * 1) LA ASIMETRIA ESTA EN LOS OJOS, NO EN LA BOCA. Los ojos van en (0,1) y
 *    (3,1): NO son espejados a proposito, es el "mirar de reojo". Y es el
 *    gesto clasico del desprecio: "rolling one's eyes is a classic gesture
 *    associated with contemptuous behavior", y el desprecio entero se
 *    define por asimetria ("a half-smirk, one side raised... subtle and
 *    asymmetric"). Un fastidio de cara perfectamente simetrica se veria
 *    robotico, que es justo lo que estamos esquivando.
 *
 * 2) LA CEJA QUE TIEMBLA es un build lento. Las guias de animacion de
 *    fastidio/enfado proponen justo eso: "es un build lento mientras la
 *    persona escucha y la realizacion se le instala poco a poco", frente al
 *    "se llena de golpe de fastidio". Y la ceja tiembla por el BORDE
 *    INTERNO, que es el que se arruga al apretar.
 *
 * 3) EL PARPADEO DURO (cierre instantaneo, 130 ms cerrado) es un "full blink
 *    de enfasis": las guias lo describen como "reset, emphasis" y el
 *    "tight blink/squint" como determination. De ahi el "aguanta la calma".
 *
 * 4) EL "TSK" es una microexpresion fugaz. Las guias dicen que el desprecio
 *    "se manifiesta como una microexpresion fugaz, de una fraccion de
 *    segundo", y que el labio rizado es el canal del desprecio. Con un solo
 *    pixel de 5x5 no hay mas que eso, y es suficiente.
 */
#include "Fastidio.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
//
//     # # . # #     <- cejas APRETADAS (4 pixeles; el (2,0) queda libre)
//     # . . # .     <- ojos DESPLAZADOS hacia afuera (mirando de reojo)
//     . . . . .
//     . # # # .     <- boca recta
//     . . . . .
//
// OJO: los ojos NO son espejados. (0,1) y (3,1) a proposito: esa asimetria
// es el "mirar de reojo" y es lo que hace que se lea fastidio y no "mirando
// a dos lados". Ver el comentario del encabezado.
// ---------------------------------------------------------------------------
static const uint8_t CEJA_IZQ[2] = {0, 0};      // extremo izq (fijo)
static const uint8_t CEJA_IZQ_IN[2] = {1, 0};   // borde interno (TIEMBLA)
static const uint8_t CEJA_DER[2] = {4, 0};      // extremo der (fijo)
static const uint8_t CEJA_DER_IN[2] = {3, 0};   // borde interno (TIEMBLA)

static const uint8_t OJO_IZQ[2] = {0, 1};
static const uint8_t OJO_DER[2] = {3, 1};

static const uint8_t BOCA[3][2] = {
    {1, 3}, {2, 3}, {3, 3}
};

// El centro del labio es el que hace el "tsk".
#define TSK_X 2
#define TSK_Y 3

#define BRILLO_REPOSO 90

// ---------------------------------------------------------------------------
// EL CICLO COMO TABLA. 700 + 360 + 240 + 320 = 1620 ms.
// ---------------------------------------------------------------------------
enum Curva
{
    C_PLANO = 0,   // todo quieto
    C_RESPIRA,     // respira FASTIDIADA (el suspiro del que aguanta el mal humor)
    C_CEJA,        // el borde interno de las cejas tiembla
    C_PARPADEO,    // los ojos se cierran DE GOLPE un instante
    C_TSK          // el centro del labio pulsa
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  curva;
};

static const Segmento CICLO[] = {
    {   0,  700, C_RESPIRA  },
    { 700, 1060, C_CEJA     },
    {1060, 1300, C_PARPADEO },
    {1300, 1620, C_TSK      },
};
static const int NCICLOS = sizeof(CICLO) / sizeof(CICLO[0]);
static const unsigned short CICLO_MS = 1620;

// Proporciones internas (de los tiempos del original)
//   respira  : 275 arriba, 275 abajo, 150 quieto   (de 700)
//   ceja     : 3 pulsos de 60 a 80 / 60 a 255     (de 360)
//   parpadeo : 130 cerrado, 110 abierto            (de 240)
//   tsk      : 2 pulsos de 80 a 90 / 80 a 255     (de 320)
#define RESP_SUBE 0.3929f
#define RESP_BAJA 0.7857f
#define CEJA_PASO 0.1667f     // 60 ms de 360
#define PARP_CIERRA 0.5417f    // 130 ms de 240
#define TSK_PASO   0.25f       // 80 ms de 320

// ---------------------------------------------------------------------------
// Estado: el reloj, el ciclo en curso y lo ultimo escrito. Se rastrean LOS
// 25 PIXELS (esta cara usa 9: 4 de ceja + 2 de ojos + 3 de boca).
// ---------------------------------------------------------------------------
static unsigned long faseBase = 0;
static unsigned long ultimoCiclo = 0;
static bool          baseDibujada = false;

static uint8_t esperado[25];
static int     ultimoBrillo = -1;
static int     ultimoCejaIzq = -1;
static int     ultimoCejaDer = -1;
static int     ultimoOjoIzq = -1;
static int     ultimoOjoDer = -1;
static int     ultimoTsk = -1;

// ---------------------------------------------------------------------------
// Escribe un pixel Y registra lo que se escribio, para que caraIntacta()
// pueda compararlo despues. Todo dibujo pasa por aca.
// ---------------------------------------------------------------------------
static void setPixelTrazado(int x, int y, int v)
{
    if (x < 0 || x > 4 || y < 0 || y > 4) return;
    if (uBit.display.image.setPixelValue(x, y, (uint8_t)v) == 0)
        esperado[y * 5 + x] = (uint8_t)v;
}

// ---------------------------------------------------------------------------
// La cara base
// ---------------------------------------------------------------------------
static void dibujarBase()
{
    uBit.display.image.clear();
    for (int i = 0; i < 25; i++) esperado[i] = 0;

    setPixelTrazado(CEJA_IZQ[0], CEJA_IZQ[1], 255);
    setPixelTrazado(CEJA_IZQ_IN[0], CEJA_IZQ_IN[1], 255);
    setPixelTrazado(CEJA_DER[0], CEJA_DER[1], 255);
    setPixelTrazado(CEJA_DER_IN[0], CEJA_DER_IN[1], 255);
    setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], 255);
    setPixelTrazado(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 3; i++)
        setPixelTrazado(BOCA[i][0], BOCA[i][1], 255);

    ultimoCejaIzq = ultimoCejaDer = 255;
    ultimoOjoIzq = ultimoOjoDer = 255;
    ultimoTsk = 255;
    baseDibujada = true;
}

// ¿La cara sigue siendo la nuestra? 25 lecturas de un byte (~1 us).
static bool caraIntacta()
{
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            if (uBit.display.image.getPixelValue(x, y) != esperado[y * 5 + x])
                return false;
    return true;
}

// ---------------------------------------------------------------------------
// Las curvas
// ---------------------------------------------------------------------------

// Respira FASTIDIADA: rampa lineal medio-fuerte (70 <-> 130) y 150 ms quieto.
// El "suspiro" del que aguanta el mal humor.
static int brilhoRespira(float p)
{
    if (p < RESP_SUBE) return 70 + (int)((130 - 70) * p / RESP_SUBE);
    if (p < RESP_BAJA) return 130 - (int)((130 - 70) * (p - RESP_SUBE)
                                          / (RESP_BAJA - RESP_SUBE));
    return BRILLO_REPOSO;
}

// La ceja TIEMBLA: 3 pulsos de 60 ms a 80 (arrugada) y 60 ms a 255. Se queda
// por ESCALONES a proposito: una ceja que titila suave se lee como un bug de
// render; el escalon duro es la irritacion que sube.
static int cejaTiembla(float p)
{
    return (((int)(p / CEJA_PASO)) & 1) ? 255 : 80;
}

// El parpadeo DURO: cierre instantaneo, 130 ms cerrado, y abre.
static bool ojosDuros(float p)
{
    return p < PARP_CIERRA;
}

// El "tsk": el centro del labio pulsa 90 <-> 255, dos veces.
static int valorTsk(float p)
{
    return (((int)(p / TSK_PASO)) & 1) ? 255 : 90;
}

// ---------------------------------------------------------------------------
// El primer frame, para las TRANSICIONES y CALLA. Ancla el ciclo.
// ---------------------------------------------------------------------------
void mostrarCaraFastidio()
{
    uBit.display.setBrightness(BRILLO_REPOSO);
    dibujarBase();
    ultimoBrillo = BRILLO_REPOSO;
    faseBase = uBit.systemTime();
    ultimoCiclo = 0;
}

// ---------------------------------------------------------------------------
// UN FRAME. La llama el bucle principal ~60 veces por segundo.
// ---------------------------------------------------------------------------
void animarFastidio()
{
    if (revisarSerial()) return;

    if (!baseDibujada || !caraIntacta())
        dibujarBase();

    unsigned long ahora = uBit.systemTime();
    unsigned long transcurrido = ahora - faseBase;
    unsigned long ciclo = transcurrido / CICLO_MS;
    unsigned short t = (unsigned short)(transcurrido % CICLO_MS);

    if (ciclo != ultimoCiclo) {
        ultimoCiclo = ciclo;
        // OJO: el printf de CODAL solo soporta %d y %s (Serial.cpp:425).
        uBit.serial.printf("F%d\n", (int)(ciclo % 1000));
    }

    // Localiza el segmento (4 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &CICLO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++) {
        if (t >= CICLO[i].desde && t < CICLO[i].hasta) { seg = &CICLO[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    // --- Brillo global: solo lo mueve la respiracion -----------------------
    int brillo = (seg->curva == C_RESPIRA) ? brilhoRespira(p) : BRILLO_REPOSO;
    // Zona muerta: el quantum del PWM avanza de a saltos de ~1,22 unidades
    // (quantum = 0.8169 * brillo), asi que un cambio de 1 o 2 no altera ni un
    // ciclo del PWM. Con el deadband, un frame con menos de 3 de diferencia
    // no paga la division entera por software de setBrightness.
    //
    // OJO: el efecto NETO aqui sale al REVES de lo que parece. La rampa
    // original eran 11 escalones de 6 unidades sostenidos 25 ms (22
    // llamadas); la version de frame evalua la curva 60 veces por segundo (32
    // llamadas). Se pagan ~10 llamadas mas, que son 0,01 ms. Es el precio de
    // que la rampa sea continua en vez de una escalera, y sale a cuenta.
    if (brillo - ultimoBrillo >= 3 || ultimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        ultimoBrillo = brillo;
    }

    // --- La ceja que tiembla (solo el BORDE INTERNO) ----------------------
    // Los extremos (0,0) y (4,0) no se tocan nunca: son la ceja apretada fija.
    int ceja = (seg->curva == C_CEJA) ? cejaTiembla(p) : 255;
    if (ceja != ultimoCejaIzq) {
        setPixelTrazado(CEJA_IZQ_IN[0], CEJA_IZQ_IN[1], ceja);
        ultimoCejaIzq = ceja;
    }
    if (ceja != ultimoCejaDer) {
        setPixelTrazado(CEJA_DER_IN[0], CEJA_DER_IN[1], ceja);
        ultimoCejaDer = ceja;
    }

    // --- El parpadeo duro: cierra DE GOLPE, sin rampa --------------------
    int ojos = (seg->curva == C_PARPADEO && ojosDuros(p)) ? 0 : 255;
    if (ojos != ultimoOjoIzq) {
        setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], ojos);
        ultimoOjoIzq = ojos;
    }
    if (ojos != ultimoOjoDer) {
        setPixelTrazado(OJO_DER[0], OJO_DER[1], ojos);
        ultimoOjoDer = ojos;
    }

    // --- El "tsk": solo el centro del labio -------------------------------
    int tsk = (seg->curva == C_TSK) ? valorTsk(p) : 255;
    if (tsk != ultimoTsk) {
        setPixelTrazado(TSK_X, TSK_Y, tsk);
        ultimoTsk = tsk;
    }

    uBit.sleep(16);   // ~60 fps
}
