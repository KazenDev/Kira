/**
 * HablarNeutral.cpp - La boca del NEUTRAL que HABLA (el "bruh" habla)
 *
 * ─────────────────────────────────────────────────────────────────────────
 * PATRON DE FRAME (la boca): un tramo del tic por frame, el estado en el reloj
 * ─────────────────────────────────────────────────────────────────────────
 *
 * ANTES esta boca era una cadena de fiber_sleep() y el bucle principal se
 * comia 840 ms SIN mirar el serial (medido: 820 ms de ventana sorda). Y pegaba
 * mas de lo que parece, porque Principal.cpp le da PRIORIDAD a TALK sobre la
 * animacion de la emocion: mientras la IA habla, esta boca es TODO lo que corre
 * en el hilo principal.
 *
 * Ahora animarBocaNeutral() es una FUNCION DE FRAME: un frame (~16 ms) del tic
 * y vuelve. El estado vive en systemTime(), asi que el comando de la IA se nota
 * en el frame siguiente. Mismo patron que el resto.
 *
 * ── OJO: ESTA BOCA ES IDENTICA A LA DE ALEGRIA ──────────────────────────
 * Mismos 3 pixeles (1,3)(2,3)(3,3), mismo tic de 280 ms, mismo rango 0..255.
 * Lo unico que cambia es la cara de alrededor (ojos cerrados con peeks) y el
 * brillo base (85 en vez de 90).
 *
 * O sea que el "bruh" de este personaje esta 100% en los OJOS, y la boca
 * habla con la misma energia que la del personaje feliz. Eso es discutible, y
 * hay dato para discutirlo (ver abajo).
 *
 * ── PENDIENTE DE TU OJO (esta a una linea) ──────────────────────────────
 * La literatura de animacion de habla emocional dice que el arousal y la
 * ARTICULACION van juntos: "menor arousal se correlaciona con menor
 * intensidad, y aparece en emociones mas suaves o de baja intensidad como la
 * tristeza O EL ABURRIMIENTO. De la misma manera, menor frecuencia e
 * intensidad implican ARTICULACION MAS DEBIL". El fastidio de este
 * proyecto ya lo hace asi (pulsa entre 60 y 240), asi que el "bruh" podria
 * hacer lo mismo.
 *
 * PERO la misma fuente pone un piso: "suprimir toda la activacion da una cara
 * estatica, apenas alcanzable por los ventrilocuas mas expertos... el habla
 * natural sin activacion se parece mas a un murmullo". O sea que la boca NO
 * puede quedarse quieta: se veria rota. Hay un medio camino, y es lo que
 * ofrece BRILLO_MAXO.
 *
 * Con 255 (el valor actual) esta boca es identica a la de Alegria. Con 200
 * seria hipoarticulada, en el mismo rango que el fastidio, y con el "bruh"
 * del personaje. No se cambio por decision propia: es diseno del personaje.
 */
#include "HablarNeutral.h"
#include "../Alegria/Hablar.h"   // modoHablar (compartido)

// ---------------------------------------------------------------------------
// Posiciones (mismas que Neutral.cpp)
static const uint8_t PARPADO_IZQ[2] = {0, 1};
static const uint8_t PARPADO_DER[2] = {4, 1};

// Los 3 LEDs de la boca recta (los que parpadean al hablar)
static const uint8_t BOCA[3][2] = {
    {1, 3}, {2, 3}, {3, 3}
};

// El tope de brillo del "bruh" hablador. 190 = hipoarticulado.
//
// POR QUE 190 Y NO 0: Fastidio, que tambien es de arousal bajo, pulsa entre
// 60 y 240 y NUNCA se cierra del todo, porque es un sonido "entre dientes".
// Un neutral no es eso: una boca que no se cierra nunca entre silabas se lee
// como rareza, no como aburrimiento. Asi que aqui se baja la AMPLITUD (cada
// silaba se nota menos) pero se deja que la boca se cierre del todo, que es
// lo natural entre palabras. La direccion la da la literatura ("menor arousal
// implica articulacion mas debil"); el numero exacto es criterio.
//
// OJO: la amplitud es solo la mitad del "bruh". La otra mitad es el RITMO, que
// antes venía atado a la tabla. Ver TIC_MS mas abajo.
#define BRILLO_MAXO 190

