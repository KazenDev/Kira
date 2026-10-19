/**
 * test_vida.cpp - TESTS del motor de vida (en el PC, sin la placa)
 *
 * El nucleo (Animaciones/Vida/Nucleo/) es C++ puro: se compila tal cual con
 * g++ y se corre con el reloj y los sentidos simulados, con semilla fija.
 *
 *   sh run_vida.sh
 *
 * Que se verifica:
 *   1. Las 8 personalidades viven 10 min sin valores fuera de rango, sin NaN
 *      y sin quedarse a oscuras (salvo parpadeos cortos).
 *   2. Parpadeo: la tasa cae en el rango humano y los intervalos NO son
 *      regulares (coeficiente de variacion alto).
 *   3. No se repite: ninguna ventana de 6 s es identica a otra.
 *   4. Todos los gestos de cada personalidad aparecen, y nunca dos iguales
 *      seguidos.
 *   5. Hablar: la boca se mueve a ritmo de silabas y se cierra al callar.
 *   6. Sentidos y animo: toque, carino, picar -> berrinche -> ofendida ->
 *      perdon, enamorada, mareo, caida, oscuridad -> dormir -> deslumbrada,
 *      aburrimiento -> dormir -> despertar, clima calor/frio.
 *   7. Minijuegos: los tres terminan solos y dan un resultado valido.
 *   8. Sonidos: todo el catalogo se codifica en el formato de CODAL.
 */
#include "Animaciones/Vida/Nucleo/Cara.h"
#include "Animaciones/Vida/Nucleo/Juegos.h"
#include "Animaciones/Vida/Nucleo/Catalogo.h"
#include "Animaciones/Vida/Nucleo/SonidoId.h"
#include "Animaciones/Emociones/Emociones.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

static int fallas = 0;
static int pruebas = 0;

#define CHEQUEAR(cond, ...) do { pruebas++; if (!(cond)) { fallas++; \
    printf("  FALLA: "); printf(__VA_ARGS__); printf("\n"); } } while (0)

static const char *NOMBRE[NUM_EMOCIONES] = {
    "Alegria", "Triste", "Enojado", "Sorprendido",
    "Neutral", "Fastidio", "Miedo", "Cansado"
};

static Estimulos quietos()
{
    Estimulos e;
    memset(&e, 0, sizeof(e));
    e.hayAcel = true;
    e.az = -1000.0f;      // acostada, quieta
    return e;
}

static uint32_t hashLienzo(const Lienzo &l)
{
    uint32_t h = 2166136261u;
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++) {
            h ^= lienzoAByte(l.v[y][x]);
            h *= 16777619u;
        }
    return h;
}

static float luzTotal(const Lienzo &l)
{
    float s = 0;
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++) s += l.v[y][x];
    return s;
}

// Corre 'ms' de vida con estimulos fijos; devuelve la ultima reaccion vista
static void vivir(EstadoCara &c, uint32_t &t, uint32_t ms, const Estimulos &e, Lienzo &l)
{
    for (uint32_t fin = t + ms; t < fin; t += 16) caraFrame(c, t, e, l);
}

