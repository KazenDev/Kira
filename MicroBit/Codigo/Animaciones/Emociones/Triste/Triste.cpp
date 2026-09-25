/**
 * Triste.cpp - La emocion TRISTE 😢 (en reposo)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME: una llamada = un frame (~60 fps), el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Antes era una SECUENCIA de ~105 sleep() con 7 checkpoints en 6 segundos. La
 * fase "respira lenta" son 1,46 s SIN un solo revisarSerial(), y como aparece
 * DOS veces por pasada, la ventana sorda era de 1.415 ms: la peor de las ocho
 * emociones, mas que la alegria tenia antes de migrarla (1.290 ms). Medido con
 * Bench/bench_emociones.cpp.
 *
 * Ahora el estado vive en systemTime(): la animacion es una funcion pura del
 * tiempo, el comando de la IA se nota en el frame siguiente y las curvas son
 * continuas en vez de escalones. Mismo patron que Alegria y que el metronomo.
 *
 * LO QUE HACE MAS DIFICIL ESTO: la tristeza es la unica con AZAR. El
 * original usaba el RNG de CODAL en dos sitios:
 *
 *   - la boca tiembla: 42 llamadas a uBit.random() por pasada
 *   - la lagrima: de que ojo cae, y cada cuanto cae (cada 4-7 pasadas)
 *
 * Eso NO es una funcion del tiempo. codal::random() es un LFSR con
 * `static uint32_t random_value` GLOBAL COMPARTIDO (CodalCompat.cpp:33) que
 * tambien usa hacerTransicion() para elegir la transicion: el valor del
 * temblor dependia de cuantos numeros se habian sacado antes en toda la
 * firmware. Con el patron de frame eso no sirve: hay que poder saltar a
 * cualquier instante y que la cara siga saliendo bien.
 *
 * Como se resuelve:
 *   - EL TEMBLOR sale de un hash del tiempo (pseudo()), no del RNG. Es una
 *     funcion pura: mismo instante, mismo temblor. Y sale MAS BARATO: ~15
 *     ciclos contra ~100, porque son dos multiplicaciones de un solo ciclo
 *     (el M0 no tiene multiplicador de alta media, pero el de 32x32->32 es
 *     single cycle) en vez de un LFSR con rechazo.
 *   - LA LAGRIMA si es una decision de calendario, no visual, asi que puede
 *     quedar como estado: un contador que baja UNA VEZ POR CICLO (cuando la
 *     fase da la vuelta), no una vez por pasada. Se conserva el "cada 4-7
 *     pasadas" del original.
 */
#include "Triste.h"
#include "../../Sistema/Sistema.h"
// cosf() para las curvas. Explicito aunque CODAL ya arrastre math.h.
#include <math.h>

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Boca INVERTIDA (frown): medio (1,3)(2,3)(3,3) + esquinas (0,4)(4,4)
static const uint8_t BOCA[5][2] = {
    {0,4}, {4,4}, {1,3}, {2,3}, {3,3}
};

#define BRILLO_REPOSO  80      // la tristeza es mas tenue que la alegria (90)

// ---------------------------------------------------------------------------
// pseudo(): hash entero SIN ESTADO, con semilla.
//
// Es lo que permite que el temblor del labio sea "aleatorio" y al mismo
// tiempo una funcion pura del reloj. Es la tecnica estandar para esto (se la
// conoce como FPS-R: ruido pseudoaleatorio sin estado, sembrado por el indice
// de frame; en vez de acordarse del pasado, solo del instante actual).
//
// Dos multiplicaciones, que en el Cortex-M0 son de un solo ciclo, y un par de
// desplazamientos: ~15 ciclos en total, contra los ~100 de codal::random().
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

