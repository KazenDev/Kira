/**
 * HablarViejo.cpp - la version ANTERIOR de la boca de Alegria (git show),
 * solo para el bench. Con modoHablar y las funciones renombradas para
 * linkearse junto a la nueva. NO se compila en la firmware.
 */
/**
 * Hablar.cpp - La boca que HABLA (lip-sync sutil) ASINCRONO con FIBERS
 *
 * REDISENO (v3):
 *   - La boca YA NO abre la boca grande: las MEJILLAS (esquinas + curva
 *     de la sonrisa) se quedan QUIETAS. Solo la linea de DIENTES
 *     (1,3)(2,3)(3,3) parpadea a ritmo de habla (~5 veces por segundo),
 *     como cuando muestras los dientes al hablar.
 *   - Los OJOS parpadean IMPREDECIBLES: la fibra usa uBit.random() para
 *     decidir cuantos ms esperar (1.5s-5s) y QUE parpadear: ~75% ambos
 *     ojos a la vez, ~12% guino izquierdo, ~12% guino derecho.
 *
 * ARQUITECTURA DE FIBRAS (CODAL):
 *   - El bucle principal (Principal.cpp) mueve la linea de dientes
 *   - Una FIBRA (create_fiber) parpadea los ojos EN PARALELO
 *   fiber_sleep() cede la CPU -> las dos animaciones corren a la vez.
 *   Los dientes tocan la fila 3; los ojos tocan (1,1) y (3,1).
 *   No se pisan -> sin condiciones de carrera.
 *
 * Lo activa la IA con "TALK". Otra emocion -> detenerHablarVieja() apaga
 * modoHablarVieja y la fibra muere sola en su siguiente ciclo.
 */
#include "Hablar.h"
#include "Alegria.h"

// Estado global (definido aqui, declarado extern en Hablar.h)
bool modoHablarVieja = false;

// ---------------------------------------------------------------------------
// Posiciones (mismas que Alegria.cpp)
// ---------------------------------------------------------------------------
static const uint8_t OJO_IZQ[2] = {1, 1};
static const uint8_t OJO_DER[2] = {3, 1};

// Esquinas de la sonrisa + curva inferior = las MEJILLAS (se quedan fijas)
static const uint8_t MEJILLA[5][2] = {
    {0,3}, {4,3}, {1,4}, {2,4}, {3,4}
};

// La linea de DIENTES: solo parpadea esto al hablar
static const uint8_t DIENTES[3][2] = {
    {1,3}, {2,3}, {3,3}
};

// ---------------------------------------------------------------------------
// Helpers (usan fiber_sleep para ceder CPU a la otra fibra)
// ---------------------------------------------------------------------------

static void setPixel(const uint8_t* p, int v)
{
    uBit.display.image.setPixelValue(p[0], p[1], v);
}

// Parpadeo rapido y sincronizado: ambos ojos se mueven JUNTOS paso a paso
// (si se hicieran secuenciales se veria como parpadeo desincronizado).
// Checa modoHablarVieja en cada paso: si llega otra emocion, la fibra
// MUERE en el acto (no sigue parpadeando sobre la animacion siguiente).
static void cerrarOjos(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablarVieja; s++) {
        int b = 255 - (255 * s) / steps;
        if (izq) setPixel(OJO_IZQ, b);
        if (der) setPixel(OJO_DER, b);
        fiber_sleep(delayMs);
    }
}

static void abrirOjos(bool izq, bool der, int steps, int delayMs)
{
    for (int s = 0; s <= steps && modoHablarVieja; s++) {
        int b = (255 * s) / steps;
        if (izq) setPixel(OJO_IZQ, b);
        if (der) setPixel(OJO_DER, b);
        fiber_sleep(delayMs);
    }
}

// Enciende o apaga la linea de dientes con fade (no salta: se ve natural)
static void setDientes(int v, int steps, int delayMs)
{
    for (int s = 0; s <= steps; s++) {
        int b = v * s / steps;
        for (int i = 0; i < 3; i++)
            setPixel(DIENTES[i], b);
        fiber_sleep(delayMs);
    }
}

// Un "tic" de habla: los dientes aparecen y desaparecen (ritmo de boca)
static void ticDientes()
{
    setDientes(255, 2, 20);   // aparecen (40ms, fade)
    fiber_sleep(80);          // visibles un momento
    setDientes(0, 2, 20);     // desaparecen (40ms, fade)
    fiber_sleep(80);          // pausa entre tics
}

// ---------------------------------------------------------------------------
// FIBRA: parpadeo IMPREDECIBLE de los ojos mientras modoHablarVieja siga activo
// ---------------------------------------------------------------------------

static void fiberParpadeo(void)
{
    while (modoHablarVieja) {
        // 1) Decidir QUE parpadea: ~75% ambos, ~12.5% guino izq, ~12.5% guino der
        int r = uBit.random(100);
        bool izq = true, der = true;
        if (r >= 88)      { izq = false; }   // guino derecho
        else if (r >= 75) { der = false; }   // guino izquierdo
        // else: ambos a la vez (lo mas humano)

        // 2) Parpadeo rapido: cerrar ~120ms, cerrado ~100ms, abrir ~120ms
        cerrarOjos(izq, der, 4, 30);
        fiber_sleep(100);
        abrirOjos(izq, der, 4, 30);

        // 3) Espera ALEATORIA al siguiente parpadeo: 1.5s a 5s
        int espera = 1500 + uBit.random(3500);
        for (int i = 0; i < espera / 100 && modoHablarVieja; i++)
            fiber_sleep(100);
    }

    // Sale del loop (modoHablarVieja ya es false) -> la fibra se libera sola
    release_fiber();
}

// ---------------------------------------------------------------------------
// Dibuja la cara base para hablar: sonrisa COMPLETA fija + ojos
// ---------------------------------------------------------------------------
static void dibujarCara()
{
    uBit.display.setBrightness(90);
    uBit.display.image.clear();
    setPixel(OJO_IZQ, 255);
    setPixel(OJO_DER, 255);
    // Mejillas SIEMPRE encendidas (nunca se apagan mientras habla)
    for (int i = 0; i < 5; i++)
        setPixel(MEJILLA[i], 255);
    // Dientes empiezan apagados
}

// ---------------------------------------------------------------------------
// API publica
// ---------------------------------------------------------------------------

// TALK: prepara la cara y lanza la fibra de parpadeo impredecible
void iniciarHablarVieja()
{
    // Si ya estamos hablando, NO crear otra fibra (si no, dos fibras
    // competirian parpadeando los mismos ojos para siempre)
    if (modoHablarVieja) return;

    modoHablarVieja = true;
    dibujarCara();
    create_fiber(fiberParpadeo);   // los ojos corren EN PARALELO
}

// Otra emocion: apaga la bandera; la fibra muere en su siguiente ciclo
void detenerHablarVieja()
{
    modoHablarVieja = false;
}

// Una pasada de "hablar": SOLO la linea de dientes (los ojos los mueve la fibra)
void animarBocaHablandoVieja()
{
    ticDientes();
    ticDientes();
    ticDientes();   // ~3 tics por pasada -> ritmo de conversacion
}
