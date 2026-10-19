/**
 * Cara.h - El MOTOR DE VIDA: una cara que decide que hacer en cada instante
 *
 * Antes cada emocion era un GUION fijo (un ciclo de 2-7 s que se repetia
 * identico). Ahora cada emocion es una PERSONALIDAD (numeros + una funcion
 * que dibuja) y el motor la hace vivir por capas, de la mas lenta a la mas
 * rapida:
 *
 *   1. RESPIRACION   brillo que sube y baja con ruido suave (nunca un seno
 *                    perfecto: eso se lee como maquina).
 *   2. OJOS          parpadeos y miradas sorteados con estadistica real
 *                    (Ojos.h). La inclinacion de la placa tira de la mirada.
 *   3. GESTOS        el repertorio de la emocion (guino, suspiro, lagrima,
 *                    giro de ojos...). Se sortean por peso, con enfriamiento
 *                    y sin repetir el ultimo: nunca hace la misma secuencia.
 *   4. HABLA         la boca de la emocion movida por la prosodia sintetica
 *                    (Habla.h) cuando la IA esta hablando.
 *   5. REACCIONES    tocar el logo, sacudirla, un golpe, dejarla sola mucho
 *                    rato. Entran rapido (120 ms) y se van suave (350 ms)
 *                    de vuelta al animo de base. Si se repiten, escalan.
 *
 * El nucleo NO conoce CODAL: recibe el tiempo y los sensores, y devuelve un
 * Lienzo y una lista de eventos (para los sonidos). Por eso se testea entero
 * en el PC (Bench/test_vida.cpp) con una semilla fija.
 */
#ifndef VIDA_CARA_H
#define VIDA_CARA_H

#include <stdint.h>
#include "Azar.h"
#include "Ruido.h"
#include "Lienzo.h"
#include "Ojos.h"
#include "Habla.h"

#define MAX_GESTOS 8

// Cuando puede aparecer un gesto
enum FlagGesto
{
    G_REPOSO   = 1,   // solo con la boca callada
    G_HABLANDO = 2,   // tambien mientras habla
    G_AMBOS    = 3
};

struct DefGesto
{
    uint16_t durMin, durMax;   // ms
    uint8_t  peso;             // probabilidad relativa
    uint8_t  flags;            // FlagGesto
    uint16_t enfriarMs;        // tiempo minimo antes de repetirlo
    uint8_t  sonido;           // SonidoId para el pegamento (0 = mudo)
};

enum Reaccion
{
    R_NINGUNA = 0,
    R_COSQUILLAS,     // tocar el logo: se rie
    R_CARINO,         // tocarla mucho o mantener el dedo: corazon
    R_GRUNIDO,        // tocar a Kira enojada: "no me toques"
    R_SOBRESALTO,     // un golpe (o tocar a Kira con miedo)
    R_DESPERTAR,      // tocar a Kira cansada o dormida
    R_MAREO,          // sacudirla
    R_DORMIR,         // nadie le hizo caso en mucho rato (o la taparon)
    // --- Animo (Eilik/EMO): como la tratan cambia como reacciona ---
    R_TOQUE,          // picarla con B: "¿mm?"
    R_MOLESTA,        // picarla de mas / demasiadas cosquillas: "¡ey!"
    R_BERRINCHE,      // tres llamas: el volcan
    R_OFENDIDA,       // ofendida: no te mira, "hmph"
    R_ENAMORADA,      // tres flores: ramo de corazones
    R_PERDON,         // la acariciaste estando ofendida: te perdona
    // --- Clima y entorno ---
    R_ESTORNUDO,      // frio: "a-a-¡achis!"
    R_TIRITAR,        // frio: "brrr"
    R_ACALORADA,      // calor: jadea y se abanica, "uff"
    R_DESLUMBRADA,    // volvio la luz de golpe
    R_CAIDA,          // caida libre: "¡aaah!"
    NUM_REACCIONES
};

