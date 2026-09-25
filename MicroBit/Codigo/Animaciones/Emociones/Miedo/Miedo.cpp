/**
 * Miedo.cpp - La emocion MIEDO 😨 (en reposo) - "¡AAAAH!"
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME: una llamada = un frame (~60 fps), el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Antes era una SECUENCIA de ~40 sleep() con 6 checkpoints en 2,2 segundos.
 * La fase "corazon acelerado" son 720 ms SIN un solo revisarSerial(), asi que
 * la ventana sorda era de 590 ms. Medido con Bench/bench_emociones.cpp.
 *
 * Ahora el estado vive en systemTime(): la animacion es una funcion pura del
 * tiempo y el comando de la IA se nota en el frame siguiente.
 *
 * ── EL AZAR ──────────────────────────────────────────────────────────────
 * Este es el segundo caso con azar (el primero fue Triste). Acá hay un solo
 * punto: el temblor del cuerpo, que era `85 + uBit.random(70)`.
 *
 * `codal::random()` es un LFSR con `static uint32_t random_value` GLOBAL
 * COMPARTIDO (CodalCompat.cpp:33) que tambien usa hacerTransicion() para
 * elegir la transicion, asi que el valor dependia de cuantos numeros se
 * habian sacado antes en toda la firmware. No es una funcion del tiempo, y
 * con el patron de frame no alcanza. Se reemplaza por pseudo(), un hash sin
 * estado: mismo instante, mismo temblor, y ~15 ciclos en vez de ~100.
 *
 * ── EL LATIDO ────────────────────────────────────────────────────────────
 * El "pum-pum" se deja POR ESCALONES a proposito, y el segundo latido mas
 * corto y mas tenue. Eso es a proposito: un latido es un golpe, no una curva,
 * y la convencion del "lub-dub" es exactamente dos latidos rapidos con el
 * segundo mas chico. Un pulso suave se lee como un parpadeo, no como un
 * corazon acelerado.
 *
 * ── DATO PENDIENTE DE TU OJO (esta a una linea) ──────────────────────────
 * El temblor se re-ranura cada TEMBLO_MS. Con 30 ms quedan ~33 Hz, y las
 * guias de movimiento sitúan el "temblor/espiro" entre 5 y 15 Hz (período de
 * 0,07 a 0,2 s): por arriba de eso un brillo que salta a azar se empieza a
 * leer como PARPADEO y no como cuerpo temblando. Si cuando lo mires te
 * parece que centellea en vez de temblar, subi TEMBLO_MS a 100 (10 Hz, en
 * plena banda). NO lo cambie por mi cuenta: es tu diseño visual.
 */
#include "Miedo.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
//
//     . # . # .     <- cejas LEVANTADAS de terror
//     # . . . #     <- ojos BIEN ABIERTOS en las esquinas
//     . # # # .     >  la boca GRANDE abierta (gritando), filas 2 y 4
//     . # . # .     >  y la fila 3 con un hueco en el medio
// ---------------------------------------------------------------------------
static const uint8_t CEJA_IZQ[2] = {1, 0};
static const uint8_t CEJA_DER[2] = {3, 0};

// Ojos BIEN ABIERTOS en las esquinas (mirando fijo, de terror)
static const uint8_t OJO_IZQ[2] = {0, 1};
static const uint8_t OJO_DER[2] = {4, 1};

// "Pupilas" hacia adentro (cuando los ojos MIRAN la amenaza). Notar que
// (1,1) y (3,1) son pixeles LIBRES de la cara en reposo: por eso el mirada al
// centro no necesita dibujar nada nuevo, solo apagar las esquinas.
static const uint8_t PUPILA_IZQ[2] = {1, 1};
static const uint8_t PUPILA_DER[2] = {3, 1};

// Boca GRANDE abierta (el grito): 8 pixeles
static const uint8_t BOCA[8][2] = {
    {1, 2}, {2, 2}, {3, 2},      // fila 2: .###.
    {1, 3}, {3, 3},              // fila 3: .#.#.
    {1, 4}, {2, 4}, {3, 4}       // fila 4: .###.
};

#define BRILLO_REPOSO 100

