/**
 * Escuchar.cpp - ESCUCHA MANUAL DEL MICRO:BIT 🎙️🔘
 *
 * La captura queda abierta hasta que una persona decide terminarla:
 *
 *   A -> iniciar o finalizar y enviar (AUDIO:END)
 *   B -> cancelar sin transcribir (AUDIO:CANCEL)
 *
 * El aro sigue la desviacion media de los samples (nivelAudio), calculada
 * por el sink de Grabar. No hay deteccion de voz ni corte por silencio: el
 * control es deliberadamente manual.
 */
#include "Escuchar.h"
#include "../Grabar/Grabar.h"
#include <math.h>

bool modoEscuchar = false;

// ---------------------------------------------------------------------------
// Aro de escucha: radio 0.3 -> 2.4, reacciona al nivel del microfono y
// respira suavemente cuando hay silencio.
// ---------------------------------------------------------------------------
static void pintarAroEscucha()
{
    float n = nivelAudio / 90.0f;
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

void escucharArmar()
{
    if (modoEscuchar) return;
    modoEscuchar = true;
    nivelAudio = 0.0f;
    uBit.display.setBrightness(180);
    uBit.display.image.clear();
}

bool escucharMicroActivo()
{
    return grabandoSerial;
}

void escucharIniciar()
{
    if (grabandoSerial) return;

    // Si la PC ya nos armó con ESCUCHAR, conservamos el aro y solo abrimos
    // el microfono al primer A. Si se llama directo desde el boton, hace
    // ambas cosas.
    if (!modoEscuchar) {
        modoEscuchar = true;
        nivelAudio = 0.0f;
        uBit.display.setBrightness(180);
        uBit.display.image.clear();
    }

    // El stream crudo y el aro usan el mismo microfono. No se llama al VAD:
    // el hardware queda esperando el siguiente flanco de A o B.
    grabarIniciar();
    if (!grabandoSerial) {
        // El microfono no pudo arrancar; no dejamos un modo fantasma.
        modoEscuchar = false;
        uBit.display.setBrightness(90);
    }
}

void escucharDetener()
{
    if (!modoEscuchar) return;

    modoEscuchar = false;
    nivelAudio = 0.0f;
    grabarDetener();
    uBit.display.setBrightness(90);
}

void escucharCancelar()
{
    if (!modoEscuchar) return;

    modoEscuchar = false;
    nivelAudio = 0.0f;
    grabarCancelar();
    uBit.display.setBrightness(90);
}

void escucharVad()
{
    // Se conserva la funcion para no romper llamadas viejas del firmware.
    // El modo manual ya no decide el fin por voz/silencio.
}

void escucharFrame()
{
    if (!modoEscuchar) return;
    pintarAroEscucha();
    uBit.sleep(16);   // ~60 fps
}
