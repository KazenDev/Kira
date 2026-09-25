/**
 * Enojado.cpp - La emocion ENOJADO 😠 (en reposo)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME: una llamada = un frame (~60 fps), el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * Antes era una SECUENCIA de ~128 sleep() con 7 checkpoints en 4,6 segundos.
 * La fase "las cejas se fruncen" son 1.110 ms SIN un solo revisarSerial(), la
 * mayor de las ocho emociones. Medido con Bench/bench_emociones.cpp.
 *
 * Ahora el estado vive en systemTime(): la animacion es una funcion pura del
 * tiempo y el comando de la IA se nota en el frame siguiente. Sin azar: todo
 * sale de la tabla de segmentos y de la fase.
 *
 * ── LO QUE SE DEJO INTACTO A PROPOSITO ──────────────────────────────────
 * Las tres oleadas de cejas se ACELERAN (400 / 370 / 340 ms). Eso ya es una
 * escalada, y la escalada es justamente lo que hace creible un enojo: las
 * guias de animacion de enojo dicen que "no solo intensifica el estallido
 * final, tambien lo hace mas creible porque el espectador siente la
 * progresion emocional". No se toco.
 *
 * El parpadeo BRUSCO se queda por escalones (255, 127, 0) y no se le
 * suaviza: un golpe tiene que resolver en pocos frames, y estirarlo le
 * quita potencia.
 *
 * ── PENDIENTE DE TU OJO (esta a una linea) ──────────────────────────────
 * ARC_CEJA: el original enciende las dos cejas interiores DE GOLPE al
 * fruncirse. Las guias de animacion de cejas dicen que el movimiento tiene
 * que ir en ARCO, no lineal: la ceja exterior adelanta y la interior la
 * sigue, con un retraso de unos 40 ms. Con ARC_CEJA = 0 queda igual que el
 * original; con el valor de abajo, la derecha sigue a la izquierda. Es
 * sutil, y es tu cara: pruobalo y decides.
 */
#include "Enojado.h"
#include "../../Sistema/Sistema.h"

// ---------------------------------------------------------------------------
// Posiciones de la cara en la matriz 5x5 (x, y)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Cejas (esquinas) y su extension al fruncirse (hacia el centro)
static const uint8_t CEJA_IZQ[2] = {0, 0};
static const uint8_t CEJA_DER[2] = {4, 0};
static const uint8_t CEJA_IZQ_F[2] = {1, 0};
static const uint8_t CEJA_DER_F[2] = {3, 0};

// Boca con dientes: linea de dientes (1,3)(2,3)(3,3) + mandibula
// (0,4)(1,4)(3,4)(4,4). El hueco (2,4) se cierra al APRETAR.
static const uint8_t BOCA[7][2] = {
    {1,3}, {2,3}, {3,3}, {0,4}, {1,4}, {3,4}, {4,4}
};

// El hueco de los dientes: en reposo apagado, y pulsando cuando aprieta.
#define DIENTE 2
#define FILA_DIENTE 4

#define BRILLO_REPOSO 95      // el enojo es INTENSO (no tenue)

// Retraso de la ceja interior derecha respecto de la izquierda. 0 = el
// original (las dos de golpe). Ver la nota del encabezado.
#define ARC_CEJA 0.036f       // ~40 ms dentro del tramo de 1110 ms

// ---------------------------------------------------------------------------
// EL CICLO COMO TABLA.
// 984 + 350 + 100 + 1110 + 690 + 350 + 984 = 4568 ms
// ---------------------------------------------------------------------------
enum Curva
{
    C_PLANO = 0,        // todo quieto
    C_RESPIRA_AGITADA,  // brillo sube/baja RAPIDO (2 rampas de 432 ms)
    C_PARP_BRUSCO,      // cierra 3 escalones, 80 ms, abre 3 escalones
    C_CEJAS,            // 3 oleadas que se ACELERAN
    C_BOCA_APRIETA      // el hueco de los dientes pulsa
};

struct Segmento
{
    unsigned short desde;
    unsigned short hasta;
    unsigned char  curva;
};