// ---------------------------------------------------------------------------
// pseudo(): hash entero SIN ESTADO. Ver el comentario de Triste.cpp: es la
// forma de tener "azar" y que la animacion siga siendo una funcion pura del
// reloj. ~15 ciclos contra los ~100 de codal::random().
// ---------------------------------------------------------------------------
static unsigned int pseudo(unsigned int x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// El temblor del cuerpo se re-ranura cada este tiempo. Ver la nota del
// encabezado: 30 ms quedan a ~33 Hz, por encima de la banda de temblor.
#define TEMBLO_MS 30
#define TEMBLO_MIN 85
#define TEMBLO_RANGO 70      // 85..154, como el original

// ---------------------------------------------------------------------------
// EL CICLO COMO TABLA. 720 + 360 + 280 + 450 + 400 = 2210 ms.
// ---------------------------------------------------------------------------
enum Curva
{
    C_PLANO = 0,   // todo quieto
    C_CORAZON,     // pum-pum: 3 latidos de 4 escalones
    C_TIEMBLA,     // el cuerpo entero vibra (el brillo, al azar)
    C_MIRA,        // los ojos van al centro (la amenaza) y vuelven
    C_PARP_RAPIDO, // 3 parpadeos seguidos
    C_BOCA_TIEMBLA  // los labios tiemblan
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  curva;
};

static const Segmento CICLO[] = {
    {   0,  720, C_CORAZON      },
    { 720, 1080, C_TIEMBLA      },
    {1080, 1360, C_MIRA         },
    {1360, 1810, C_PARP_RAPIDO  },
    {1810, 2210, C_BOCA_TIEMBLA },
};
static const int NCICLOS = sizeof(CICLO) / sizeof(CICLO[0]);
static const unsigned short CICLO_MS = 2210;

// Proporciones internas (derivadas de los tiempos del original)
//   corazon      : 3 repeticiones de 240 ms; dentro: 50 / 80 / 40 / 70
//   tiembla      : 12 pasos de 30 ms
//   mira         : 180 fuera, 100 de vuelta              (de 280)
//   parpadeo     : 3 repeticiones de 150 ms; dentro: 60 / 90
//   boca tiembla : 4 repeticiones de 100 ms; dentro: 50 / 50
#define MIRA_FUERA 0.6429f

// ---------------------------------------------------------------------------
// Estado: el reloj, el ciclo en curso y lo ultimo escrito.
// Se rastrean LOS 25 PIXELS: el miedo es la cara mas grande de las ocho
// (2 cejas + 2 ojos + 8 de boca + 2 pupilas = 14 en uso), asi que un
// rastreo parcial de 7 seria una mentira.
// ---------------------------------------------------------------------------
static unsigned long faseBase = 0;
static unsigned long ultimoCiclo = 0;
static bool          baseDibujada = false;

static uint8_t esperado[25];
static int     ultimoBrillo = -1;
static int     ultimoOjoIzq = -1;
static int     ultimoOjoDer = -1;
static int     ultimoPupIzq = -1;
static int     ultimoPupDer = -1;
static int     ultimoBoca = -1;

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
    setPixelTrazado(CEJA_DER[0], CEJA_DER[1], 255);
    setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], 255);
    setPixelTrazado(OJO_DER[0], OJO_DER[1], 255);
    for (int i = 0; i < 8; i++)
        setPixelTrazado(BOCA[i][0], BOCA[i][1], 255);

    ultimoOjoIzq = ultimoOjoDer = 255;
    ultimoPupIzq = ultimoPupDer = 0;
    ultimoBoca = 255;
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

// El "pum-pum": 3 repeticiones de 240 ms. Dentro de cada una, cuatro
// ESCALONES (no una curva): pum fuerte, descanso, pum mas chico, descanso.
// El segundo latido mas bajo y mas corto es el "dub" del "lub-dub".
static int brilloCorazon(float p)
{
    float q = p / 0.33333f;                 // 0..3, la repeticion
    float r = q - (float)(int)q;            // 0..1 dentro de la repeticion

    if (r < 0.2083f) return 160;            // pum      (50 ms)
    if (r < 0.5417f) return BRILLO_REPOSO;  // descanso (80 ms)
    if (r < 0.7083f) return 140;            // pum chico(40 ms)
    return BRILLO_REPOSO;                   // descanso (70 ms)
}