// ---------------------------------------------------------------------------
// EL TIC COMO TABLA
//
// Un tic de habla neutral: los 3 LEDs se encienden, quedan, apagan y quedan
// apagados. Las proporciones son las del tic original de Alegria
// (60+80+60+80), asi que cambiar el ritmo NO cambia la forma del gesto.
//
// Los bordes van como FRACCIONES de TIC_MS y no en milisegundos, para que el
// ritmo sea UNA sola constante. Antes estaban duplicados en dos lugares (la
// tabla y valorEn()), y por eso cambiar la velocidad no era de una linea:
// habia que editar los dos sin que dejaran de concordar.
//
// El tic original era de 280 ms con bordes en 0 / 60 / 140 / 200 / 280, o sea
//   0,000 -> 0,214  sube
//   0,214 -> 0,500  quieto
//   0,500 -> 0,714  baja
//   0,714 -> 1,000  oscuro
// ---------------------------------------------------------------------------
enum Tramo
{
    T_ENCIENDE = 0,
    T_QUIETO,
    T_APAGA,
    T_OSCURO
};

#define NTRAMOS 4
static const float BORDE[] = { 0.0000f, 0.2143f, 0.5000f, 0.7143f, 1.0000f };
static const unsigned char TRAMO[NTRAMOS] = { T_ENCIENDE, T_QUIETO, T_APAGA, T_OSCURO };

// EL RITMO DEL "BRUH". Con 280 ms eran 3,6 silabas/s, que es lo MISMO que
// Alegria: por eso la cara se leia como una Alegria con los ojos cerrados, y
// bajar solo la amplitud no alcanzaba.
//
// Con 360 ms son 2,8 silabas/s. La escala de las ocho bocas queda:
//
//     Enojado      170 ms -> 5,9   (arousal alto)
//     Sorprendido  235 ms -> 4,3
//     Fastidio     248 ms -> 4,0
//     Alegria      280 ms -> 3,6   (ritmo de habla normal, ~4,0)
//     Triste       300 ms -> 3,3
//     Neutral      360 ms -> 2,8   <- aca: aburrido, por debajo de Alegria
//     Cansado      460 ms -> 2,2       y por encima del sueño
//
// 2,8 queda por debajo de Triste y por encima de Cansado, que es lo que
// corresponde: el fastidio es arousal bajo (como la tristeza) pero el
// personaje esta DESPIERTO, no dormido. Es un 29% mas lento que antes, un
// paso que se nota sin tirar el ritmo al extremo.
//
// Es el numero mas "de gusto" de los cinco ajustes: la banda la dan los
// datos, el valor exacto no. Con 340 (3,0) el contraste es menor; con 400
// (2,5) empieza a meterse en territorio de Cansado.
#define TIC_MS 360

static unsigned long faseBase = 0;
static int ultimoGesto = -1;

// El brillo de los 3 LEDs. p va de 0 a 1 DENTRO del tramo, asi que cada
// tramo es solo una rampa y no necesita saber los bordes.
static int valorEn(unsigned char tramo, float p)
{
    switch (tramo)
    {
        case T_ENCIENDE: return (int)(BRILLO_MAXO * p);
        case T_QUIETO:   return BRILLO_MAXO;
        case T_APAGA:    return BRILLO_MAXO - (int)(BRILLO_MAXO * p);
        default:         return 0;
    }
}

// ---------------------------------------------------------------------------
// FIBRA: peeks de ojos en paralelo (SIN CAMBIOS: ya cede la CPU, rompe el
// loop sola y no procesa comandos)
// ---------------------------------------------------------------------------

