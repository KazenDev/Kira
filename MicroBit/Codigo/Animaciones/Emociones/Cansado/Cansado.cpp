/**
 * Cansado.cpp - La emocion CANSADO 😪 (en reposo) - "ya no puedo mas"
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME: una llamada = un frame (~60 fps), el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Antes era una SECUENCIA de ~66 sleep() con 6 checkpoints en 4,3 segundos.
 * La fase "parpadeo pesado" son 1.04 s SIN un solo revisarSerial(), y aparece
 * DOS veces por pasada, asi que la ventana sorda era de 950 ms: la peor de
 * las ocho emociones. Medido con Bench/bench_emociones.cpp.
 *
 * Ahora el estado vive en systemTime(): la animacion es una funcion pura del
 * tiempo, el comando de la IA se nota en el frame siguiente y las rampas son
 * continuas. Mismo patron que Alegria y Triste.
 *
 * A diferencia de Triste, aqui NO HAY AZAR: la maxima es puramente mecanica,
 * asi que todo sale de la tabla de segmentos y de la fase.
 *
 * ── Por que NO se le pone easing encima ──────────────────────────────────
 * En un bucle ambiental, la propia fase YA es la curva. Aplicar easing
 * encima de una curva derivada de la fase se pelea con ella (es lo que
 * recomiendan las guias de easing para pixel art: "los bucles deben derivar
 * de la fase, no de estado con easing"). Y los tiempos del guion original
 * ya estan bien: un bostezo real se hace con abrir lento + hold largo +
 * cerrar lento, que es exactamente lo que hay aqui (180 ms / 400 ms /
 * 180 ms). No se tocan los tiempos: el cambio es de fluidez, no de guion.
 */
#include "Cansado.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
// Ojos normales (pero que se cierran pesado)
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Boca CHICA (cerrada, fila 3)
static const uint8_t BOCA_CHICA[3][2] = {
    {1, 3}, {2, 3}, {3, 3}
};

// El BOSTEZO abre la boca en grande: agrega la fila de arriba (y=2, la que
// queda justo bajo los ojos) y la de abajo (y=4). Seis pixeles, ninguno
// encima de la cara en reposo.
static const uint8_t BOCA_ABIERTA[6][2] = {
    {1,2}, {2,2}, {3,2},
    {1,4}, {2,4}, {3,4}
};

#define BRILLO_REPOSO  75      // apagada: el cansado no brilla

// ---------------------------------------------------------------------------
// EL CICLO COMO TABLA. Los tramos son los del guion original: 920 + 1040 +
// 760 + 1040 + 570 = 4330 ms.
// ---------------------------------------------------------------------------
enum Curva
{
    C_PLANO = 0,   // todo quieto
    C_RESPIRA,     // respira cansada: rampa 55->95->55 + 200 ms quieto
    C_PARP,        // cierra lento, queda cerrado 350 ms, abre lento
    C_BOSTEZO,     // la boca chica se abre en grande
    C_CABECEO      // el brillo se va a pique (75->40) y vuelve
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  curva;
};

static const Segmento CICLO[] = {
    {   0,  920, C_RESPIRA  },
    { 920, 1960, C_PARP     },
    {1960, 2720, C_BOSTEZO  },
    {2720, 3760, C_PARP     },
    {3760, 4330, C_CABECEO  },
};
static const int NCICLOS = sizeof(CICLO) / sizeof(CICLO[0]);
static const unsigned short CICLO_MS = 4330;

// Proporciones internas de cada curva (derivadas de los tiempos del original)
//   respira  : 360 arriba, 360 abajo, 200 quieto        (de  920)
//   parpadeo : 270 cierra, 350 cerrado, 270 abre, 150   (de 1040)
//   bostezo  : 180 abre, 400 abierto, 180 cierra         (de  760)
//   cabeceo  : 160 baja, 150 quieto, 160 sube, 100      (de  570)
#define RESP_SUBE   0.3913f
#define RESP_BAJA   0.7826f
#define PARP_CIERRA 0.2596f
#define PARP_CERRADO 0.5962f
#define PARP_ABRE   0.8558f
#define BOS_ABRE    0.2368f
#define BOS_CIERRA  0.7632f
#define CAB_BAJA    0.2807f
#define CAB_SUBE    0.8246f

// ---------------------------------------------------------------------------
// Estado: el reloj, el ciclo en curso y lo ultimo que escribimos.
// Se rastrean LOS 25 PIXELS: el bostezo agrega 6 que no son de la cara en
// reposo, asi que un rastreo parcial (como el de Alegria, que solo necesita
// 7) no serviria. Cuesta ~1,5 ms de CPU por ciclo de 4,3 s (0,03%) y a
// cambio la cara se auto-repara sola si otra cosa escribio en la pantalla.
// ---------------------------------------------------------------------------
static unsigned long faseBase = 0;
static unsigned long ultimoCiclo = 0;
static bool          baseDibujada = false;

