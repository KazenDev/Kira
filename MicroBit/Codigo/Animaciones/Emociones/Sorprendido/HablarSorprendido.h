/**
 * HablarSorprendido.h - La boca del SORPRENDIDO que HABLA 😲🗣️
 *
 * Cada emocion tiene SU hablar. Cuando la IA manda "TALK" y la emocion
 * activa es SORPRENDIDO, el sorprendido habla con "OH OH OH":
 *   - La boca "o" se ABRE en diamante y cierra a ritmo de habla
 *     (un gaspo de asombro por palabra, como exclamando cada cosa)
 *   - UNA FIBRA de CODAL parpadea los ojos EN PARALELO pero RARISIMO:
 *     esperas de 4-8s entre parpadeos (el asombrado mira fijo, casi
 *     no parpadea)
 *
 * Al terminar de hablar (otra emocion o STOP) modoHablar se apaga,
 * la fibra muere sola y el bucle principal vuelve a la sorpresa en
 * reposo (animarSorprendido).
 */
#ifndef HABLARSORPRENDIDO_H
#define HABLARSORPRENDIDO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// TALK (con sorpresa activa): dibuja la cara sorprendida hablando y
// lanza la fibra de parpadeo rarisimo
void iniciarHablarSorprendido();

// Una pasada de la boca "OH OH" (la llama el bucle principal)
void animarBocaSorprendida();

#endif // HABLARSORPRENDIDO_H
