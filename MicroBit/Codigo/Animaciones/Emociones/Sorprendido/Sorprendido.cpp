/**
 * Sorprendido.cpp - La emocion SORPRENDIDO 😲 (en reposo)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME: una llamada = un frame (~60 fps), el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Antes era una SECUENCIA de ~56 sleep() con 6 checkpoints. La fase del
 * "pulso de asombro" son 750 ms SIN un solo revisarSerial(), y la boca "o"
 * son 460 ms (y sale dos veces). Medido con Bench/bench_emociones.cpp: 725 ms
 * de peor latencia.
 *
 * ── EL CICLO ES VARIABLE, Y ESO HAY QUE CUIDAR ───────────────────────────
 * El pulso de asombro sale 1 de cada 3 pasadas, asi que el ciclo NO tiene
 * duracion fija: 2630 ms con pulso, 1880 ms sin el. Con un unico ciclo fijo
 * de 2630, dos de cada tres ciclos tendrian 750 ms de cara quieta de mas:
 * un 40% mas lento, y el ritmo seeria NOTABLEmente mas lento.
 *
 * Por eso el ciclo se mide con DOS divisores CONSTANTES, elegidos con un
 * condicional segun si este ciclo tiene pulso:
 *
 *     t = pulso ? transcurrido % 2630 : transcurrido % 1880
 *
 * Los dos divisores son constantes de compilacion, asi que GCC los convierte
 * en multiplicacion+shift: no aparece ninguna llamada a __aeabi_idiv (que
 * seria lo unico que romperia la propiedad de "cero divisiones en toda la
 * firmware"). Con un unico `cicloMs` variable seria una division real por
 * frame.
 *
 * ── LO QUE SE DEJO INTACTO ───────────────────────────────────────────────
 * Los ojos se dilatan y quedan 280 ms MIRANDO FIJO, y eso es correcto: las
 * guias de animacion de sorpresa dicen que "puede haber un momento de
 * quietud mientras el personaje procesa lo que esta pasando", y que "una
 * reaccion de sorpresa autentica pasa muy rapido, en apenas unos frames".
 * El hold ES la sorpresa; el movimiento es solo el arranque.
 *
 * El pulso queda POR ESCALONES (90 -> 255 en 12 pasos de 15). Un startle es
 * involuntario y abrupto: una rampa suave lo dejaria leido como una
 *transition normal y no como un susto.
 */
#include "Sorprendido.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
//
//     . . . . .
//     . # . # .     <- ojos bien abiertos
//     . . . . .
//     . . # . .     <- el centro de la boca "o" (siempre encendido)
//     . . . . .
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// La boca "o": el centro (2,3) + el diamante grande (2,2),(1,3),(3,3),(2,4)
static const uint8_t O_CENTRO[2] = {2, 3};
static const uint8_t O_DIAMANTE[4][2] = {
    {2, 2}, {1, 3}, {3, 3}, {2, 4}
};

#define BRILLO_REPOSO 90