// El temblor del cuerpo: sale del hash del tiempo, no del RNG. Mismo
// instante, mismo temblor; se puede saltar a cualquier punto de la animacion
// y sigue viniendo bien.
static int brilloTemblor(unsigned long ciclo, unsigned short t)
{
    unsigned int semilla = pseudo(ciclo * 1000u + (unsigned int)(t / TEMBLO_MS));
    return TEMBLO_MIN + (int)(semilla % TEMBLO_RANGO);
}

// Parpadeo RAPIDISIMO: 3 repeticiones de 150 ms (60 apagado, 90 abierto).
static bool ojosParpadeoRapido(float p)
{
    float q = p / 0.33333f;
    float r = q - (float)(int)q;
    return r < 0.4f;      // true = ojos apagados
}

// Los labios tiemblan: 4 repeticiones de 100 ms (50 a 150, 50 a 255).
static int bocaTiemblaValor(float p)
{
    float q = p / 0.25f;                    // 0..4
    float r = q - (float)(int)q;
    return r < 0.5f ? 150 : 255;
}

// ---------------------------------------------------------------------------
// El primer frame, para las TRANSICIONES y CALLA. Ancla el ciclo.
// ---------------------------------------------------------------------------
void mostrarCaraMiedo()
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
void animarMiedo()
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
        uBit.serial.printf("M%d\n", (int)(ciclo % 1000));
    }

    // Localiza el segmento (5 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &CICLO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++) {
        if (t >= CICLO[i].desde && t < CICLO[i].hasta) { seg = &CICLO[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    // --- Ojos y pupilas ---------------------------------------------------
    // El mirada al centro apaga las esquinas y enciende (1,1) y (3,1), que
    // son pixeles LIBRES de la cara en reposo.
    int ojos = 255, pupilas = 0;
    if (seg->curva == C_MIRA) {
        if (p < MIRA_FUERA) { ojos = 0; pupilas = 255; }
    } else if (seg->curva == C_PARP_RAPIDO) {
        if (ojosParpadeoRapido(p)) ojos = 0;
    }

    if (ojos != ultimoOjoIzq) {
        setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], ojos);
        ultimoOjoIzq = ojos;
    }
    if (ojos != ultimoOjoDer) {
        setPixelTrazado(OJO_DER[0], OJO_DER[1], ojos);
        ultimoOjoDer = ojos;
    }
    if (pupilas != ultimoPupIzq) {
        setPixelTrazado(PUPILA_IZQ[0], PUPILA_IZQ[1], pupilas);
        ultimoPupIzq = pupilas;
    }
    if (pupilas != ultimoPupDer) {
        setPixelTrazado(PUPILA_DER[0], PUPILA_DER[1], pupilas);
        ultimoPupDer = pupilas;
    }

    // --- Brillo global: lo mueven el corazon y el temblor ---------------
    int brillo = BRILLO_REPOSO;
    if (seg->curva == C_CORAZON) brillo = brilloCorazon(p);
    else if (seg->curva == C_TIEMBLA) brillo = brilloTemblor(ciclo, t);

    // Zona muerta: el quantum del PWM avanza de a saltos de ~1,22 unidades
    // (quantum = 0.8169 * brillo). Con el temblor eso importa doble: los
    // valores del hash son casi todos distintos, asi que sin deadband se
    // pagaria una division entera por software en cada frame.
    if (brillo - ultimoBrillo >= 3 || ultimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        ultimoBrillo = brillo;
    }

    // --- La boca: el grito que tiembla -----------------------------------
    int boca = (seg->curva == C_BOCA_TIEMBLA) ? bocaTiemblaValor(p) : 255;
    if (boca != ultimoBoca) {
        for (int i = 0; i < 8; i++)
            setPixelTrazado(BOCA[i][0], BOCA[i][1], boca);
        ultimoBoca = boca;
    }

    uBit.sleep(16);   // ~60 fps
}