// --- 1-4: cada personalidad en reposo -----------------------------------------
static void testPersonalidades()
{
    printf("[1-4] Personalidades en reposo (10 min cada una)\n");
    for (int em = 0; em < NUM_EMOCIONES; em++) {
        const Personalidad *p = personalidadDe(em);
        EstadoCara c;
        Lienzo l;
        caraIniciar(c, p, 1000, 12345u + em, true);
        Estimulos e = quietos();

        bool fuera = false, nan = false;
        uint32_t oscuroDesde = 0, peorOscuro = 0;
        int parpadeos = 0;
        bool cerradoAntes = false;
        uint32_t ultimoParpadeo = 0;
        double sumaIbi = 0, sumaIbi2 = 0;
        int nIbi = 0;
        int usosGesto[MAX_GESTOS] = {0};
        int repetidos = 0, ultimoGesto = -1;

        const uint32_t DURA = 600000;
        const int VENT = 6000 / 16;        // frames por ventana de 6 s
        static uint32_t ventanas[200];
        int nVent = 0;
        uint32_t hv = 0;
        int enVent = 0;

        // Mantener la cara "atendida" (si no, a los 150 s se duerme)
        for (uint32_t t = 1000; t < 1000 + DURA; t += 16) {
            if ((t - 1000) % 50000 == 0) caraActividad(c, t);
            caraFrame(c, t, e, l);

            for (int y = 0; y < 5; y++)
                for (int x = 0; x < 5; x++) {
                    float v = l.v[y][x];
                    if (v != v) nan = true;
                    if (v < -0.001f || v > 1.3f) fuera = true;
                }
            if (luzTotal(l) < 0.05f) {
                if (!oscuroDesde) oscuroDesde = t;
                if (t - oscuroDesde > peorOscuro) peorOscuro = t - oscuroDesde;
            } else {
                oscuroDesde = 0;
            }

            bool cerrado = c.ojos.cierreParpadeo > 0.9f;
            if (cerrado && !cerradoAntes) {
                parpadeos++;
                if (ultimoParpadeo) {
                    double ibi = (double)(t - ultimoParpadeo);
                    sumaIbi += ibi; sumaIbi2 += ibi * ibi; nIbi++;
                }
                ultimoParpadeo = t;
            }
            cerradoAntes = cerrado;

            if ((c.eventos & EV_GESTO) && c.eventoGesto >= 0) {
                usosGesto[c.eventoGesto]++;
                if (c.eventoGesto == ultimoGesto && !(c.eventos & EV_ENTRADA)) repetidos++;
                ultimoGesto = c.eventoGesto;
            }

            hv = hv * 31u + hashLienzo(l);
            if (++enVent == VENT) {
                if (nVent < 200) ventanas[nVent++] = hv;
                hv = 0;
                enVent = 0;
            }
        }

        double media = nIbi ? sumaIbi / nIbi : 0;
        double sd = nIbi ? sqrt(sumaIbi2 / nIbi - media * media) : 0;
        double porMin = parpadeos / (DURA / 60000.0);
        int iguales = 0;
        for (int i = 0; i < nVent; i++)
            for (int k = i + 1; k < nVent; k++)
                if (ventanas[i] == ventanas[k]) iguales++;
        int gestosVistos = 0;
        for (int g = 0; g < p->nGestos; g++) if (usosGesto[g]) gestosVistos++;

        printf("  %-12s parpadeos %5.1f/min  CV %.2f  oscuro max %4u ms  gestos %d/%d  ventanas iguales %d\n",
               NOMBRE[em], porMin, media > 0 ? sd / media : 0.0, peorOscuro,
               gestosVistos, p->nGestos, iguales);

        CHEQUEAR(!nan, "%s: hay NaN en el lienzo", NOMBRE[em]);
        CHEQUEAR(!fuera, "%s: valores fuera de rango", NOMBRE[em]);
        CHEQUEAR(peorOscuro < 700, "%s: la cara quedo a oscuras %u ms", NOMBRE[em], peorOscuro);
        CHEQUEAR(porMin > 4.0 && porMin < 30.0, "%s: %.1f parpadeos/min fuera del rango humano", NOMBRE[em], porMin);
        CHEQUEAR(media > 0 && sd / media > 0.4, "%s: parpadeo demasiado regular (CV %.2f)", NOMBRE[em], sd / media);
        CHEQUEAR(iguales == 0, "%s: %d ventanas de 6 s identicas (se repite)", NOMBRE[em], iguales);
        CHEQUEAR(gestosVistos == p->nGestos, "%s: solo aparecieron %d de %d gestos", NOMBRE[em], gestosVistos, p->nGestos);
        CHEQUEAR(repetidos == 0, "%s: %d gestos repetidos seguidos", NOMBRE[em], repetidos);
    }
}