// pseudo(): hash entero SIN ESTADO (ver Triste.cpp). El pulso NO usa azar
// aqui (eso lo decide un contador, ver abajo), pero hace falta para variar
// cada cuanto sale sin usar el LFSR global de CODAL.
static unsigned int pseudo(unsigned int x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

// ---------------------------------------------------------------------------
// EL CICLO COMO TABLA
//   430 + 460 + 120 + 460 + 750 + 410 = 2630 ms   (con pulso)
//   430 + 460 + 120 + 460 +          410 = 1880 ms   (sin pulso)
// ---------------------------------------------------------------------------
enum Curva
{
    C_PLANO = 0,   // todo quieto
    C_OJOS,        // los ojos se dilatan y quedan mirando fijo
    C_BOCA,        // la boca "o" se abre en diamante y se cierra
    C_PULSO,       // el susto: todo el brillo sube DE GOLPE (1 de cada 3)
    C_PARPADEO     // parpadeo rarisimo
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  curva;
};

// Con el pulso. El tramo del pulso va de 1470 a 2220; sin pulso se salta.
static const Segmento CICLO_CON_PULSO[] = {
    {   0,  430, C_OJOS     },
    { 430,  890, C_BOCA     },
    { 890, 1010, C_PLANO    },
    {1010, 1470, C_BOCA     },
    {1470, 2220, C_PULSO    },
    {2220, 2630, C_PARPADEO },
};
static const int NCICLOS = sizeof(CICLO_CON_PULSO) / sizeof(CICLO_CON_PULSO[0]);

#define CICLO_MS_CON_PULSO  2630
#define CICLO_MS_SIN_PULSO  1880
#define PULSO_DESDE 1470
#define PARPADEO_DESDE 2220

// Proporciones internas (de los tiempos del original)
//   ojos    : 150 ms de dilatacion, 280 ms mirando fijo   (de  430)
//   boca    : 3 pasos de 40 al abrir, 180 abierta, 4 de 40 al cerrar (de 460)
//   pulso   : 12 escalones de 25 subiendo, 12 bajando, 150 quieto (de  750)
//   parpadeo: 3 escalones de 20, 90 cerrado, 3 de 20, 200 quieto (de 410)
#define OJOS_DILATA 0.3488f
#define BOCA_P1     0.0870f
#define BOCA_P2     0.1739f
#define BOCA_P3     0.7391f
#define BOCA_P4     0.8261f
#define BOCA_P5     0.9130f
#define PULSO_PASO  0.0333f   // 25 ms de 750
#define RARO_P1     0.0488f
#define RARO_P2     0.0976f
#define RARO_P3     0.4146f
#define RARO_P4     0.4634f

// El pulso de asombro: decision de CALENDARIO, no visual. Este contador baja
// UNA VEZ POR CICLO (cuando la fase da la vuelta), nunca por frame: con 60
// frames por ciclo bajarlo por frame haria saltar el susto 60 veces por
// segundo. El original era `uBit.random(3) == 0` (1 de cada 3 pasadas), que
// usa el LFSR GLOBAL de CODAL y por lo tanto no es una funcion del tiempo.
static int  hastaPulso = 1;                 // el primer ciclo ya tiene pulso
static bool pulsoEnEsteCiclo = true;
static bool avisoEnviado = false;

// ---------------------------------------------------------------------------
// Estado: el reloj, el ciclo en curso y lo ultimo escrito. Se rastrean LOS
// 25 PIXELS (esta cara usa 7: 2 ojos, el centro y el diamante).
// ---------------------------------------------------------------------------
static unsigned long inicioCiclo = 0;   // ancla ABSOLUTA del ciclo corriente
static unsigned long cicloActual = CICLO_MS_SIN_PULSO;  // duracion de este ciclo
static unsigned long ultimoCiclo = 0;
static bool          baseDibujada = false;

static uint8_t esperado[25];
static int     ultimoBrillo = -1;
static int     ultimoOjoIzq = -1;
static int     ultimoOjoDer = -1;
static int     ultimoNivel = -1;

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
    setPixelTrazado(O_CENTRO[0], O_CENTRO[1], 255);

    ultimoOjoIzq = ultimoOjoDer = 255;
    ultimoNivel = 0;
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

// Los ojos se DILATAN: el brillo del pixel sube de 220 a 255 en 150 ms y
// despues quedan 280 ms mirando fijo. Ese hold es la sorpresa.
static int ojosDilatan(float p)
{
    if (p < OJOS_DILATA) return 220 + (int)(35 * p / OJOS_DILATA);
    return 255;
}

// El diamante crece en etapas (top -> lados -> fondo) y se achica al reves.
// Nivel 0 = solo el centro; 1 = +top; 2 = +lados; 3 = +fondo.
static int bocaNivel(float p)
{
    if (p < BOCA_P1) return 1;    // 40 ms: aparece el top
    if (p < BOCA_P2) return 2;    // 40 ms: los lados
    if (p < BOCA_P3) return 3;    // el fondo + los 180 ms abierta
    if (p < BOCA_P4) return 2;    // cierra: lados
    if (p < BOCA_P5) return 1;    // cierra: top
    return 0;                    // queda solo el centro
}

// El "wow" en 4 pixeles. Bit 0 = top (2,2), bit 1 = los dos lados
// (1,3) y (3,3), bit 2 = el fondo (2,4).
static void dibujarBoca(int nivel)
{
    static const uint8_t MASCARA[4] = { 0x0, 0x1, 0x3, 0xF };
    uint8_t m = MASCARA[nivel];
    setPixelTrazado(O_DIAMANTE[0][0], O_DIAMANTE[0][1], (m & 0x1) ? 255 : 0);
    setPixelTrazado(O_DIAMANTE[1][0], O_DIAMANTE[1][1], (m & 0x2) ? 255 : 0);
    setPixelTrazado(O_DIAMANTE[2][0], O_DIAMANTE[2][1], (m & 0x2) ? 255 : 0);
    setPixelTrazado(O_DIAMANTE[3][0], O_DIAMANTE[3][1], (m & 0x4) ? 255 : 0);
}

// El pulso de asombro: POR ESCALONES. Un startle es involuntario y abrupto;
// una rampa suave se leeria como una transicion normal y no como un susto.
static int brilloPulso(float p)
{
    int n = (int)(p / PULSO_PASO);          // 25 ms de 750
    if (n < 12) return 90 + 15 * n;         // subiendo
    if (n < 24) return 255 - 15 * (n - 12); // bajando
    return 90;                              // los 150 ms quieto
}

// Parpadeo RARISIMO: rapido, pero casi no parpadea.
static int ojosRaros(float p)
{
    if (p < RARO_P1) return 255;
    if (p < RARO_P2) return 127;
    if (p < RARO_P3) return 0;
    if (p < RARO_P4) return 127;
    return 255;
}

// ---------------------------------------------------------------------------
// El primer frame, para las TRANSICIONES y CALLA. Ancla el ciclo.
// ---------------------------------------------------------------------------
void mostrarCaraSorprendido()
{
    uBit.display.setBrightness(BRILLO_REPOSO);
    dibujarBase();
    ultimoBrillo = BRILLO_REPOSO;
    inicioCiclo = uBit.systemTime();
    cicloActual = CICLO_MS_CON_PULSO;   // el primer ciclo arranca con pulso
    ultimoCiclo = 0;
    pulsoEnEsteCiclo = true;
    hastaPulso = 1;
    avisoEnviado = false;
}

// ---------------------------------------------------------------------------
// UN FRAME. La llama el bucle principal ~60 veces por segundo.
// ---------------------------------------------------------------------------
void animarSorprendido()
{
    if (revisarSerial()) return;

    if (!baseDibujada || !caraIntacta())
        dibujarBase();

    unsigned long ahora = uBit.systemTime();

    // --- EL CICLO ES VARIABLE: se mide desde el INICIO DE ESTE CICLO ------
    //
    // NO se puede hacer (ahora - faseBase) % 2630 alternando con
    // (ahora - faseBase) % 1880. Los dos modulos son de periodo distinto y
    // latiguean entre si: los limites de ciclo caen donde caiga y el pulso se
    // dispara a destiempo. (Comprobado en la placa: 54 "ciclos" en 80 s con
    // duraciones de 50 ms a 2,6 s, cuando deberian ser 1880 o 2630.)
    //
    // Con un ciclo de duracion variable hay que llevar el INICIO del ciclo
    // corriente y avanzar ese ancla en cada wrap. Sin division, sin drift, y
    // el pulso siempre en el lugar correcto.
    unsigned long t = ahora - inicioCiclo;

    if (t >= cicloActual) {
        // Ancla ABSOLUTA: se le suma la duracion del ciclo que acaba de
        // terminar en vez de reiniciar a "ahora". Asi un frame tardio no
        // acumula error.
        inicioCiclo += cicloActual;
        t = ahora - inicioCiclo;
        if (t >= cicloActual) t = 0;     // salvaguarda por si se paso mucho

        ultimoCiclo++;
        pulsoEnEsteCiclo = (--hastaPulso <= 0);
        if (pulsoEnEsteCiclo) {
            avisoEnviado = false;
            // El original era 1 de cada 3 pasadas. Con 2 o 3 el promedio da
            // ~2,5 ciclos entre sustos.
            hastaPulso = 2 + (int)(pseudo(ultimoCiclo) % 2);
        }
        cicloActual = pulsoEnEsteCiclo ? CICLO_MS_CON_PULSO : CICLO_MS_SIN_PULSO;

        // OJO: el printf de CODAL solo soporta %d y %s (Serial.cpp:425).
        uBit.serial.printf("S%d\n", (int)(ultimoCiclo % 1000));
    }

    unsigned short fase = (unsigned short)t;

    // --- Sin pulso, el tramo del pulso NO EXISTE: se corre el parpadeo -----
    // El parpadeo esta en 2220..2630 de la tabla CON pulso. Un ciclo sin
    // pulso dura 1880, o sea que la fase nunca llegaria a 2220 y el
    // parpadeo se quedaria a mitad de su rampa al terminar el ciclo. Por eso
    // se corren los ultimos 750 ms: en un ciclo sin pulso, fase=1470 tiene
    // que LEER el parpadeo entero (1470..1880).
    unsigned short busqueda = fase;
    if (!pulsoEnEsteCiclo && fase >= PULSO_DESDE)
        busqueda = (unsigned short)(fase + (PARPADEO_DESDE - PULSO_DESDE));

    // Localiza el segmento (con el offset ya aplicado, un solo recorrido).
    const Segmento *seg = &CICLO_CON_PULSO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++) {
        if (busqueda >= CICLO_CON_PULSO[i].desde && busqueda < CICLO_CON_PULSO[i].hasta) {
            seg = &CICLO_CON_PULSO[i];
            break;
        }
    }
    float p = (float)(busqueda - seg->desde) / (float)(seg->hasta - seg->desde);

    // --- Ojos: la dilatacion y el parpadeo rarisimo ------------------------
    int ojos = 255;
    if (seg->curva == C_OJOS)   ojos = ojosDilatan(p);
    else if (seg->curva == C_PARPADEO) ojos = ojosRaros(p);
    if (ojos != ultimoOjoIzq) {
        setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], ojos);
        ultimoOjoIzq = ojos;
    }
    if (ojos != ultimoOjoDer) {
        setPixelTrazado(OJO_DER[0], OJO_DER[1], ojos);
        ultimoOjoDer = ojos;
    }

    // --- La boca "o" en diamante ------------------------------------------
    int nivel = (seg->curva == C_BOCA) ? bocaNivel(p) : 0;
    if (nivel != ultimoNivel) {
        dibujarBoca(nivel);
        ultimoNivel = nivel;
    }

    // --- El brillo: el pulso de asombro es el unico que lo mueve ----------
    int brillo = BRILLO_REPOSO;
    if (seg->curva == C_PULSO && pulsoEnEsteCiclo) {
        if (!avisoEnviado) {
            uBit.serial.send("PULSO\n");   // debug: el susto
            avisoEnviado = true;
        }
        brillo = brilloPulso(p);
    }
    // Zona muerta: el quantum del PWM avanza de a saltos de ~1,22 unidades
    // (quantum = 0.8169 * brillo). Los escalones del pulso son de 15, asi
    // que ningun escalon se pierde.
    if (brillo - ultimoBrillo >= 3 || ultimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        ultimoBrillo = brillo;
    }

    uBit.sleep(16);   // ~60 fps
}
