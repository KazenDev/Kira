/**
 * Oido.cpp - Deteccion de voces que llegan y golpes de ruido (ver Oido.h)
 */
#include "Oido.h"

#define BULLA_SOBRE_BASE     10.0f    // "hay alguien hablando"
#define CALMA_SOBRE_BASE      5.0f    // "volvio la calma" (histeresis)
#define VOCES_SOSTENIDO_MS   1200u
#define VOCES_CALMA_PREVIA   40000u   // tiene que venir de un rato tranquilo
#define VOCES_ENFRIAR_MS     60000u
#define APLAUSO_SOBRE_BASE   35.0f
#define APLAUSO_SALTO        25.0f    // sube esto de una muestra a la otra
#define APLAUSO_ENFRIAR_MS    8000u
#define APLAUSO_MINIMO       55.0f    // y ademas fuerte de verdad (no solo "mas que la base")
// Al arrancar (o al retomar el microfono) la base parte de lo que haya y se
// ajusta RAPIDO: medido en la placa, empezar con la base en 0 hacia que
// cualquier ruido normal pareciera un aplauso.
#define ARRANQUE_MS          15000u

static float limitarF(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

void oidoIniciar(DetectorOido &d, uint32_t t)
{
    d.listo = false;
    d.rapido = d.base = d.anterior = 0.0f;
    d.ultimoT = t;
    d.enBulla = false;
    d.avisado = false;
    d.altoDesde = 0;
    d.finBulla = t;          // desde el arranque cuenta como calma
    d.silencioPrevio = 0;
    d.ultimoVoces = 0;
    d.ultimoAplauso = 0;
    d.arranque = t;
}

float oidoActividad(const DetectorOido &d)
{
    float a = d.rapido - d.base;
    return a > 0.0f ? a : 0.0f;
}

int oidoMuestra(DetectorOido &d, float nivel, uint32_t t, bool sordo)
{
    float dt = (float)(t - d.ultimoT) * 0.001f;
    d.ultimoT = t;
    if (dt <= 0.0f) dt = 0.001f;
    if (dt > 1.0f) dt = 1.0f;

    if (!d.listo) {
        d.rapido = d.base = d.anterior = nivel;
        d.listo = true;
        return OIDO_NADA;
    }

    if (sordo) {
        // Kira esta sonando: no se escucha. Y lo que venga justo despues no
        // cuenta como "alguien llego": la calma previa arranca de nuevo.
        d.enBulla = false;
        d.avisado = false;
        d.finBulla = t;
        d.anterior = nivel;
        return OIDO_NADA;
    }

    d.rapido += (nivel - d.rapido) * limitarF(dt / 0.35f, 0.0f, 1.0f);
    // La base baja rapido (silencio) y sube lento (bullicio sostenido). En
    // el arranque sigue al nivel enseguida: todavia no sabe como es el lugar.
    bool arrancando = t - d.arranque < ARRANQUE_MS;
    float tau = arrancando ? 1.5f : (nivel < d.base ? 4.0f : 30.0f);
    d.base += (nivel - d.base) * limitarF(dt / tau, 0.0f, 1.0f);

    float salto = nivel - d.anterior;
    d.anterior = nivel;
    if (arrancando) {
        d.finBulla = t;      // la calma previa se empieza a contar despues
        return OIDO_NADA;
    }

    // APLAUSO: golpe brusco y fuerte
    if (nivel - d.base > APLAUSO_SOBRE_BASE && salto > APLAUSO_SALTO && nivel > APLAUSO_MINIMO &&
        (d.ultimoAplauso == 0 || t - d.ultimoAplauso > APLAUSO_ENFRIAR_MS)) {
        d.ultimoAplauso = t;
        return OIDO_APLAUSO;
    }

    // VOCES: bulla sostenida que viene despues de un rato de calma
    float delta = d.rapido - d.base;
    if (delta > BULLA_SOBRE_BASE) {
        if (!d.enBulla) {
            d.enBulla = true;
            d.altoDesde = t;
            d.silencioPrevio = t - d.finBulla;
        }
        if (!d.avisado && t - d.altoDesde >= VOCES_SOSTENIDO_MS &&
            d.silencioPrevio >= VOCES_CALMA_PREVIA &&
            (d.ultimoVoces == 0 || t - d.ultimoVoces > VOCES_ENFRIAR_MS)) {
            d.avisado = true;
            d.ultimoVoces = t;
            return OIDO_VOCES;
        }
    } else if (delta < CALMA_SOBRE_BASE && d.enBulla) {
        d.enBulla = false;
        d.avisado = false;
        d.finBulla = t;
    }
    return OIDO_NADA;
}
