/**
 * Pantalla.cpp - Iconos, texto, flecha de brujula y temporizador (ver Pantalla.h)
 */
#include "Pantalla.h"
#include "Ruido.h"
#include <string.h>

// Los dibujos: 5 filas de 5 bits (bit 4 = columna 0)
static const uint8_t ICONOS[NUM_ICONOS][5] = {
    {0x0A, 0x1F, 0x1F, 0x0E, 0x04},   // corazon
    {0x15, 0x0E, 0x1F, 0x0E, 0x15},   // sol
    {0x00, 0x0C, 0x1E, 0x1F, 0x00},   // nube
    {0x0E, 0x1F, 0x00, 0x15, 0x0A},   // lluvia
    {0x04, 0x1F, 0x0E, 0x0A, 0x11},   // estrella
    {0x06, 0x08, 0x10, 0x08, 0x06},   // luna
    {0x00, 0x01, 0x02, 0x14, 0x08},   // si (check)
    {0x11, 0x0A, 0x04, 0x0A, 0x11},   // no (x)
    {0x04, 0x0E, 0x1F, 0x0A, 0x0E},   // casa
    {0x07, 0x05, 0x05, 0x1B, 0x1B},   // musica
    {0x06, 0x0C, 0x1F, 0x06, 0x0C},   // rayo
    {0x0E, 0x02, 0x04, 0x00, 0x04},   // pregunta
    {0x04, 0x0A, 0x0A, 0x1F, 0x0E},   // termometro
};
static const char *const NOMBRES[NUM_ICONOS] = {
    "CORAZON", "SOL", "NUBE", "LLUVIA", "ESTRELLA", "LUNA",
    "SI", "NO", "CASA", "MUSICA", "RAYO", "PREGUNTA", "TERMOMETRO",
};

// Flechas en 8 direcciones (norte arriba, en sentido horario)
static const uint8_t FLECHAS[8][5] = {
    {0x04, 0x0E, 0x15, 0x04, 0x04},   // N
    {0x07, 0x03, 0x05, 0x08, 0x10},   // NE
    {0x04, 0x02, 0x1F, 0x02, 0x04},   // E
    {0x10, 0x08, 0x05, 0x03, 0x07},   // SE
    {0x04, 0x04, 0x15, 0x0E, 0x04},   // S
    {0x01, 0x02, 0x14, 0x18, 0x1C},   // SO
    {0x04, 0x08, 0x1F, 0x08, 0x04},   // O
    {0x1C, 0x18, 0x14, 0x02, 0x01},   // NO
};

#define TEXTO_PASO_MS   95     // ms por columna al desplazar
#define TIMER_ASOMA_CADA 15000u
#define TIMER_ASOMA_MS    1300u
#define ALARMA_MS         2600u

static void dibujarBits(Lienzo &l, const uint8_t filas[5], float v, int dx)
{
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            if (filas[y] & (0x10 >> x)) lienzoPixel(l, x + dx, y, v);
}

void pantallaIniciar(EstadoPantalla &p)
{
    memset(&p, 0, sizeof(p));
}

int pantallaIconoPorNombre(const char *nombre)
{
    if (!nombre) return -1;
    for (int i = 0; i < NUM_ICONOS; i++) {
        const char *a = NOMBRES[i], *b = nombre;
        while (*a && *b && (*a == (*b >= 'a' && *b <= 'z' ? *b - 32 : *b))) { a++; b++; }
        if (!*a && !*b) return i;
    }
    return -1;
}

void pantallaIcono(EstadoPantalla &p, int icono, uint32_t t, uint32_t ms)
{
    if (icono < 0 || icono >= NUM_ICONOS) return;
    p.capa = CAPA_ICONO;
    p.icono = (uint8_t)icono;
    p.inicio = t;
    p.fin = t + ms;
}

void pantallaTexto(EstadoPantalla &p, const char *texto, uint32_t t)
{
    int n = 0;
    for (; texto && texto[n] && n < (int)sizeof(p.texto) - 1; n++) p.texto[n] = texto[n];
    p.texto[n] = '\0';
    p.capa = CAPA_TEXTO;
    p.inicio = t;
    // entra por la derecha, pasa entero y sale por la izquierda
    p.fin = t + (uint32_t)((n * 6 + 5) * TEXTO_PASO_MS);
}

void pantallaFlecha(EstadoPantalla &p, float rumbo, uint32_t t, uint32_t ms)
{
    p.capa = CAPA_FLECHA;
    p.rumboObjetivo = rumbo;
    p.inicio = t;
    p.fin = t + ms;
}

void pantallaTimer(EstadoPantalla &p, uint32_t segundos, uint32_t t)
{
    if (segundos == 0) { p.timerActivo = false; return; }
    p.timerActivo = true;
    p.timerInicio = t;
    p.timerFin = t + segundos * 1000u;
}

