/**
 * Habla.h - BOCA QUE HABLA sin escuchar el audio (prosodia sintetica)
 *
 * La placa no oye la voz de Kira (el TTS suena en el cel o la PC), asi que la
 * boca no puede seguir el audio real. Lo que SI puede es moverse como se
 * mueve una boca hablando, en vez de un metronomo de 280 ms:
 *
 *   SILABAS    cada una dura distinto (el espanol articula ~6-7,8 silabas/s,
 *              Pellegrino 2011) y abre distinto: unas apenas, otras de golpe.
 *   PALABRAS   grupos de 1-4 silabas con un hueco corto (boca cerrada).
 *   FRASES     cada 3-8 palabras una pausa (~400-600 ms en espanol, Campione
 *              & Veronis 2002). Ahi los ojos parpadean (se parpadea en las
 *              pausas, Cornelis 2025): por eso el modulo AVISA el fin de frase.
 *   EMOCION    cambia el ritmo y la apertura: triste lento y chico, miedo
 *              rapido, enojo con golpes fuertes (ver cada Personalidad).
 *   ENFASIS    algunas silabas fuertes: la cara las acompana (cejas, brillo).
 */
#ifndef VIDA_HABLA_H
#define VIDA_HABLA_H

#include <stdint.h>
#include "Azar.h"

struct ParamHabla
{
    float silabaMs;        // duracion media de una silaba
    float silabaCv;
    float silabasPalabra;  // media de silabas por palabra
    float huecoMs;         // hueco entre palabras
    float pausaMs;         // pausa media entre frases
    uint8_t palabrasMin;   // palabras por frase
    uint8_t palabrasMax;
    float ampMin, ampMax;  // apertura de cada silaba (0..1)
    float enfasisProb;     // probabilidad de silaba con enfasis
};

struct EstadoHabla
{
    bool activa;
    uint8_t fase;          // silaba / hueco / pausa
    uint32_t inicio;
    float dur;
    float pico;
    int silabasRestantes;
    int palabrasRestantes;
    // Salidas del frame
    float apertura;        // 0..1
    float enfasis;         // 1 en la silaba fuerte, decae solo
    bool finFrase;         // true SOLO en el frame que empieza una pausa
};

void hablaIniciar(EstadoHabla &h, const ParamHabla &p, uint32_t t, Azar &azar);
void hablaDetener(EstadoHabla &h);
void hablaAvanzar(EstadoHabla &h, const ParamHabla &p, uint32_t t, float dt, Azar &azar);

#endif // VIDA_HABLA_H