// --- 5: hablar ----------------------------------------------------------------
static void testHabla()
{
    printf("[5] Hablar\n");
    for (int em = 0; em < NUM_EMOCIONES; em++) {
        EstadoCara c;
        Lienzo l;
        Estimulos e = quietos();
        caraIniciar(c, personalidadDe(em), 1000, 777u + em, false);
        uint32_t t = 1000;
        caraHablar(c, true, t);
        int picos = 0, pausas = 0;
        bool subiendo = false;
        float antes = 0;
        for (uint32_t fin = t + 30000; t < fin; t += 16) {
            caraFrame(c, t, e, l);
            float a = c.habla.apertura;
            if (a > antes + 0.01f) subiendo = true;
            if (subiendo && a < antes - 0.01f) { picos++; subiendo = false; }
            if (c.eventos & EV_FIN_FRASE) pausas++;
            antes = a;
        }
        float silabasSeg = picos / 30.0f;
        caraHablar(c, false, t);
        vivir(c, t, 250, e, l);
        printf("  %-12s %.1f silabas/s, %d pausas de frase en 30 s, boca al callar %.2f\n",
               NOMBRE[em], silabasSeg, pausas, c.habla.apertura);
        CHEQUEAR(silabasSeg > 2.0f && silabasSeg < 8.5f, "%s: %.1f silabas/s", NOMBRE[em], silabasSeg);
        CHEQUEAR(pausas >= 3, "%s: casi no hace pausas (%d)", NOMBRE[em], pausas);
        CHEQUEAR(c.habla.apertura < 0.01f, "%s: la boca no se cerro al callar", NOMBRE[em]);
    }
}

// --- 6: sentidos y animo -------------------------------------------------------
static uint8_t ultimaReaccion(const EstadoCara &c)
{
    return (c.eventos & EV_REACCION) ? c.eventoReaccion : 0;
}

// Un frame con el estimulo y devuelve la reaccion que dispara (en el
// frame siguiente, porque los eventos se publican un frame despues)
static uint8_t estimular(EstadoCara &c, uint32_t &t, Estimulos e, Lienzo &l)
{
    caraFrame(c, t, e, l);
    t += 16;
    Estimulos q = quietos();
    q.toque = e.toque;          // si se mantiene el dedo, se mantiene
    caraFrame(c, t, q, l);
    uint8_t r = ultimaReaccion(c);
    t += 16;
    return r;
}

static void soltar(EstadoCara &c, uint32_t &t, Lienzo &l, uint32_t ms)
{
    vivir(c, t, ms, quietos(), l);
}

