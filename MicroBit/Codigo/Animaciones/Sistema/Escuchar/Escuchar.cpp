/**
 * Escuchar.cpp - MODO ESCUCHA GPT 🎙️🌀
 *
 * El micro:bit escucha en vivo (aro + audio streaming) y streamea TODO el
 * audio a la PC: el CORTE lo decide el SERVER con Silero VAD (red neuronal,
 * mira cada frame y distingue voz real de ruido/pausas) y nos avisa con
 * STOP por serial. Este VAD de energia queda SOLO DE RESPALDO: corta por
 * silencio larguisimo (4s, si el server se murio) o por timeout.
 *
 * Niveles (desviacion media del chunk, 8-bit signed):
 *   silencio -> ~2..8
 *   voz      -> ~15..60
 * Umbrales: voz > 10 (sostenido 150ms), respaldo < 7 por 4s.
 * Timeouts: nadie hablo en 8s -> corta vacio. Maximo 30s -> corta igual.
 */
#include "Escuchar.h"
#include "../Grabar/Grabar.h"
#include <math.h>

bool modoEscuchar = false;

// Estado del VAD
static bool     huboVoz = false;      // el usuario ya empezo a hablar?
static uint64_t ultimoConVoz = 0;     // cuando fue la ultima vez que hubo voz
static uint64_t inicioEscucha = 0;    // cuando arranco esta sesion
static int      framesVoz = 0;        // frames seguidos con voz (umbral)

// Umbrales (nivel 0..~90, desviacion media del chunk)
#define UMBRAL_VOZ    10.0f
#define UMBRAL_CORTE  7.0f
#define SILENCIO_CORTE_MS   4000   // RESPALDO: 4s de silencio (el server corta antes)
#define SIN_VOZ_MAX_MS      8000   // nadie hablo en 8s -> cortar vacio
#define ESCUCHA_MAX_MS      30000  // maximo total 30s

// ---------------------------------------------------------------------------
// Aro de la escucha: igual que la variante Aro pero con el nivel del sink
// (radio 0.3 puntito -> 2.4 pantalla llena, respira en silencio)
// ---------------------------------------------------------------------------
static void pintarAroEscucha()
{
    float n = nivelAudio / 90.0f;          // normalizado 0..1
    if (n < 0.0f) n = 0.0f;
    if (n > 1.0f) n = 1.0f;

    float t = uBit.systemTime() / 1000.0f;
    float osc = sinf(t * 3.1f);
    float respiro = 0.13f * osc * (1.0f - n * 0.6f);

    float radio = 0.30f + n * 2.10f + respiro;
    float brillo = 90.0f + n * 165.0f;
    brillo += 40.0f * (1.0f - n) * (0.5f + 0.5f * osc);

    for (int y = 0; y < 5; y++) {
        for (int x = 0; x < 5; x++) {
            float dx = x - 2.0f, dy = y - 2.0f;
            float d = sqrtf(dx * dx + dy * dy);
            float cerca = 1.0f - fabsf(d - radio) / 0.9f;
            if (cerca < 0.0f) cerca = 0.0f;
            if (cerca > 1.0f) cerca = 1.0f;
            int br = (int)(brillo * cerca * cerca);
            if (br > 255) br = 255;
            uBit.display.image.setPixelValue(x, y, br);
        }
    }
}

void escucharIniciar()
{
    if (modoEscuchar) return;   // ya escuchando (idempotente)

    modoEscuchar = true;
    huboVoz = false;
    framesVoz = 0;
    inicioEscucha = uBit.systemTime();
    ultimoConVoz = inicioEscucha;

    uBit.display.setBrightness(180);
    uBit.display.image.clear();

    // Arranca el stream de audio crudo a la PC (AUDIO:START + samples).
    // El sink de grabacion calcula nivelAudio con cada chunk (para el VAD
    // y para el aro). NO usamos iniciarVoz/MicFft: el mic solo puede
    // alimentar un downstream, y el grabador es el que manda el audio.
    grabarIniciar();
}

void escucharDetener()
{
    if (!modoEscuchar) return;

    modoEscuchar = false;

    // Cierra el stream de audio (AUDIO:END, con retry por si el serial
    // esta ocupado con el ultimo chunk)
    grabarDetener();

    uBit.display.setBrightness(90);   // vuelve a la luz tenue
}

void escucharFrame()
{
    if (!modoEscuchar) return;
    pintarAroEscucha();
    uBit.sleep(16);   // ~60 fps
}

void escucharVad()
{
    if (!modoEscuchar) return;

    // Nivel del mic desde el sink de grabacion (actualizado por el stream)
    float nivel = nivelAudio;

    uint64_t ahora = uBit.systemTime();

    if (nivel > UMBRAL_VOZ) {
        framesVoz++;
        // Sostenido 150ms (9 frames a 60fps): un ruidito no cuenta
        if (framesVoz >= 9) {
            if (!huboVoz) huboVoz = true;
            ultimoConVoz = ahora;
        }
    } else {
        framesVoz = 0;
    }

    // Reglas de corte
    if (!huboVoz) {
        // nadie hablo todavia: dar 7s, despues cortar vacio
        if (ahora - inicioEscucha > SIN_VOZ_MAX_MS)
            escucharDetener();
    } else {
        // ya hubo voz: cortar por silencio sostenido o por timeout max
        bool silencioLargo = (nivel < UMBRAL_CORTE)
                             && (ahora - ultimoConVoz > SILENCIO_CORTE_MS);
        if (silencioLargo || (ahora - inicioEscucha > ESCUCHA_MAX_MS))
            escucharDetener();
    }
}