static const Segmento CICLO[] = {
    {   0,  984, C_RESPIRA_AGITADA },
    { 984, 1334, C_PARP_BRUSCO    },
    {1334, 1434, C_PLANO          },
    {1434, 2544, C_CEJAS          },
    {2544, 3234, C_BOCA_APRIETA   },
    {3234, 3584, C_PARP_BRUSCO    },
    {3584, 4568, C_RESPIRA_AGITADA },
};
static const int NCICLOS = sizeof(CICLO) / sizeof(CICLO[0]);
static const unsigned short CICLO_MS = 4568;

// Donde empieza y donde acaba el tramo de las cejas. Fuera de el, las cejas
// quedan fruncidas hasta que el ciclo da la vuelta (que es lo que hacia el
// original: al terminar la 3ra oleada quedaban fruncidas el resto de la
// pasada, y se relajaban de golpe al arrancar la siguiente).
#define CEJAS_DESDE 1434

// Umbrales de las 3 oleadas, como fraccion del tramo de 1110 ms.
//   oleada 0: relajada 200 ms, fruncida 200 ms
//   oleada 1: relajada 200 ms, fruncida 170 ms
//   oleada 2: relajada 200 ms, fruncida 140 ms
#define OLA0_A 0.1802f
#define OLA0_B 0.3604f
#define OLA1_A 0.5405f
#define OLA1_B 0.6946f
#define OLA2_A 0.8739f

// El parpadeo brusco: 3 escalones de 20 ms para cerrar (255, 127, 0), 80 ms
// cerrados, 3 escalones para abrir (0, 127, 255) y 150 ms de reposo.
// 20 ms de 350 = 0.0571
#define BRUCO_P1 0.0571f
#define BRUCO_P2 0.1143f
#define BRUCO_P3 0.4571f   // 3er escalon de cierre + 80 ms + 1ro de apertura
#define BRUCO_P4 0.5143f

// La mandibula: 6 pulsos de 90 ms (540) y 150 ms apretada. 90 de 690 = 0.1304
#define DIENTE_PULSO 0.1304f

// ---------------------------------------------------------------------------
// Estado: el reloj, el ciclo en curso y lo ultimo escrito. Se rastrean LOS
// 25 PIXELS: el enojo usa 14 (2 cejas de esquina + 2 interiores + 2 ojos +
// 7 de boca + el diente del hueco), asi que un rastreo de 7 seria mentira.
// ---------------------------------------------------------------------------
static unsigned long faseBase = 0;
static unsigned long ultimoCiclo = 0;
static bool          baseDibujada = false;

static uint8_t esperado[25];
static int     ultimoBrillo = -1;
static int     ultimoOjoIzq = -1;
static int     ultimoOjoDer = -1;
static int     ultimoCejaIzqF = -1;
static int     ultimoCejaDerF = -1;
static int     ultimoDiente = -1;

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
    for (int i = 0; i < 7; i++)
        setPixelTrazado(BOCA[i][0], BOCA[i][1], 255);

    ultimoOjoIzq = ultimoOjoDer = 255;
    ultimoCejaIzqF = ultimoCejaDerF = 0;
    ultimoDiente = 0;
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

// Respira AGITADA: dos rampas rapidas de 432 ms (55 <-> 145) y 120 ms quieto
// en 95. RAMPA y no seno: es un bucle, y la fase ya es la curva.
static int brilloRespiraAgitada(float p)
{
    if (p < 0.8780f) {                      // las dos rampas
        float r = p / 0.4390f;              // 0..2, la repeticion
        r = r - (float)(int)r;               // 0..1 dentro de la repeticion
        if (r < 0.5f) return 55 + (int)((145 - 55) * r * 2.0f);
        return 145 - (int)((145 - 55) * (r - 0.5f) * 2.0f);
    }
    return BRILLO_REPOSO;
}

// Parpadeo BRUSCO: 3 escalones para cerrar, 80 ms cerrados, 3 para abrir.
// Se queda escalonado a proposito: un golpe tiene que resolver en pocos
// frames; estirarlo le quita potencia.
static int ojosBruscos(float p)
{
    if (p < BRUCO_P1) return 255;
    if (p < BRUCO_P2) return 127;
    if (p < BRUCO_P3) return 0;
    if (p < BRUCO_P4) return 127;
    return 255;
}

