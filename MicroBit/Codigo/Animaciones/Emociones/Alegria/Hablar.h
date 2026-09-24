/**
 * Hablar.h - La boca que HABLA (lip-sync sutil) + parpadeo ASINCRONO
 *
 * Cuando la IA manda "TALK":
 *   - El bucle principal parpadea la linea de DIENTES (1,3)(2,3)(3,3)
 *     a ritmo de habla. Las MEJILLAS (esquinas + curva de la sonrisa)
 *     se quedan quietas.
 *   - UNA FIBRA de CODAL parpadea los OJOS EN PARALELO (asincrono real)
 *     de forma IMPREDECIBLE: uBit.random() decide el tiempo entre
 *     parpadeos (1.5s-5s) y si son ambos ojos, guino izq o guino der.
 *
 * Asi la cara sigue "viva" mientras habla, como una persona de verdad.
 */
#ifndef HABLAR_H
#define HABLAR_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Estado global: true = la boca esta hablando (lo pone el comando TALK)
extern bool modoHablar;

// TALK: dibuja la cara, marca modoHablar y lanza la fibra de parpadeo
void iniciarHablar();

// Otra emocion: apaga modoHablar (la fibra muere sola en su siguiente ciclo)
void detenerHablar();

// Una pasada del movimiento de dientes hablando (la llama el bucle principal)
void animarBocaHablando();

#endif // HABLAR_H