static void testSentidos()
{
    printf("[6] Sentidos y animo\n");
    EstadoCara c;
    Lienzo l;
    uint32_t t = 1000;
    caraIniciar(c, personalidadDe(0), t, 99u, false);
    soltar(c, t, l, 500);

    Estimulos toque = quietos(); toque.toque = true;
    uint8_t r = estimular(c, t, toque, l);
    CHEQUEAR(r == R_COSQUILLAS, "tocar a la alegria deberia dar cosquillas (dio %d)", r);
    soltar(c, t, l, 300);

    // Mantener el dedo = carino
    for (int i = 0; i < 100; i++) { caraFrame(c, t, toque, l); t += 16; }
    CHEQUEAR(c.reaccion == R_CARINO, "mantener el dedo deberia dar carino (esta en %d)", c.reaccion);
    CHEQUEAR(c.nFlores == 1, "una caricia deberia dejar una flor (hay %d)", c.nFlores);
    soltar(c, t, l, 4000);

    // Picar con B: toque, toque, molesta (+llama), y a la tercera llama: berrinche
    Estimulos picar = quietos(); picar.picar = true;
    uint8_t r1 = estimular(c, t, picar, l); soltar(c, t, l, 800);
    uint8_t r2 = estimular(c, t, picar, l); soltar(c, t, l, 800);
    uint8_t r3 = estimular(c, t, picar, l); soltar(c, t, l, 800);
    CHEQUEAR(r1 == R_TOQUE && r2 == R_TOQUE, "los dos primeros piques deberian ser TOQUE (%d,%d)", r1, r2);
    CHEQUEAR(r3 == R_MOLESTA, "el tercer pique deberia molestarla (%d)", r3);
    CHEQUEAR(c.nFlores == 0 && c.nLlamas == 0, "la primera molestia se lleva la flor (flores %d llamas %d)", c.nFlores, c.nLlamas);
    uint8_t r4 = 0, r5 = 0;
    r4 = estimular(c, t, picar, l); soltar(c, t, l, 800);
    r5 = estimular(c, t, picar, l); soltar(c, t, l, 800);
    uint8_t r6 = estimular(c, t, picar, l);
    CHEQUEAR(r4 == R_MOLESTA && r5 == R_MOLESTA, "seguir picando molesta (%d,%d)", r4, r5);
    CHEQUEAR(r6 == R_BERRINCHE, "la tercera llama deberia dar berrinche (%d)", r6);
    CHEQUEAR(caraOfendida(c), "despues del berrinche queda ofendida");
    soltar(c, t, l, 3000);

    // Ofendida: tocarla da "hmph"; acariciarla la perdona
    r = estimular(c, t, toque, l);
    CHEQUEAR(r == R_OFENDIDA, "tocarla ofendida deberia dar OFENDIDA (%d)", r);
    bool perdono = false;
    for (int i = 0; i < 120; i++) {
        caraFrame(c, t, toque, l); t += 16;
        if (ultimaReaccion(c) == R_PERDON) perdono = true;
    }
    CHEQUEAR(perdono && !caraOfendida(c), "una caricia larga deberia perdonarla");
    soltar(c, t, l, 4000);

    // Tres caricias = enamorada
    bool enamorada = false;
    for (int k = 0; k < 3; k++) {
        for (int i = 0; i < 100; i++) {
            caraFrame(c, t, toque, l); t += 16;
            if (ultimaReaccion(c) == R_ENAMORADA) enamorada = true;
        }
        soltar(c, t, l, 3200);
        if (ultimaReaccion(c) == R_ENAMORADA) enamorada = true;
    }
    CHEQUEAR(enamorada, "tres caricias deberian enamorarla");
    soltar(c, t, l, 4000);

    // Las flores y llamas se van solas a los 2 min
    Estimulos picar1 = quietos(); picar1.picar = true;
    c.picadas = 5;               // que el proximo pique sea "molesta"
    c.ultimaPicada = t;
    c.nFlores = 0;               // (una molestia se llevaria una flor primero)
    estimular(c, t, picar1, l);
    CHEQUEAR(c.nLlamas == 1, "un pique de mas deja una llama (%d)", c.nLlamas);
    for (int i = 0; i < 125; i++) { if (i % 40 == 0) caraActividad(c, t); soltar(c, t, l, 1000); }
    CHEQUEAR(c.nLlamas == 0, "la llama deberia irse sola a los 2 min (%d)", c.nLlamas);

    // Sacudida sostenida = mareo
    Estimulos sac = quietos(); sac.sacudida = true;
    bool mareo = false;
    for (int i = 0; i < 40; i++) { caraFrame(c, t, sac, l); t += 16; if (ultimaReaccion(c) == R_MAREO) mareo = true; }
    soltar(c, t, l, 50);
    if (ultimaReaccion(c) == R_MAREO) mareo = true;
    CHEQUEAR(mareo, "sacudirla deberia marearla");
    soltar(c, t, l, 3500);
    CHEQUEAR(c.reaccion == R_NINGUNA, "el mareo deberia pasarse solo (%d)", c.reaccion);

    // Caida libre
    Estimulos cae = quietos(); cae.caida = true;
    r = estimular(c, t, cae, l);
    CHEQUEAR(r == R_CAIDA, "la caida libre deberia dar CAIDA (%d)", r);
    soltar(c, t, l, 2000);

    // Oscuridad: se duerme; vuelve la luz: deslumbrada
    Estimulos luz = quietos(); luz.hayLuz = true; luz.luz = 80;
    for (int i = 0; i < 60; i++) { caraFrame(c, t, luz, l); t += 16; }
    Estimulos osc = quietos(); osc.hayLuz = true; osc.luz = 2;
    for (int i = 0; i < 250; i++) { caraFrame(c, t, osc, l); t += 16; }
    CHEQUEAR(c.reaccion == R_DORMIR, "tapada deberia dormirse (%d)", c.reaccion);
    bool deslumbrada = false;
    for (int i = 0; i < 20; i++) { caraFrame(c, t, luz, l); t += 16; if (ultimaReaccion(c) == R_DESLUMBRADA) deslumbrada = true; }
    CHEQUEAR(deslumbrada, "al volver la luz deberia despertarse deslumbrada");
    soltar(c, t, l, 2000);

    // Lugar oscuro PARA EL SENSOR (marca 0 siempre, como en una pieza de
    // noche): no se tiene que dormir por eso
    {
        EstadoCara d;
        Lienzo ld;
        uint32_t td = 1000;
        caraIniciar(d, personalidadDe(0), td, 5u, false);
        Estimulos cero = quietos(); cero.hayLuz = true; cero.luz = 0;
        for (int i = 0; i < 600; i++) { caraFrame(d, td, cero, ld); td += 16; }
        CHEQUEAR(d.reaccion != R_DORMIR, "con el sensor en 0 (pieza oscura) NO se debe dormir");
    }

    // Aburrimiento: a los 150 s sola se duerme; tocarla la despierta
    vivir(c, t, 155000, quietos(), l);
    CHEQUEAR(c.reaccion == R_DORMIR, "sola 150 s deberia dormirse (%d)", c.reaccion);
    r = estimular(c, t, toque, l);
    CHEQUEAR(r == R_DESPERTAR, "tocarla dormida deberia despertarla (%d)", r);
    soltar(c, t, l, 2000);

    // Clima: el termometro manda con histeresis y 8 s de confirmacion
    Estimulos calor = quietos(); calor.hayTemp = true; calor.temp = 35;
    bool acalorada = false;
    for (int i = 0; i < 700; i++) { caraFrame(c, t, calor, l); t += 16; if (ultimaReaccion(c) == R_ACALORADA) acalorada = true; }
    CHEQUEAR(c.clima == CLIMA_CALOR && acalorada, "a 35 grados deberia acalorarse (clima %d)", c.clima);
    Estimulos tibio = quietos(); tibio.hayTemp = true; tibio.temp = 32;
    for (int i = 0; i < 700; i++) { caraFrame(c, t, tibio, l); t += 16; }
    CHEQUEAR(c.clima == CLIMA_CALOR, "a 32 grados sigue con calor (histeresis)");
    Estimulos normal = quietos(); normal.hayTemp = true; normal.temp = 25;
    for (int i = 0; i < 700; i++) { caraFrame(c, t, normal, l); t += 16; }
    CHEQUEAR(c.clima == CLIMA_NORMAL, "a 25 grados vuelve a normal (%d)", c.clima);
    caraForzarClima(c, CLIMA_FRIO, t);
    CHEQUEAR(c.clima == CLIMA_FRIO, "CLIMA:FRIO deberia aplicarse al instante");
    soltar(c, t, l, 2500);
    r = estimular(c, t, toque, l);
    CHEQUEAR(r == R_CARINO, "tocarla con frio la abriga (CARINO, dio %d)", r);
    bool estornudo = false;
    for (int i = 0; i < 40; i++) {
        caraActividad(c, t);
        for (int k = 0; k < 500; k++) { caraFrame(c, t, quietos(), l); t += 16; if (ultimaReaccion(c) == R_ESTORNUDO) estornudo = true; }
    }
    CHEQUEAR(estornudo, "con frio deberia estornudar alguna vez en 5 min");
    caraForzarClima(c, -1, t);

}