// ---------------------------------------------------------------------------
// EL CICLO COMO TABLA
//
// Los tramos son los del guion original, para que el cambio sea de fluidez y
// no de contenido. El hueco de la lagrima (760 ms) esta SIEMPRE en la tabla:
// en los ciclos donde no toca lagrima, ese tramo es simplemente quietud (la
// cara sigue respirando). Asi el ciclo es de duracion FIJA y la fase se
// puede calcular con un modulo.
// ---------------------------------------------------------------------------
enum Curva
{
    C_PLANO = 0,    // todo quieto
    C_RESP_LENTA,   // respira lento: rampa 45->100->45 + 200 ms quieto
    C_PARP_PESADO,  // cierra lento, queda cerrado, abre lento
    C_TIEMBLA,      // la boca tiembla (el labio a punto de llorar)
    C_LAGRIMA       // cae una lagrima (solo en los ciclos que toca)
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  curva;
};

static const Segmento CICLO[] = {
    {   0, 1460, C_RESP_LENTA  },
    {1460, 2560, C_PARP_PESADO },
    {2560, 2740, C_PLANO      },
    {2740, 3440, C_TIEMBLA    },
    {3440, 4900, C_RESP_LENTA  },
    {4900, 6000, C_PARP_PESADO },
    {6000, 6760, C_LAGRIMA    },
};
static const int NCICLOS = sizeof(CICLO) / sizeof(CICLO[0]);
static const unsigned short CICLO_MS = 6760;

// Proporciones internas de cada curva (derivadas de los tiempos del original)
//   respira lenta : 630 ms arriba, 630 ms abajo, 200 ms quieto  (de 1460)
//   parpadeo pesado: 350 cierra, 280 cerrado, 350 abre, 120 quieto (de 1100)
#define RESP_SUBE     0.4315f
#define RESP_BAJA     0.8630f
#define PARP_CIERRA   0.3182f
#define PARP_CERRADO  0.5727f
#define PARP_ABRE     0.8909f

// ---------------------------------------------------------------------------
// Estado: el reloj, el indice de ciclo y lo ultimo que escribimos.
// Se rastrean LOS 25 PIXELS, no solo los 7 de la cara: la lagrima pasa por la
// mejilla y pisa pixeles de la boca, asi que un rastreo parcial mentiria.
// ---------------------------------------------------------------------------
static unsigned long faseBase = 0;       // systemTime() del inicio del ciclo
static unsigned long ultimoCiclo = 0;    // indice del ciclo en curso
static unsigned long faseVista = 0;      // fase del frame anterior (detecta wrap)
static bool          baseDibujada = false;
static bool          avisoEnviado = false;

static uint8_t esperado[25];             // lo que CREEMOS que hay en pantalla
static int     ultimoBrillo = -1;
static int     ultimoOjoIzq = -1;
static int     ultimoOjoDer = -1;

// La lagrima: decision de CALENDARIO, no visual. Este contador baja una vez
// por ciclo (no una vez por frame) y conserva el "cada 4-7 pasadas" original.
// OJO: si bajara en cada frame, con 60 frames por ciclo caeria una lagrima
// POR SEGUNDO.
static int  hastaLagrima = 4;
static bool lagrimaEnEsteCiclo = false;

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
    for (int i = 0; i < 5; i++)
        setPixelTrazado(BOCA[i][0], BOCA[i][1], 255);

    ultimoOjoIzq = 255;
    ultimoOjoDer = 255;
    baseDibujada = true;
}

// ¿La cara sigue siendo la nuestra? 25 lecturas de un byte (~1 us), se paga
// en cada frame. Antes esto lo resolvia repintando la cara entera en cada
// pasada; ahora se comprueba y solo se repinta si hace falta, que es lo que
// pasaba cuando un loading se detenia o un comando desconocido imprimia "?"
// en la pantalla. Tambien es la red de seguridad si una lagrima se aborta a
// mitad y deja la mejilla manchada.
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