static uint8_t esperado[25];
static int     ultimoBrillo = -1;
static int     ultimoOjoIzq = -1;
static int     ultimoOjoDer = -1;
static int     ultimoBostezo = -1;      // valor de la fila extra de la boca

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

    setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], 255);
    setPixelTrazado(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 3; i++)
        setPixelTrazado(BOCA_CHICA[i][0], BOCA_CHICA[i][1], 255);

    ultimoOjoIzq = 255;
    ultimoOjoDer = 255;
    ultimoBostezo = 0;
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

// Respira CANSADA: rampa lineal lenta, y apagada (55-95, contra los 60-120 de
// la alegria). RAMPA y no seno: en un bucle la fase ya ES la curva.
static int brilloRespira(float p)
{
    if (p < RESP_SUBE) return 55 + (int)((95 - 55) * p / RESP_SUBE);
    if (p < RESP_BAJA) return 95 - (int)((95 - 55) * (p - RESP_SUBE)
                                         / (RESP_BAJA - RESP_SUBE));
    return BRILLO_REPOSO;
}

// Parpadeo PESADO: los DOS ojos con el MISMO valor (movidos juntos a
// proposito; en serie se veria desincronizado).
static int ojosPesados(float p)
{
    if (p < PARP_CIERRA)  return 255 - (int)(255 * p / PARP_CIERRA);
    if (p < PARP_CERRADO) return 0;
    if (p < PARP_ABRE)    return (int)(255 * (p - PARP_CERRADO)
                                       / (PARP_ABRE - PARP_CERRADO));
    return 255;
}

// El BOSTEZO: valor de la fila extra de la boca (0 = cerrada, 255 = abierta).
static int bostezoValor(float p)
{
    if (p < BOS_ABRE)   return (int)(255 * p / BOS_ABRE);
    if (p < BOS_CIERRA) return 255;
    return 255 - (int)(255 * (p - BOS_CIERRA) / (1.0f - BOS_CIERRA));
}

// El CABECEO: la cabeza cae por el sueno (75 -> 40) y vuelve.
static int brilloCabeceo(float p)
{
    if (p < CAB_BAJA) return 75 - (int)(35 * p / CAB_BAJA);
    if (p < CAB_SUBE) return 40 + (int)(35 * (p - CAB_BAJA) / (CAB_SUBE - CAB_BAJA));
    return BRILLO_REPOSO;
}

// ---------------------------------------------------------------------------
// El primer frame, para las TRANSICIONES y CALLA. Ancla el ciclo.
// ---------------------------------------------------------------------------
void mostrarCaraCansado()
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
void animarCansado()
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
        uBit.serial.printf("C%d\n", (int)(ciclo % 1000));
    }

    // Localiza el segmento (5 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &CICLO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++) {
        if (t >= CICLO[i].desde && t < CICLO[i].hasta) { seg = &CICLO[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    // --- Ojos ---
    int ojos = (seg->curva == C_PARP) ? ojosPesados(p) : 255;
    if (ojos != ultimoOjoIzq) {
        setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], ojos);
        ultimoOjoIzq = ojos;
    }
    if (ojos != ultimoOjoDer) {
        setPixelTrazado(OJO_DER[0], OJO_DER[1], ojos);
        ultimoOjoDer = ojos;
    }

    // --- Brillo global: lo mueven la respiracion y el cabeceo ---
    int brillo = BRILLO_REPOSO;
    if (seg->curva == C_RESPIRA) brillo = brilloRespira(p);
    else if (seg->curva == C_CABECEO) brillo = brilloCabeceo(p);

    // Zona muerta: el quantum del PWM avanza de a saltos de ~1,22 unidades
    // (quantum = 0.8169 * brillo), asi que 1-2 unidades no se ven. Y
    // setBrightness() en el M0+ hace una division entera por software.
    if (brillo - ultimoBrillo >= 3 || ultimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        ultimoBrillo = brillo;
    }

    // --- La boca: chica en reposo, abierta de a poco en el bostezo ---
    int boca = (seg->curva == C_BOSTEZO) ? bostezoValor(p) : 0;
    if (boca != ultimoBostezo) {
        for (int i = 0; i < 6; i++)
            setPixelTrazado(BOCA_ABIERTA[i][0], BOCA_ABIERTA[i][1], boca);
        ultimoBostezo = boca;
    }
    // La boca chica (fila 3) nunca se apaga en el bostezo, como en el
    // original: no hace falta reescribirla. Solo si quedo sucia.

    uBit.sleep(16);   // ~60 fps
}