// Las cejas interiores: 3 oleadas, cada una mas rapida que la anterior.
// Devuelve si la ceja interior izquierda esta fruncida. La derecha se
// consulta con un retraso (ARC_CEJA) para que la siga en arco.
static bool cejaIzqFruncida(float p)
{
    if (p < OLA0_A) return false;   // oleada 0 relajada
    if (p < OLA0_B) return true;    // oleada 0 fruncida
    if (p < OLA1_A) return false;   // oleada 1 relajada
    if (p < OLA1_B) return true;    // oleada 1 fruncida
    if (p < OLA2_A) return false;   // oleada 2 relajada
    return true;                   // oleada 2 fruncida (y queda fruncida)
}

// La mandibula aprieta: 6 pulsos y despues queda apretada.
static int dienteValor(float p)
{
    if (p >= 6 * DIENTE_PULSO) return 255;      // queda apretada
    int i = (int)(p / DIENTE_PULSO);
    return (i % 2) ? 255 : 60;
}

// ---------------------------------------------------------------------------
// El primer frame, para las TRANSICIONES y CALLA. Ancla el ciclo.
// ---------------------------------------------------------------------------
void mostrarCaraEnojado()
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
void animarEnojado()
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
        uBit.serial.printf("E%d\n", (int)(ciclo % 1000));
    }

    // Localiza el segmento (7 entradas: busqueda lineal, sin RAM extra).
    const Segmento *seg = &CICLO[NCICLOS - 1];
    for (int i = 0; i < NCICLOS; i++) {
        if (t >= CICLO[i].desde && t < CICLO[i].hasta) { seg = &CICLO[i]; break; }
    }
    float p = (float)(t - seg->desde) / (float)(seg->hasta - seg->desde);

    // --- Brillo global: solo lo mueve la respiracion agitada ---------------
    int brillo = (seg->curva == C_RESPIRA_AGITADA) ? brilloRespiraAgitada(p)
                                                   : BRILLO_REPOSO;
    // Zona muerta: el quantum del PWM avanza de a saltos de ~1,22 unidades
    // (quantum = 0.8169 * brillo). Aqui la rampa es larga (55 a 145), asi que
    // el deadband descarta el escalon mas chico de la original (8 unidades
    // por 18 ms) sin que se note.
    if (brillo - ultimoBrillo >= 3 || ultimoBrillo - brillo >= 3) {
        uBit.display.setBrightness(brillo);
        ultimoBrillo = brillo;
    }

    // --- Ojos: el parpadeo brusco ------------------------------------------
    int ojos = (seg->curva == C_PARP_BRUSCO) ? ojosBruscos(p) : 255;
    if (ojos != ultimoOjoIzq) {
        setPixelTrazado(OJO_IZQ[0], OJO_IZQ[1], ojos);
        ultimoOjoIzq = ojos;
    }
    if (ojos != ultimoOjoDer) {
        setPixelTrazado(OJO_DER[0], OJO_DER[1], ojos);
        ultimoOjoDer = ojos;
    }

    // --- Cejas: fruncen en el tramo de las oleadas y quedan fruncidas -----
    // hasta el final del ciclo (igual que el original). Antes de ese tramo
    // estan relajadas.
    bool izqF = false, derF = false;
    if (t >= CEJAS_DESDE) {
        if (seg->curva == C_CEJAS) {
            izqF = cejaIzqFruncida(p);
            derF = cejaIzqFruncida(p - ARC_CEJA);   // la derecha sigue en arco
        } else {
            izqF = derF = true;                    // ya fruncidas, no se relajan
        }
    }
    int vIzq = izqF ? 255 : 0, vDer = derF ? 255 : 0;
    if (vIzq != ultimoCejaIzqF) {
        setPixelTrazado(CEJA_IZQ_F[0], CEJA_IZQ_F[1], vIzq);
        ultimoCejaIzqF = vIzq;
    }
    if (vDer != ultimoCejaDerF) {
        setPixelTrazado(CEJA_DER_F[0], CEJA_DER_F[1], vDer);
        ultimoCejaDerF = vDer;
    }

    // --- La mandibula aprieta ---------------------------------------------
    int diente = (seg->curva == C_BOCA_APRIETA) ? dienteValor(p) : 0;
    if (diente != ultimoDiente) {
        setPixelTrazado(DIENTE, FILA_DIENTE, diente);
        ultimoDiente = diente;
    }

    uBit.sleep(16);   // ~60 fps
}
