/**
 * Juegos.cpp - Los minijuegos (ver Juegos.h)
 */
#include "Juegos.h"
#include "Ruido.h"
#include "SonidoId.h"
#include <stdio.h>

// --- Dibujos -------------------------------------------------------------------

// Las caras del dado en la matriz 5x5 (crisp: pixeles enteros)
static void dibujarDado(Lienzo &l, int n, float v)
{
    static const uint8_t P[6][6][2] = {
        {{2,2}},
        {{0,0},{4,4}},
        {{0,0},{2,2},{4,4}},
        {{0,0},{4,0},{0,4},{4,4}},
        {{0,0},{4,0},{2,2},{0,4},{4,4}},
        {{0,0},{0,2},{0,4},{4,0},{4,2},{4,4}},
    };
    if (n < 1 || n > 6) return;
    for (int i = 0; i < n; i++) lienzoPixel(l, P[n - 1][i][0], P[n - 1][i][1], v);
}

// Piedra (una bola), papel (una hoja) y tijera (una X)
static void dibujarPPT(Lienzo &l, int k, float v, float dy)
{
    if (k == 0) {
        for (int y = 1; y <= 3; y++)
            for (int x = 1; x <= 3; x++) lienzoPunto(l, (float)x, y + dy, v);
        lienzoPunto(l, 2, 0 + dy, 0.4f * v);
        lienzoPunto(l, 0, 2 + dy, 0.4f * v);
        lienzoPunto(l, 4, 2 + dy, 0.4f * v);
        lienzoPunto(l, 2, 4 + dy, 0.4f * v);
    } else if (k == 1) {
        for (int i = 0; i < 5; i++) {
            lienzoPunto(l, (float)i, 0 + dy, v);
            lienzoPunto(l, (float)i, 4 + dy, v);
        }
        for (int y = 1; y < 4; y++) {
            lienzoPunto(l, 0, y + dy, v);
            lienzoPunto(l, 4, y + dy, v);
        }
    } else {
        for (int i = 0; i < 5; i++) {
            lienzoPunto(l, (float)i, i + dy, v);
            lienzoPunto(l, (float)(4 - i), i + dy, v);
        }
    }
}

// Caras de resultado (filas 0-3), compartidas por los juegos
static void caraGana(Lienzo &l, float te)       // Kira gana: presume
{
    float dy = -0.3f * fabsf(sinf(6.2831853f * 2.5f * te));
    lienzoPunto(l, 1, 0 + dy, 1.0f); lienzoPunto(l, 3, 0 + dy, 1.0f);
    lienzoPunto(l, 0, 1 + dy, 0.8f); lienzoPunto(l, 2, 1 + dy, 0.8f); lienzoPunto(l, 4, 1 + dy, 0.8f);
    lienzoPunto(l, 0, 2.5f + dy, 0.8f); lienzoPunto(l, 4, 2.5f + dy, 0.8f);
    for (int x = 1; x < 4; x++) lienzoPunto(l, (float)x, 3.5f + dy, 0.85f);
}

static void caraPierde(Lienzo &l, float te)     // Kira pierde: se enfurruna
{
    float dy = 0.25f * suave(te / 0.6f);
    lienzoPunto(l, 1, 1 + dy, 0.8f); lienzoPunto(l, 3, 1 + dy, 0.8f);
    for (int x = 1; x < 4; x++) lienzoPunto(l, (float)x, 3 + dy, 0.7f);
    lienzoPunto(l, 0, 4, 0.6f); lienzoPunto(l, 4, 4, 0.6f);
    // Una lagrimita
    float f = fmodf(te, 1.4f) / 1.4f;
    lienzoPunto(l, 1, 1.8f + 2.2f * acelera(f), 0.45f * campana(f, 0.2f));
}

static void caraSorpresa(Lienzo &l, float te)   // empate / muy rapido
{
    float dy = -0.2f * campana(te / 0.5f, 0.3f);
    lienzoPunto(l, 1, 0 + dy, 1.0f); lienzoPunto(l, 1, 1 + dy, 1.0f);
    lienzoPunto(l, 3, 0 + dy, 1.0f); lienzoPunto(l, 3, 1 + dy, 1.0f);
    lienzoPunto(l, 2, 2.5f + dy, 0.4f); lienzoPunto(l, 1, 3.2f + dy, 0.4f);
    lienzoPunto(l, 3, 3.2f + dy, 0.4f); lienzoPunto(l, 2, 3.2f + dy, 0.9f);
}