void pantallaCancelar(EstadoPantalla &p)
{
    p.capa = CAPA_NADA;
    p.timerActivo = false;
}

bool pantallaOcupada(const EstadoPantalla &p, uint32_t t)
{
    return p.capa != CAPA_NADA && (int32_t)(t - p.fin) < 0;
}

// Desvanecido de entrada y salida de una capa
static float envolvente(const EstadoPantalla &p, uint32_t t)
{
    float dentro = (float)(t - p.inicio) / 180.0f;
    float fuera = (float)(int32_t)(p.fin - t) / 250.0f;
    return limitar(dentro < fuera ? dentro : fuera, 0.0f, 1.0f);
}

static void dibujarTimer(Lienzo &l, const EstadoPantalla &p, uint32_t t, FuenteGlifo glifo)
{
    uint32_t falta = p.timerFin - t;
    if (falta <= 5000 && glifo) {
        // 5, 4, 3, 2, 1 grandes
        char c = (char)('1' + (falta - 1) / 1000);
        const uint8_t *g = glifo(c);
        float pulso = 1.0f - 0.4f * ((float)((falta - 1) % 1000) / 1000.0f);
        if (g) dibujarBits(l, g, pulso, 0);
        return;
    }
    // Barra de lo que falta: 25 LEDs que se van apagando
    float q = (float)falta / (float)(p.timerFin - p.timerInicio);
    int encendidos = (int)(q * 25.0f + 0.999f);
    for (int i = 0; i < 25; i++)
        lienzoPixel(l, i % 5, i / 5, i < encendidos ? 0.8f : 0.06f);
}

bool pantallaFrame(EstadoPantalla &p, uint32_t t, float rumboKira, FuenteGlifo glifo,
                   Lienzo &out)
{
    p.eventos = 0;

    // El temporizador corre por debajo de cualquier capa
    if (p.timerActivo && (int32_t)(t - p.timerFin) >= 0) {
        p.timerActivo = false;
        p.capa = CAPA_ALARMA;
        p.inicio = t;
        p.fin = t + ALARMA_MS;
        p.eventos |= EVP_TIMER_FIN;
    }

    if (p.capa != CAPA_NADA && (int32_t)(t - p.fin) >= 0)
        p.capa = CAPA_NADA;

    lienzoLimpiar(out);
    if (p.capa == CAPA_ICONO) {
        float te = (float)(t - p.inicio) * 0.001f;
        float late = 0.85f + 0.15f * sinf(6.2831853f * 1.2f * te);
        dibujarBits(out, ICONOS[p.icono], envolvente(p, t) * late, 0);
        return true;
    }
    if (p.capa == CAPA_TEXTO) {
        if (!glifo) { p.capa = CAPA_NADA; return false; }
        int col = (int)((t - p.inicio) / TEXTO_PASO_MS);   // columnas corridas
        int n = (int)strlen(p.texto);
        for (int i = 0; i < n; i++) {
            int x0 = 5 + i * 6 - col;                     // donde cae la letra
            if (x0 > 4 || x0 < -5) continue;
            const uint8_t *g = glifo(p.texto[i]);
            if (g) dibujarBits(out, g, 0.9f, x0);
        }
        return true;
    }
    if (p.capa == CAPA_FLECHA) {
        if (rumboKira < 0.0f) {
            // Sin brujula calibrada no hay a donde apuntar: un "?" y aviso
            dibujarBits(out, ICONOS[IC_PREGUNTA], envolvente(p, t), 0);
            if (t - p.inicio < 20) p.eventos |= EVP_SIN_BRUJULA;
            return true;
        }
        float rel = p.rumboObjetivo - rumboKira;
        while (rel < 0.0f) rel += 360.0f;
        while (rel >= 360.0f) rel -= 360.0f;
        int dir = (int)((rel + 22.5f) / 45.0f) % 8;
        dibujarBits(out, FLECHAS[dir], envolvente(p, t), 0);
        return true;
    }
    if (p.capa == CAPA_ALARMA) {
        // ¡Tiempo!: la matriz entera titila
        float on = ((t - p.inicio) / 160) % 2 == 0 ? 1.0f : 0.15f;
        for (int y = 0; y < 5; y++)
            for (int x = 0; x < 5; x++) lienzoPixel(out, x, y, on);
        return true;
    }
    // Temporizador: se asoma un instante cada 15 s, y los ultimos 5 s se queda
    if (p.timerActivo) {
        uint32_t paso = t - p.timerInicio;
        uint32_t falta = p.timerFin - t;
        bool asoma = (paso % TIMER_ASOMA_CADA) < TIMER_ASOMA_MS && paso > 800;
        if (falta <= 5000 || asoma) {
            dibujarTimer(out, p, t, glifo);
            return true;
        }
    }
    return false;
}
