/**
 * Neutral.cpp - La emocion NEUTRAL 😑 (en reposo) - la cara "bruh"
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME: una llamada = un frame (~60 fps), el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Antes era una SECUENCIA de ~79 sleep() con 6 checkpoints en 3,9 segundos.
 * La fase "ojos peek" son 840 ms SIN un solo revisarSerial(), y sale dos
 * veces por pasada, asi que la ventana sorda era de 750 ms. Medido con
 * Bench/bench_emociones.cpp.
 *
 * Ahora el estado vive en systemTime(): la animacion es una funcion pura del
 * tiempo y el comando de la IA se nota en el frame siguiente. Sin azar: todo
 * sale de la tabla de segmentos y de la fase.
 *
 * ── LOS DOS GESTOS YA ESTABAN BIEN DISEÑADOS (y hay respaldo) ───────────
 * No se toco ni un tiempo. Esto es lo que dice la literatura y coincide con
 * lo que el codigo hacia:
 *
 * 1) EL PEEK. Se apagan los parpados EXTERIORES y quedan las pupilas a la
 *    vista, con 420 ms de mirada fija. Las guias de animacion de ojos dicen
 *    que "el parpado superior hace la mayor parte del movimiento y el
 *    inferior la sigue" (aca el parpado ES lo que se mueve y la pupila queda
 *    al descubierto) y que "usar parpadeos a medio cerrar para personajes
 *    informales". Ademas, "una mirada sostenida con pocos parpadeos transmite
 *    concentracion o foco intense": de ahi el hold de 420 ms. Es un medio
 *    parpadeo, no un parpadeo.
 *
 * 2) EL "MEH". Solo se mueve el lado izquierdo del labio: (1,3) baja y (1,2)
 *    sube, mientras la derecha (2,3) y (3,3) quedan quietas. Eso es
 *    ASIMETRIA, y hay evidencia de que funciona: "incluir uno o una
 *    combinacion de movimientos asimetricos de ceja, boca o parpado aumenta
 *    la credibilidad, atractivo y naturalidad percibidos", y "las caras
 *    puramente simetricas pueden haber contribuido a que el personaje virtual
 *    parezca artificial". Un "neutral" simetrico es justo lo que hace que una
 *    cara se vea robotica.
 *
 * OJO: la misma literatura aclara que la asimetria rinde sobre todo en
 * emociones COMPLEJAS/ambivalentes, y que para las basicas conviene la
 * simetria. Un "meh" es justamente una ambivalencia (descontento sin
 * enfaderse), asi que cae en el lado bueno de esa regla.
 */
#include "Neutral.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
//
// Ojos CERRADOS: cada ojo cerrado son 2 pixeles. (0,1) y (4,1) son los
// parpados exteriores; (1,1) y (3,1) son las pupilas que quedan al
// descubierto cuando el parpado se levanta (el "peek").
// NO son cuatro ojos: es cada ojo cerrado de 2 pixeles.
// ---------------------------------------------------------------------------
static const uint8_t PARPADO_IZQ[2] = {0, 1};
static const uint8_t PARPADO_DER[2] = {4, 1};
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Boca RECTA neutra: (1,3)(2,3)(3,3)
static const uint8_t BOCA[3][2] = {
    {1, 3}, {2, 3}, {3, 3}
};

// El "meh": el labio izquierdo (1,3) baja y aparece el de arriba (1,2). Es el
// UNICO pixel de la cara que no existe en reposo: la asimetria del "meh".
#define LABIO_IZQ_X 1
#define LABIO_IZQ_Y 3
#define ARRIBA_IZQ_X 1
#define ARRIBA_IZQ_Y 2

#define BRILLO_REPOSO 85

// ---------------------------------------------------------------------------
// EL CICLO COMO TABLA. 810 + 840 + 810 + 580 + 840 = 3880 ms.
// ---------------------------------------------------------------------------
enum Curva
{
    C_PLANO = 0,   // todo quieto
    C_RESPIRA,     // respira CALMADA (ritmo medio, ni agitada ni lenta)
    C_PEEK,        // los parpados se levantan, queda la pupila, y vuelven
    C_RETO         // el "meh": solo el lado izquierdo del labio se mueve
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  curva;
};

static const Segmento CICLO[] = {
    {   0,  810, C_RESPIRA },
    { 810, 1650, C_PEEK    },
    {1650, 2460, C_RESPIRA },
    {2460, 3040, C_RETO    },
    {3040, 3880, C_PEEK    },
};
static const int NCICLOS = sizeof(CICLO) / sizeof(CICLO[0]);
static const unsigned short CICLO_MS = 3880;

// Proporciones internas (de los tiempos del original)
//   respira : 330 arriba, 330 abajo, 150 quieto   (de 810)
//   peek    : 150 abre, 420 a la vista, 150 cierra, 120 quieto (de 840)
//   reto    : 140 sube, 300 quieto, 140 baja       (de 580)
#define RESP_SUBE 0.4074f
#define RESP_BAJA 0.8148f
#define PEEK_ABRE 0.1786f
#define PEEK_MIRA 0.6786f
#define PEEK_CIERRA 0.8571f
#define RETO_SUBE 0.2414f
#define RETO_BAJA 0.7586f