static void caraContenta(Lienzo &l)
{
    lienzoPixel(l, 1, 1, 1.0f); lienzoPixel(l, 3, 1, 1.0f);
    lienzoPixel(l, 0, 2, 0.75f); lienzoPixel(l, 4, 2, 0.75f);
    for (int x = 1; x < 4; x++) lienzoPixel(l, x, 3, 0.85f);
}

static void caraMeh(Lienzo &l)
{
    lienzoPixel(l, 0, 1, 0.5f); lienzoPixel(l, 1, 1, 1.0f);
    lienzoPixel(l, 3, 1, 1.0f); lienzoPixel(l, 4, 1, 0.5f);
    for (int x = 1; x < 4; x++) lienzoPixel(l, x, 3, 0.75f);
}

static void caraDormida(Lienzo &l, float te)
{
    lienzoPixel(l, 0, 1, 0.2f); lienzoPixel(l, 1, 1, 0.2f);
    lienzoPixel(l, 3, 1, 0.2f); lienzoPixel(l, 4, 1, 0.2f);
    lienzoPixel(l, 2, 3, 0.35f);
    float f = fmodf(te, 1.2f) / 1.2f;
    lienzoPunto(l, 3.2f + f, 2.6f - 2.6f * f, 0.5f * campana(f, 0.3f));
}

static void caraTramposo(Lienzo &l, float te)
{
    float dx = 0.35f * sinf(6.2831853f * 4.0f * te) * (1.0f - suave(te / 1.2f));
    lienzoPunto(l, 0 + dx, 0, 0.6f); lienzoPunto(l, 1 + dx, 0, 0.6f);
    lienzoPunto(l, 3 + dx, 0, 0.6f); lienzoPunto(l, 4 + dx, 0, 0.6f);
    lienzoPunto(l, 0 + dx, 1, 1.0f); lienzoPunto(l, 3 + dx, 1, 1.0f);
    lienzoPunto(l, 1 + dx, 3, 0.7f); lienzoPunto(l, 2 + dx, 3, 0.7f);
    lienzoPunto(l, 3.4f + dx, 2.6f, 0.7f);
}

// --- Maquina de estados ----------------------------------------------------------

// Fases comunes
enum { F_INTRO = 0, F_JUGANDO, F_ELIGE, F_CUENTA, F_REVELA, F_RESULTADO,
       F_ESPERA, F_YA, F_TRAMPA, F_FIN };

static void fase(EstadoJuego &j, uint8_t f, uint32_t t)
{
    j.fase = f;
    j.faseDesde = t;
}

static void avisar(EstadoJuego &j, const char *texto)
{
    snprintf(j.aviso, sizeof(j.aviso), "%s", texto);
    j.hayAviso = true;
}

bool juegoActivo(const EstadoJuego &j)
{
    return j.juego != J_NINGUNO && j.fase != F_FIN;
}

void juegoTerminar(EstadoJuego &j)
{
    j.fase = F_FIN;
    j.juego = J_NINGUNO;
}

void juegoIniciar(EstadoJuego &j, uint8_t juego, uint32_t t, uint32_t semilla)
{
    j.juego = juego;
    azarSembrar(j.azar, semilla);
    j.valor = 1 + azarEntero(j.azar, 6);
    j.jugador = -1;
    j.resultado = 0;
    j.espera = 0;
    j.reaccionMs = 0;
    j.proxCambio = t;
    j.paso = 50;
    j.sonido = 0;
    j.sonidoInicio = S_JUEGO;
    j.hayAviso = false;
    j.aviso[0] = '\0';
    fase(j, F_INTRO, t);
}

// --- DADO --------------------------------------------------------------------------

static void tirarDado(EstadoJuego &j, uint32_t t)
{
    j.paso = 45;
    j.proxCambio = t;
    fase(j, F_JUGANDO, t);
}