// PEEK sincronizado: ambos parpados se mueven JUNTOS paso a paso.
//   cerrar=false -> ENTREABRIR: los parpados se apagan 255 -> 0
//   cerrar=true  -> CERRAR: los parpados suben 0 -> 255
static void peekOjosNeutral(bool izq, bool der, int steps, int delayMs, bool cerrar)
{
    for (int s = 0; s <= steps && modoHablar; s++) {
        int p = cerrar ? 255 * s / steps : 255 * (steps - s) / steps;
        if (izq) uBit.display.image.setPixelValue(PARPADO_IZQ[0], PARPADO_IZQ[1], p);
        if (der) uBit.display.image.setPixelValue(PARPADO_DER[0], PARPADO_DER[1], p);
        fiber_sleep(delayMs);
    }
}

static void fiberPeekNeutral(void)
{
    while (modoHablar) {
        // ~75% ambos, ~12% peek izquierdo, ~12% peek derecho
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 88)      { izq = false; }
        else if (r >= 75) { der = false; }

        peekOjosNeutral(izq, der, 4, 30, false);   // entreabrir (~120ms)
        fiber_sleep(350);                          // mirando (solo pupilas)
        peekOjosNeutral(izq, der, 4, 30, true);    // cerrar

        int espera = 1500 + uBit.random(3500);    // 1.5s a 5s
        for (int i = 0; i < espera / 100 && modoHablar; i++)
            fiber_sleep(100);
    }
    release_fiber();
}

// ---------------------------------------------------------------------------
// La cara base para hablar: ojos cerrados + boca recta
// ---------------------------------------------------------------------------
static void dibujarCaraNeutral()
{
    uBit.display.setBrightness(85);
    uBit.display.image.clear();
    uBit.display.image.setPixelValue(PARPADO_IZQ[0], PARPADO_IZQ[1], 255);
    uBit.display.image.setPixelValue(PARPADO_DER[0], PARPADO_DER[1], 255);
    uBit.display.image.setPixelValue(1, 1, 255);   // pupilas
    uBit.display.image.setPixelValue(3, 1, 255);
    for (int i = 0; i < 3; i++)
        uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], 255);
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK (con neutral activo): prepara la cara y lanza la fibra de peeks
void iniciarHablarNeutral()
{
    // Si ya estamos hablando (con cualquier emocion), NO crear otra fibra
    if (modoHablar) return;

    modoHablar = true;
    dibujarCaraNeutral();
    uBit.serial.send("TALK-NEU\n");   // debug: confirmar el dispatch
    faseBase = uBit.systemTime();     // el tic arranca ahora
    ultimoGesto = -1;                // fuerza la primera escritura
    create_fiber(fiberPeekNeutral);
}

// UN frame de la boca recta. La llama el bucle principal ~60 veces por
// segundo mientras modoHablar siga activo.
void animarBocaNeutral()
{
    // t cabe en un int (siempre < TIC_MS), asi que se trabaja en int para no
    // mezclar signos en las comparaciones.
    int t = (int)((uBit.systemTime() - faseBase) % TIC_MS);

    // Localiza el tramo. Los bordes se calculan con multiplicaciones sobre
    // TIC_MS, que es constante de compilacion, asi que no aparecen divisiones.
    int desde = 0;
    int hasta = (int)(BORDE[1] * TIC_MS);
    unsigned char tramo = TRAMO[0];
    for (int i = 1; i < NTRAMOS; i++) {
        int b = (int)(BORDE[i] * TIC_MS);
        if (t >= b) {
            desde = b;
            hasta = (i + 1 < NTRAMOS) ? (int)(BORDE[i + 1] * TIC_MS) : TIC_MS;
            tramo = TRAMO[i];
        }
    }
    float p = (float)(t - desde) / (float)(hasta - desde);

    int v = valorEn(tramo, p);
    if (v != ultimoGesto) {
        for (int i = 0; i < 3; i++)
            uBit.display.image.setPixelValue(BOCA[i][0], BOCA[i][1], (uint8_t)v);
        ultimoGesto = v;
    }

    // fiber_sleep() y uBit.sleep() son la MISMA llamada (CodalDevice.cpp:30
    // -> fiber_sleep). Cede la CPU a la fibra de los peeks, al replicador del
    // LED y al stack de BLE. 16 ms = el techo del display (60 Hz).
    fiber_sleep(16);
}