// --- 7: minijuegos -------------------------------------------------------------
static void testJuegos()
{
    printf("[7] Minijuegos\n");
    static const char *N[4] = { "", "Dado", "PPT", "Reflejos" };
    for (int jg = J_DADO; jg < NUM_JUEGOS; jg++) {
        for (int ronda = 0; ronda < 20; ronda++) {
            EstadoJuego j;
            Lienzo l;
            uint32_t t = 5000;
            juegoIniciar(j, (uint8_t)jg, t, 1000u + ronda * 17u + jg);
            bool sigue = true, aviso = false, jingle = false;
            char textoAviso[40] = "";
            uint32_t frames = 0;
            while (sigue && frames < 3000) {
                EntradaJuego e = { false, false, false, false };
                // El "jugador": en PPT aprieta B a un tiempo distinto cada
                // ronda; en reflejos aprieta 280 ms despues del flash
                if (jg == J_PPT && j.fase == 2 && t - j.faseDesde == 400u + ronda * 128u) e.pulsoB = true;
                if (jg == J_REFLEJOS && j.fase == 7 && t - j.faseDesde >= 280) e.pulsoA = true;
                sigue = juegoFrame(j, t, e, l);
                if (j.sonido == S_JUEGO) jingle = true;
                if (j.hayAviso) { aviso = true; snprintf(textoAviso, sizeof(textoAviso), "%s", j.aviso); }
                t += 16;
                frames++;
            }
            CHEQUEAR(!sigue, "%s ronda %d: no termino solo", N[jg], ronda);
            CHEQUEAR(jingle, "%s: no sono el jingle de inicio", N[jg]);
            CHEQUEAR(aviso, "%s ronda %d: no aviso el resultado por serial", N[jg], ronda);
            if (ronda == 0) printf("  %-9s termina en %4.1f s  -> %s\n", N[jg], frames * 0.016f, textoAviso);
            if (jg == J_DADO) CHEQUEAR(j.valor >= 1 && j.valor <= 6, "dado fuera de 1..6 (%d)", j.valor);
            if (jg == J_PPT) CHEQUEAR(j.jugador >= 0 && j.jugador < 3 && j.valor >= 0 && j.valor < 3,
                                      "ppt: elecciones invalidas (%d,%d)", j.jugador, j.valor);
        }
    }
    // Reflejos: apretar antes del flash es trampa
    EstadoJuego j;
    Lienzo l;
    juegoIniciar(j, J_REFLEJOS, 1000, 5u);
    uint32_t t = 1000;
    bool trampa = false;
    for (int i = 0; i < 300; i++) {
        EntradaJuego e = { j.fase == 6 && t - j.faseDesde > 200, false, false, false };
        juegoFrame(j, t, e, l);
        if (j.hayAviso && strstr(j.aviso, "TRAMPA")) trampa = true;
        t += 16;
    }
    CHEQUEAR(trampa, "reflejos: apretar antes del flash deberia ser trampa");
}