// Respira LENTA: RAMPA LINEAL, no seno. El original hacian escalones de 4
// unidades cada 45 ms; un triangulo lineal se ve igual de bien (en un panel de
// 5x5 la curva no aporta nada visible) y ademas coincide con el guion.
static int brilloRespira(float p)
{
    if (p < RESP_SUBE)  return 45 + (int)((100 - 45) * p / RESP_SUBE);
    if (p < RESP_BAJA)  return 100 - (int)((100 - 45) * (p - RESP_SUBE)
                                           / (RESP_BAJA - RESP_SUBE));
    return BRILLO_REPOSO;
}

// Parpadeo PESADO: los DOS ojos con el MISMO valor (el original los movia
// juntos a proposito: en serie se veria desincronizado). Cierra lento, queda
// cerrado un rato, abre lento.
static int ojosPesados(float p)
{
    if (p < PARP_CIERRA)  return 255 - (int)(255 * p / PARP_CIERRA);
    if (p < PARP_CERRADO) return 0;
    if (p < PARP_ABRE)    return (int)(255 * (p - PARP_CERRADO)
                                       / (PARP_ABRE - PARP_CERRADO));
    return 255;
}

// ---------------------------------------------------------------------------
// La LAGRIMA
//
// 4 sub-tramos, con los mismos tiempos que el original:
//   juntarse bajo el ojo (210 ms) + destello (160) + caida (300) + borrar (90)
//
// De que ojo cae sale de pseudo(ciclo), o sea que es el MISMO ojo durante
// todo el ciclo (no parpadea de un lado al otro en cada frame) y es
// diferente en cada ciclo.
// ---------------------------------------------------------------------------
static void dibujarLagrima(float p, unsigned long ciclo)
{
    int ex = (pseudo(ciclo) & 1) ? 3 : 1;

    if (p < 0.4884f) {                       // 370/760: se junta + destello
        if (p < 0.2763f)                     // 210/760
            setPixelTrazado(ex, 2, (int)(220 * p / 0.2763f));
        else if (p < 0.3553f)                // 90/760: el destello
            setPixelTrazado(ex, 2, 110);
        else
            setPixelTrazado(ex, 2, 235);
        return;
    }

    if (p < 0.8816f) {                       // 300/760: cae por la mejilla
        if (p < 0.6711f) {                   // 140/760
            setPixelTrazado(ex, 2, 90);
            setPixelTrazado(ex, 3, 240);
        } else {
            setPixelTrazado(ex, 3, 90);
            setPixelTrazado(ex, 4, 240);
        }
        return;
    }

    // 90/760 al final: se apaga y limpia la estela. OJO: (ex,3) y (ex,4) son
    // pixeles de la BOCA (x=1 o 3), asi que hay que devolverlos a 255 o la
    // cara queda con un agujero en el labio.
    setPixelTrazado(ex, 4, 0);
    setPixelTrazado(ex, 2, 0);
    setPixelTrazado(BOCA[2][0], BOCA[2][1], 255);
    setPixelTrazado(BOCA[4][0], BOCA[4][1], 255);
}

// ---------------------------------------------------------------------------
// El primer frame, para las TRANSICIONES y CALLA. Ancla el ciclo.
// ---------------------------------------------------------------------------
void mostrarCaraTriste()
{
    uBit.display.setBrightness(BRILLO_REPOSO);
    dibujarBase();
    ultimoBrillo = BRILLO_REPOSO;
    faseBase = uBit.systemTime();
    faseVista = 0;
    ultimoCiclo = 0;
    avisoEnviado = false;
    lagrimaEnEsteCiclo = false;
}