static bool frameDado(EstadoJuego &j, uint32_t t, const EntradaJuego &e, Lienzo &l)
{
    uint32_t en = t - j.faseDesde;
    switch (j.fase) {
        case F_INTRO: {
            // El "dado" (un cuadradito) que late antes de tirar
            float v = 0.5f + 0.5f * sinf(6.2831853f * 3.0f * en * 0.001f);
            for (int i = 1; i < 4; i++) {
                lienzoPixel(l, i, 1, v); lienzoPixel(l, i, 3, v);
                lienzoPixel(l, 1, i, v); lienzoPixel(l, 3, i, v);
            }
            if (en > 700) tirarDado(j, t);
            return true;
        }
        case F_JUGANDO: {
            // Rueda rapido y va frenando (cada cambio tarda 18% mas). Si la
            // siguen sacudiendo, no frena.
            if ((int32_t)(t - j.proxCambio) >= 0) {
                int nuevo = 1 + azarEntero(j.azar, 5);
                if (nuevo >= j.valor) nuevo++;          // nunca la misma cara
                j.valor = nuevo;
                if (!e.sacudida) j.paso = (uint32_t)(j.paso * 1.18f) + 4;
                j.proxCambio = t + j.paso;
                j.sonido = S_TIC;
            }
            dibujarDado(l, j.valor, 0.9f);
            if (j.paso > 330) {
                fase(j, F_RESULTADO, t);
                j.sonido = j.valor == 6 ? S_GANA : (j.valor == 1 ? S_PIERDE : S_DING);
                char a[40];
                snprintf(a, sizeof(a), "JUEGO:DADO:%d", j.valor);
                avisar(j, a);
            }
            return true;
        }
        case F_RESULTADO: {
            // El resultado parpadea dos veces y queda
            float v = (en < 400 && (en / 100) % 2 == 1) ? 0.25f : 1.0f;
            dibujarDado(l, j.valor, v);
            if (e.pulsoB || e.sacudida) { tirarDado(j, t); return true; }
            if (en > 3500) { juegoTerminar(j); return false; }
            return true;
        }
        default:
            juegoTerminar(j);
            return false;
    }
}

// --- PIEDRA, PAPEL O TIJERA -------------------------------------------------------

static bool framePPT(EstadoJuego &j, uint32_t t, const EntradaJuego &e, Lienzo &l)
{
    uint32_t en = t - j.faseDesde;
    float te = en * 0.001f;
    switch (j.fase) {
        case F_INTRO:
            // Las tres opciones pasan rapido: "vamos a jugar a esto"
            dibujarPPT(l, (int)(en / 220) % 3, 0.9f, 0);
            if (en > 660) fase(j, F_ELIGE, t);
            return true;
        case F_ELIGE: {
            // Pasan lento: apreta A o B (o toca el logo) cuando sale la tuya
            int k = (int)(en / 800) % 3;
            float v = 0.55f + 0.45f * campana((en % 800) / 800.0f, 0.15f);
            dibujarPPT(l, k, v, 0);
            if (e.pulsoA || e.pulsoB || e.toque) {
                j.jugador = k;
                j.valor = azarEntero(j.azar, 3);
                fase(j, F_CUENTA, t);
                j.sonido = S_CUENTA;
            } else if (en > 9600) {
                juegoTerminar(j);           // nadie jugo: vuelve a la cara
                return false;
            }
            return true;
        }
        case F_CUENTA: {
            // "pie-dra, pa-pel, ti-je-ra": el puno de Kira sube y baja 3 veces
            float dy = -0.6f * fabsf(sinf(3.14159265f * te / 0.3f));
            dibujarPPT(l, 0, 0.8f, dy);
            if (en > 900) {
                fase(j, F_REVELA, t);
                j.sonido = S_YA;
                // 1 gana el jugador, 0 empate, -1 gana Kira
                // piedra(0) le gana a tijera(2), papel(1) a piedra(0), tijera(2) a papel(1)
                if (j.jugador == j.valor) j.resultado = 0;
                else if ((j.jugador + 3 - j.valor) % 3 == 1) j.resultado = 1;
                else j.resultado = -1;
                static const char *N[3] = { "PIEDRA", "PAPEL", "TIJERA" };
                char a[40];
                snprintf(a, sizeof(a), "JUEGO:PPT:%s:%s:%d", N[j.jugador], N[j.valor], j.resultado);
                avisar(j, a);
            }
            return true;
        }
        case F_REVELA:
            // Lo tuyo tenue un instante, despues lo de Kira bien prendido
            if (en < 450) dibujarPPT(l, j.jugador, 0.35f, 0);
            else          dibujarPPT(l, j.valor, 1.0f, 0);
            if (en > 1500) {
                fase(j, F_RESULTADO, t);
                j.sonido = j.resultado < 0 ? S_GANA : (j.resultado > 0 ? S_PIERDE : S_OH);
            }
            return true;
        case F_RESULTADO:
            if (j.resultado < 0)      caraGana(l, te);
            else if (j.resultado > 0) caraPierde(l, te);
            else                      caraSorpresa(l, te);
            if (en > 2400) { juegoTerminar(j); return false; }
            return true;
        default:
            juegoTerminar(j);
            return false;
    }
}

