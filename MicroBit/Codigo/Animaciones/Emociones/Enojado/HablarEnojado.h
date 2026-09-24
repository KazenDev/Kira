/**
 * HablarEnojado.h - La boca del ENOJADO que HABLA 😠🗣️
 *
 * Cada emocion tiene SU hablar. Cuando la IA manda "TALK" y la emocion
 * activa es ENOJADO, el enojado habla GRUÑENDO:
 *   - La mandibula se abre y cierra RAPIDO (el gruñido): el hueco
 *     (2,4) de los dientes pulsa a ritmo de habla enojada.
 *     Las cejas siguen fruncidas y la boca seria de fondo.
 *   - UNA FIBRA de CODAL parpadea los ojos EN PARALELO con parpadeo
 *     BRUSCO e impredecible: ~80% ambos ojos, 10% guino izq, 10%
 *     guino der, espera 0.8-2.5s (el enojado casi no parpadea: mira
 *     fijo y directo).
 *
 * Al terminar de hablar (otra emocion o STOP) modoHablar se apaga,
 * la fibra muere sola y el bucle principal vuelve al enojo en reposo.
 */
#ifndef HABLARENOJADO_H
#define HABLARENOJADO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// TALK (con enojo activo): dibuja la cara enojada hablando y lanza
// la fibra de parpadeo brusco
void iniciarHablarEnojado();

// Una pasada de la boca gruñendo (la llama el bucle principal)
void animarBocaEnojada();

#endif // HABLARENOJADO_H