// ---------------------------------------------------------------------------
// UN FRAME. La llama el bucle principal ~60 veces por segundo.
// ---------------------------------------------------------------------------
void animarTriste()
{
    if (revisarSerial()) return;

    // La primera vez (o si otra cosa toco la pantalla) se dibuja la base.
    if (!baseDibujada || !caraIntacta())
        dibujarBase();

    unsigned long ahora = uBit.systemTime();
    unsigned long transcurrido = ahora - faseBase;
    unsigned long ciclo = transcurrido / CICLO_MS;
    unsigned short t = (unsigned short)(transcurrido % CICLO_MS);

    // --- La fase dio la vuelta: aqui se decide si este ciclo lleva lagrima --
    if (t < faseVista) {
        lagrimaEnEsteCiclo = (--hastaLagrima <= 0);
        if (lagrimaEnEsteCiclo) {
            avisoEnviado = false;                          // esta puede avisar
            hastaLagrima = 4 + (int)(pseudo(ciclo) % 4);   // 4-7 ciclos
        }
    }
    faseVista = t;

    if (ciclo != ultimoCiclo) {
        ultimoCiclo = ciclo;
        // OJO: el printf de CODAL solo soporta %d y %s (Serial.cpp:425), NO
        // %lu. Con %lu el contador de debug salia vacio.
        uBit.serial.printf("T%d\n", (int)(ciclo % 1000));
    }

    // Localiza el segmento (7 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &CICLO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++) {
        if (t >= CICLO[i].desde && t < CICLO[i].hasta) { seg = &CICLO[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    // --- Ojos: mismo valor para los dos (el original los movia juntos) ---
    int ojos = (seg->curva == C_PARP_PESADO) ? ojosPesados(p) : 255;
    if (ojos != ultimoOjoIzq) {
        setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], ojos);
        ultimoOjoIzq = ojos;
    }
    if (ojos != ultimoOjoDer) {
        setPixelTrazado(OJO_DER[0], OJO_DER[1], ojos);
        ultimoOjoDer = ojos;
    }

    // --- Brillo global: solo la respiracion lo mueve ---
    int brillo = (seg->curva == C_RESP_LENTA) ? brilloRespira(p) : BRILLO_REPOSO;
    // Zona muerta: el quantum del PWM avanza de a saltos de ~1,22 unidades
    // (quantum = 0.8169 * brillo), asi que 1-2 unidades no se ven. Y
    // setBrightness() en el M0+ hace una division entera por software.
    if (brillo - ultimoBrillo >= 3 || ultimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        ultimoBrillo = brillo;
    }

    // --- La boca: tiembla, o la lagrima la pisa ---
    if (seg->curva == C_TIEMBLA) {
        // El temblor se re-ranura cada ~50 ms, como el original (que
        // dormia 50 ms por paso). El valor sale del hash del tiempo: mismo
        // instante, mismo temblor, sin estado.
        unsigned int semilla = pseudo(ciclo * 1000u + (unsigned int)(t / 50));
        int medio = 190 + (int)(semilla % 66);      // 190..255
        int izq   = 200 + (int)((semilla >> 8) % 56);
        setPixelTrazado(1, 3, izq);
        setPixelTrazado(2, 3, medio);
        setPixelTrazado(3, 3, izq);
        // Las esquinas quedan firmes (como en el original).
    } else {
        // Boca en reposo. Se escribe SOLO si hace falta: durante la lagrima
        // este bloque corre antes y la lagrima lo pisa, y al terminar el
        // tramo la lagrima devuelve los pixeles que toco.
        if (esperado[3 * 5 + 1] != 255) setPixelTrazado(1, 3, 255);
        if (esperado[3 * 5 + 2] != 255) setPixelTrazado(2, 3, 255);
        if (esperado[3 * 5 + 3] != 255) setPixelTrazado(3, 3, 255);
    }

    // --- La lagrima: solo en los ciclos que le tocan ---
    if (seg->curva == C_LAGRIMA && lagrimaEnEsteCiclo) {
        if (!avisoEnviado) {
            uBit.serial.send("LAGRIMA\n");     // debug: confirmar por serial
            avisoEnviado = true;
        }
        dibujarLagrima(p, ciclo);
    }

    uBit.sleep(16);   // ~60 fps
}