// ---------------------------------------------------------------------------
// Estado: el reloj, el ciclo en curso y lo ultimo escrito. Se rastrean LOS
// 25 PIXELS (esta cara usa 8: 2 parpados + 2 pupilas + 3 de boca + el de
// arriba del "meh").
// ---------------------------------------------------------------------------
static unsigned long faseBase = 0;
static unsigned long ultimoCiclo = 0;
static bool          baseDibujada = false;

static uint8_t esperado[25];
static int     ultimoBrillo = -1;
static int     ultimoParpIzq = -1;
static int     ultimoParpDer = -1;
static int     ultimoLabio = -1;
static int     ultimoArriba = -1;

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

    setPixelTrazado(PARPADO_IZQ[0], PARPADO_IZQ[1], 255);
    setPixelTrazado(PARPADO_DER[0], PARPADO_DER[1], 255);
    setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], 255);
    setPixelTrazado(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 3; i++)
        setPixelTrazado(BOCA[i][0], BOCA[i][1], 255);

    ultimoParpIzq = ultimoParpDer = 255;
    ultimoLabio = 255;
    ultimoArriba = 0;
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

// Respira CALMADA: rampa lineal a ritmo medio (65 <-> 115) y 150 ms quieto
// en 85. Ni tan agitada como el enojo ni tan lenta como la tristeza.
static int brilloRespira(float p)
{
    if (p < RESP_SUBE) return 65 + (int)((115 - 65) * p / RESP_SUBE);
    if (p < RESP_BAJA) return 115 - (int)((115 - 65) * (p - RESP_SUBE)
                                          / (RESP_BAJA - RESP_SUBE));
    return BRILLO_REPOSO;
}

// El PEEK: el parpado se levanta y queda la pupila a la vista 420 ms. Las
// pupilas (1,1) y (3,1) NUNCA se apagan: el que se mueve es el parpado.
static int parpadoPeek(float p)
{
    if (p < PEEK_ABRE)   return 255 - (int)(255 * p / PEEK_ABRE);
    if (p < PEEK_MIRA)   return 0;
    if (p < PEEK_CIERRA) return (int)(255 * (p - PEEK_MIRA)
                                      / (PEEK_CIERRA - PEEK_MIRA));
    return 255;
}

// El "meh": el labio izquierdo baja de 255 a 55 mientras el de arriba sube de
// 0 a 200. Los otros dos pixeles de la boca NO se tocan (asimetria).
static void bocaReto(float p, int &labio, int &arriba)
{
    if (p < RETO_SUBE) {
        float q = p / RETO_SUBE;
        labio  = 255 - (int)(200 * q);
        arriba = (int)(200 * q);
    } else if (p < RETO_BAJA) {
        labio  = 55;
        arriba = 200;
    } else {
        float q = (p - RETO_BAJA) / (1.0f - RETO_BAJA);
        labio  = 55 + (int)(200 * q);
        arriba = 200 - (int)(200 * q);
    }
}

// ---------------------------------------------------------------------------
// El primer frame, para las TRANSICIONES y CALLA. Ancla el ciclo.
// ---------------------------------------------------------------------------
void mostrarCaraNeutral()
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
void animarNeutral()
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
        uBit.serial.printf("N%d\n", (int)(ciclo % 1000));
    }

    // Localiza el segmento (5 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &CICLO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++) {
        if (t >= CICLO[i].desde && t < CICLO[i].hasta) { seg = &CICLO[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    // --- Brillo global: solo lo mueve la respiracion -----------------------
    int brillo = (seg->curva == C_RESPIRA) ? brilloRespira(p) : BRILLO_REPOSO;
    // Zona muerta: el quantum del PWM avanza de a saltos de ~1,22 unidades
    // (quantum = 0.8169 * brillo). Los escalones del original eran de 5, asi
    // que el deadband descarta los cambios que no se ven y ahorra la
    // division entera por software de cada setBrightness.
    if (brillo - ultimoBrillo >= 3 || ultimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        ultimoBrillo = brillo;
    }

    // --- El peek: los parpados exteriores se levantan ---------------------
    int parpado = (seg->curva == C_PEEK) ? parpadoPeek(p) : 255;
    if (parpado != ultimoParpIzq) {
        setPixelTrazado(PARPADO_IZQ[0], PARPADO_IZQ[1], parpado);
        ultimoParpIzq = parpado;
    }
    if (parpado != ultimoParpDer) {
        setPixelTrazado(PARPADO_DER[0], PARPADO_DER[1], parpado);
        ultimoParpDer = parpado;
    }
    // Las pupilas quedan siempre encendidas: son ellas las que el peek
    // deja al descubierto, asi que no se tocan nunca.

    // --- El "meh": solo el lado izquierdo del labio se mueve ---------------
    int labio = 255, arriba = 0;
    if (seg->curva == C_RETO) {
        bocaReto(p, labio, arriba);
    }
    if (labio != ultimoLabio) {
        setPixelTrazado(LABIO_IZQ_X, LABIO_IZQ_Y, labio);
        ultimoLabio = labio;
    }
    if (arriba != ultimoArriba) {
        setPixelTrazado(ARRIBA_IZQ_X, ARRIBA_IZQ_Y, arriba);
        ultimoArriba = arriba;
    }
    // (2,3) y (3,3) NO se tocan nunca: esa es la asimetria del "meh".

    uBit.sleep(16);   // ~60 fps
}