// --- 8: catalogo de sonidos ------------------------------------------------------
static int digitos(const char *s, int n)
{
    int v = 0;
    for (int i = 0; i < n; i++) {
        if (s[i] < '0' || s[i] > '9') return -1;
        v = v * 10 + (s[i] - '0');
    }
    return v;
}

static void testSonidos()
{
    printf("[8] Catalogo de sonidos\n");
    char buf[MAX_CODIGO];
    int total = 0;
    for (int id = 1; id < NUM_SONIDOS; id++) {
        int nv = catalogoVariantes((uint8_t)id);
        CHEQUEAR(nv > 0, "el sonido %d no tiene variantes", id);
        for (int v = 0; v < nv; v++) {
            for (int extremo = 0; extremo < 2; extremo++) {
                const Nota *n = 0;
                int k = catalogoNotas((uint8_t)id, v, &n);
                float a = extremo ? 0.999f : 0.0f;
                int L = codificarNotas(n, k, 0.92f + 0.16f * a, 0.9f + 0.2f * a, buf, sizeof(buf));
                total++;
                // El mismo chequeo que SoundExpressions::playAsync
                unsigned len = (unsigned)strlen(buf);
                unsigned cnt = (len + 1) / 73;
                bool ok = L > 0 && len == cnt * 73 - 1;
                for (unsigned i = 0; ok && i < cnt; i++) {
                    const char *e = buf + i * 73;
                    if (i > 0 && e[-1] != ',') ok = false;
                    int onda = digitos(e, 1), vol = digitos(e + 1, 4), f = digitos(e + 5, 4);
                    int d = digitos(e + 9, 4), forma = digitos(e + 13, 2), ff = digitos(e + 18, 4);
                    int vf = digitos(e + 26, 4), pasos = digitos(e + 30, 4);
                    if (onda < 0 || onda > 4 || vol < 0 || vol > 1023 || f < 20 || d < 1 ||
                        ff < 20 || vf < 0 || vf > 1023 || pasos < 1 ||
                        (forma != 0 && forma != 1 && forma != 2))
                        ok = false;
                }
                CHEQUEAR(ok, "el sonido %d variante %d no se codifica bien", id, v);
            }
        }
    }
    printf("  %d codificaciones revisadas\n", total);
}

int main()
{
    testPersonalidades();
    testHabla();
    testSentidos();
    testJuegos();
    testSonidos();
    printf("\n%d chequeos, %d fallas\n", pruebas, fallas);
    return fallas ? 1 : 0;
}
