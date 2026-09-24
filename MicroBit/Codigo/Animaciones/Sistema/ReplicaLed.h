/**
 * ReplicaLed.h - RETRANSMISION del display LED real
 *
 * Una fibra paralela lee el framebuffer actual del micro:bit
 * (uBit.display.image) y lo transmite por serial como:
 *
 *     LED:#########################\n   (25 chars: '#' encendido, '.' apagado)
 *
 * Asi la IA / la web pueden pintar EXACTAMENTE lo que el micro:bit
 * esta mostrando en este momento: una replica fiel, en vivo.
 */
#ifndef REPLICA_LED_H
#define REPLICA_LED_H

#include "MicroBit.h"

// Arranca la fibra de retransmision (se llama UNA vez en main)
void iniciarReplicaLed();

#endif // REPLICA_LED_H
