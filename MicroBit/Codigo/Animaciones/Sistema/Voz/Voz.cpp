/**
 * Voz.cpp - El DIRECTOR de las variantes del aro de voz
 *
 * Cada variante vive en su PROPIA CARPETA con su propio codigo:
 *   Voz/Aro/Aro.cpp, Voz/Sonar/Sonar.cpp, ...
 * Aqui solo se REGISTRAN en el array VARIANTES y se elige al azar.
 * El nivel de voz suavizado compartido (vozNivelSuave) vive aca para
 * que las variantes no dupliquen el suavizado tipo ecualizador.
 *
 * PARA AGREGAR UNA VARIANTE NUEVA:
 *   1. Crear Voz/MiVoz/MiVoz.h + MiVoz.cpp
 *   2. Incluir su header aca abajo
 *   3. Meter vozFrameMiVoz() en VARIANTES y subir NUM_VOZ
 */
#include "Voz.h"
#include "../MicFft/MicFft.h"
#include "../Grabar/Grabar.h"   // grabandoSerial: callar debug durante grabacion
#include "Aro/Aro.h"
#include "Sonar/Sonar.h"
#include "Orbe/Orbe.h"
#include "Anillos/Anillos.h"
#include "Cometa/Cometa.h"

// Registro de variantes (todas con la misma firma: void vozFrameXxx())
static const int NUM_VOZ = 5;
typedef void (*FrameVoz)();
static const FrameVoz VARIANTES[NUM_VOZ] = {
    vozFrameAro,       // 0: aro que crece y respira (el clasico)
    vozFrameSonar,     // 1: ondas que viajan con cada palabra 📡
    vozFrameOrbe,      // 2: mancha que tiembla/hierve con la voz ⚡
    vozFrameAnillos,   // 3: dos anillos, interno rapido + externo lento 🪐
    vozFrameCometa     // 4: aro con rastro que persigue el nivel ☄️
};

// ---------------------------------------------------------------------------
// Nivel compartido: maximo de las 5 bandas, suavizado (ataque rapido,
// release lento). Igual que el ecualizador Barra: la voz sube al toque
// y el aro no se desinfla de golpe al dejar de hablar.
// ---------------------------------------------------------------------------
static float nivelSuave = 0.0f;

float vozNivelSuave()
{
    const float *b = micFftBandas();
    float nivel = 0.0f;
    for (int i = 0; i < 5; i++)
        if (b[i] > nivel) nivel = b[i];

    float dif = nivel - nivelSuave;
    nivelSuave += dif * (dif > 0.0f ? 0.30f : 0.10f);
    if (nivelSuave < 0.02f) nivelSuave = 0.0f;
    if (nivelSuave > 4.5f)  nivelSuave = 4.5f;
    return nivelSuave;
}

// ---------------------------------------------------------------------------
// Modo bucle (igual que Loading)
// ---------------------------------------------------------------------------
bool modoVoz = false;
static int varianteActual = 0;   // la elegida al azar al iniciar

// VOZ0..VOZ4: activa ESA variante. Si ya hay otra, cambia a esta.
void iniciarVoz(int idx)
{
    if (idx < 0 || idx >= NUM_VOZ) return;
    varianteActual = idx;
    modoVoz = true;
    // Prende el microfono DE VERDAD (idempotente: si ya corre, no hace nada)
    micFftIniciar();
    uBit.display.setBrightness(180);
    uBit.display.image.clear();
}

// VOZ: variante al azar (nunca carga igual dos veces). Si ya esta en
// modo voz, no reinicia. VOZ0..VOZ4 fuerzan una variante especifica.
void iniciarVoz()
{
    if (modoVoz) return;
    iniciarVoz(uBit.random(NUM_VOZ));
}

void detenerVoz()
{
    modoVoz = false;
    // Apaga el microfono de verdad: stream ADC + corriente del MEMS.
    // Seguro siempre (no-op si nunca se prendio).
    micFftDetener();
}

// Un frame de la variante activa, sin limpiar pantalla entre pasadas
void mostrarVozBucle()
{
    // Debug leve cada ~1s: variante activa, nivel y FFTs (verifica mic).
    // OJO: callado durante la grabacion/escucha (el debug ensuciaria el
    // stream de audio crudo que sale por el MISMO serial).
    if (!grabandoSerial) {
        static int dbg = 0;
        if (++dbg >= 63) {
            dbg = 0;
            uBit.serial.printf("VOZ%d n=%d W%d\n", varianteActual,
                               (int)(vozNivelSuave() * 20.0f), micFftConteo());
        }
    }
    if (varianteActual >= 0 && varianteActual < NUM_VOZ)
        VARIANTES[varianteActual]();
}
