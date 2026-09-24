/**
 * HablarTriste.h - La boca de la TRISTEZA que HABLA 😢🗣️
 *
 * Cada emocion tiene SU hablar. Cuando la IA manda "TALK" y la emocion
 * activa es TRISTE, la cara triste habla:
 *   - La boca "wah" (el labio triste) se ABRE y CIERRA a ritmo de
 *     habla: el medio del frown se abre hacia abajo, como quien
 *     habla a punto de llorar. Las esquinas quedan firmes.
 *   - UNA FIBRA de CODAL parpadea los ojos EN PARALELO, con parpadeo
 *     PESADO e impredecible: 75% ambos ojos, 12% guino izq, 12%
 *     guino der, espera 2-6s (mas lento que la alegria).
 *
 * Al terminar de hablar (otra emocion o STOP) modoHablar se apaga,
 * la fibra muere sola y el bucle principal vuelve a la cara triste
 * en reposo (animarTriste).
 */
#ifndef HABLARTRISTE_H
#define HABLARTRISTE_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// TALK (con tristeza activa): dibuja la cara triste hablando y lanza
// la fibra de parpadeo pesado
void iniciarHablarTriste();

// Una pasada de la boca "wah" hablando (la llama el bucle principal)
void animarBocaTriste();

#endif // HABLARTRISTE_H