// --- REFLEJOS ---------------------------------------------------------------------

static int estrellas(uint32_t ms)
{
    if (ms < 230) return 5;
    if (ms < 300) return 4;
    if (ms < 400) return 3;
    if (ms < 550) return 2;
    return 1;
}

static bool frameReflejos(EstadoJuego &j, uint32_t t, const EntradaJuego &e, Lienzo &l)
{
    uint32_t en = t - j.faseDesde;
    float te = en * 0.001f;
    bool aprieta = e.pulsoA || e.pulsoB || e.toque;
    switch (j.fase) {
        case F_INTRO:
            // Mira fijo, concentrada (ojos que se achican)
            lienzoPixel(l, 1, 1, 1.0f); lienzoPixel(l, 3, 1, 1.0f);
            lienzoPixel(l, 0, 0, 0.5f * suave(te / 0.5f)); lienzoPixel(l, 4, 0, 0.5f * suave(te / 0.5f));
            lienzoPixel(l, 1, 3, 0.6f); lienzoPixel(l, 2, 3, 0.6f); lienzoPixel(l, 3, 3, 0.6f);
            if (en > 800) {
                j.espera = 1500 + (uint32_t)azarEntero(j.azar, 2500);
                fase(j, F_ESPERA, t);
            }
            return true;
        case F_ESPERA: {
            // "Espera...": tres puntitos que respiran en la fila de abajo
            for (int i = 0; i < 3; i++) {
                float v = 0.2f + 0.5f * (0.5f + 0.5f * sinf(6.2831853f * (te * 1.5f - i * 0.25f)));
                lienzoPixel(l, 1 + i, 2, v);
            }
            if (aprieta) {
                fase(j, F_TRAMPA, t);
                j.sonido = S_OFENDIDA;
                avisar(j, "JUEGO:REFLEJOS:TRAMPA");
            } else if (en >= j.espera) {
                fase(j, F_YA, t);
                j.sonido = S_YA;
            }
            return true;
        }
        case F_YA:
            // ¡FLASH! toda la matriz
            for (int y = 0; y < 5; y++)
                for (int x = 0; x < 5; x++) lienzoPixel(l, x, y, 1.0f);
            if (aprieta || en > 2000) {
                j.reaccionMs = aprieta ? en : 2000;
                fase(j, F_RESULTADO, t);
                int s = aprieta ? estrellas(j.reaccionMs) : 0;
                j.sonido = s >= 5 ? S_GANA : (s >= 3 ? S_DING : (s >= 1 ? S_TOQUE : S_PIERDE));
                char a[40];
                snprintf(a, sizeof(a), "JUEGO:REFLEJOS:%u", (unsigned)j.reaccionMs);
                avisar(j, a);
            }
            return true;
        case F_RESULTADO: {
            int s = j.reaccionMs >= 2000 ? 0 : estrellas(j.reaccionMs);
            if (s >= 5)      caraSorpresa(l, te);
            else if (s >= 3) caraContenta(l);
            else if (s >= 1) caraMeh(l);
            else             caraDormida(l, te);
            // Las estrellas en la fila de abajo, de a una
            for (int i = 0; i < s; i++)
                if (en > 300 + (uint32_t)i * 120) lienzoPixel(l, i, 4, 1.0f);
            if (en > 2800) { juegoTerminar(j); return false; }
            return true;
        }
        case F_TRAMPA:
            caraTramposo(l, te);
            if (en > 1600) { juegoTerminar(j); return false; }
            return true;
        default:
            juegoTerminar(j);
            return false;
    }
}

bool juegoFrame(EstadoJuego &j, uint32_t t, const EntradaJuego &e, Lienzo &out)
{
    j.sonido = j.sonidoInicio;
    j.sonidoInicio = 0;
    j.hayAviso = false;
    lienzoLimpiar(out);
    switch (j.juego) {
        case J_DADO:     return frameDado(j, t, e, out);
        case J_PPT:      return framePPT(j, t, e, out);
        case J_REFLEJOS: return frameReflejos(j, t, e, out);
        default:         return false;
    }
}