enum Clima
{
    CLIMA_NORMAL = 0,
    CLIMA_CALOR,
    CLIMA_FRIO
};

// Umbrales del termometro (es el del CHIP: marca ~2-4 grados mas que el
// ambiente con la placa andando). Histeresis para no titilar en el borde.
#define CALOR_ENTRA  33
#define CALOR_SALE   31
#define FRIO_ENTRA   14
#define FRIO_SALE    16

// Animo: cada flor o llama se va sola a los 2 min (EMO)
#define ANIMO_MAX     3
#define ANIMO_DURA_MS 120000u

// Eventos del frame (bits): el pegamento los traduce a sonido y serial
enum EventoCara
{
    EV_GESTO    = 1,   // empezo el gesto c.eventoGesto
    EV_REACCION = 2,   // empezo la reaccion c.eventoReaccion
    EV_FIN_FRASE = 4,  // la boca hizo una pausa de frase
    EV_ENTRADA  = 8    // la emocion acaba de llegar
};

// Lo que los sentidos de la placa le cuentan a la cara en este frame
struct Estimulos
{
    bool  hayAcel;          // hay muestra nueva del acelerometro
    float ax, ay, az;       // mili-g
    bool  toque;            // logo tocado AHORA
    bool  sacudida;         // CODAL detecta sacudida AHORA
    bool  caida;            // CODAL detecta caida libre AHORA
    bool  picar;            // toque corto del boton B en este frame
    bool  hayTemp;          // lectura nueva del termometro
    int   temp;             // grados (del chip)
    bool  hayLuz;           // lectura nueva de luz
    int   luz;              // 0..255
};

struct EstadoCara;

// Desplazamiento y brillo de toda la cara en este frame (temblor, rebote...)
struct Pose
{
    float dx, dy;
    float brillo;
};

struct Personalidad
{
    ParamOjos  ojos;
    ParamHabla habla;
    const DefGesto *gestos;
    uint8_t nGestos;
    float gestoMs, gestoCv;     // intervalo medio entre gestos
    float respiraHz, respiraProf;
    float intensidad;           // luz pico (lineal) de la cara
    float temblor;              // temblor permanente (miedo)
    int8_t gestoEntrada;        // gesto al llegar la emocion (-1 ninguno)
    uint8_t reaccionToque;      // que hace si la tocan
    void (*dibujar)(Lienzo &l, const EstadoCara &c, const Pose &pose);
};

struct EstadoCara
{
    const Personalidad *p;
    Azar azar;
    uint32_t semilla;
    uint32_t t, t0, tPrev;
    float dt;

    EstadoOjos  ojos;
    EstadoHabla habla;

    // Gestos
    int8_t   gesto;             // -1 = ninguno
    uint32_t gestoInicio, gestoDur;
    uint32_t proxGesto;
    uint32_t ultimoUso[MAX_GESTOS];
    float    usos[MAX_GESTOS];
    int8_t   ultimoGesto;
    float    gp;                // progreso del gesto 0..1

    bool hablando;
    float respira;              // -1..1

    // Reaccion activa
    uint8_t  reaccion;
    uint32_t reaccionInicio, reaccionFin;   // fin = 0: indefinida
    float    reaccionPeso;      // 0..1 (fundido con la cara de base)

    // Memoria de los sentidos
    bool     toqueAntes;
    uint32_t toqueDesde;
    uint8_t  toquesSeguidos;
    uint32_t ultimoToque;
    uint32_t sacudidaDesde;
    bool     sacudiendo;
    uint32_t ultimoGolpe;
    bool     baseLista;
    float    baseX, baseY, baseZ;
    float    prevX, prevY, prevZ;
    float    inclX, inclY;      // desvio de la pose de reposo (-1..1)
    uint32_t ultimaActividad;
    bool     sentidosActivos;
    uint32_t ultimaCaida;

    // Animo: flores (mimos) y llamas (molestias), con su hora de llegada
    uint32_t flores[ANIMO_MAX];
    uint8_t  nFlores;
    uint32_t llamas[ANIMO_MAX];
    uint8_t  nLlamas;
    uint32_t ofendidaHasta;     // 0 = no esta ofendida
    float    ofendidaLado;      // hacia donde mira para no mirarte
    uint8_t  picadas;           // toques de B seguidos
    uint32_t ultimaPicada;
    bool     caricia;           // ya sumo la flor de este dedo apoyado
    bool     despertoBrusco;    // la despertaron con B (sale de mal humor)

    // Clima
    uint8_t  clima;
    int8_t   climaForzado;      // -1 = automatico (termometro)
    uint8_t  climaCandidato;
    uint32_t climaDesde;
    int      temp;
    bool     tempLista;
    uint32_t proxClima;         // proxima reaccion de clima (estornudo, uff)
    uint32_t proxGota;          // proxima gota de sudor
    uint32_t gotaInicio;
    uint8_t  ultimaDeFrio;      // estornudo o tiritar: se alternan

    // Luz
    float    luzBase;
    bool     luzLista;
    bool     oscuro;
    uint32_t oscuroDesde;
    bool     dormidaPorOscuro;

    // Eventos de este frame (lo emitido se publica en el frame siguiente,
    // asi tambien llegan los que nacen en caraIniciar o caraHablar)
    uint8_t eventos;
    int8_t  eventoGesto;
    uint8_t eventoReaccion;
    uint8_t pendEventos;
    int8_t  pendGesto;
    uint8_t pendReaccion;
};

// Arranca una emocion. conEntrada = hace su gesto de llegada.
void caraIniciar(EstadoCara &c, const Personalidad *p, uint32_t t,
                 uint32_t semilla, bool conEntrada);
// Cambia de personalidad conservando el azar, el habla y los sentidos
void caraCambiar(EstadoCara &c, const Personalidad *p, uint32_t t, bool conEntrada);
void caraHablar(EstadoCara &c, bool hablar, uint32_t t);
// Un comando de la IA o un humano: la cara "se entera" (corta el aburrimiento)
void caraActividad(EstadoCara &c, uint32_t t);
void caraForzarGesto(EstadoCara &c, int idx, uint32_t t);
void caraForzarReaccion(EstadoCara &c, uint8_t r, uint32_t t);
// CLIMA:CALOR / CLIMA:FRIO / CLIMA:AUTO (para mostrarlo en la feria)
void caraForzarClima(EstadoCara &c, int clima, uint32_t t);
bool caraOfendida(const EstadoCara &c);

// UN frame: avanza todo y compone la cara en 'out'
void caraFrame(EstadoCara &c, uint32_t t, const Estimulos &est, Lienzo &out);

// Que tan aburrida esta (0 = recien atendida, 1 = a punto de dormirse)
float caraAburrimiento(const EstadoCara &c);

// --- Pinceles compartidos por las emociones --------------------------------

// Un ojo de UN pixel que mira y parpadea (el parpado lo baja un poco).
void ojoPunto(Lienzo &l, const EstadoCara &c, float bx, float by,
              float apertura, float nivel, const Pose &pose);
// Un ojo ALTO de 2 pixeles (filas by-1 y by): el parpado lo achica de arriba.
void ojoAlto(Lienzo &l, const EstadoCara &c, float bx, float by,
             float apertura, float nivel, const Pose &pose);
// Rasgos fijos (boca, cejas) con el desplazamiento de la pose
void rasgo(Lienzo &l, const uint8_t (*pts)[2], int n, float nivel, const Pose &pose);
void punto(Lienzo &l, float x, float y, float nivel, const Pose &pose);

// Las reacciones se dibujan en Reacciones.cpp
void dibujarReaccion(Lienzo &l, const EstadoCara &c, uint8_t r, uint32_t desde);
uint32_t duracionReaccion(uint8_t r);

#endif // VIDA_CARA_H
