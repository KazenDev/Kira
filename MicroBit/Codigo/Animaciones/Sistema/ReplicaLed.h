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
 *
 * SE PUEDE CALLAR con el comando "REPLICA:OFF" (y volver con "REPLICA:ON").
 *
 * POR QUE EXISTE ESE COMANDO. La replica comparte el UART con los comandos,
 * y manda ~20 fps mientras la imagen se mueve. Con el puerto asi, la
 * latencia de un comando no se puede medir: el piso es el propio trafico de
 * la replica (medido: 134 ms de media con Lluvia, contra 20 ms de mediana
 * en reposo). No es un defecto de la replica, es que dos cosas comparten un
 * canal y una de las dos habla constantly.
 *
 * El comando sirve para dos cosas:
 *   - MEDIR. Con la replica callada el puerto queda limpio y la latencia
 *     medida es real. Es lo que hace MicroBit/prueba_rayo.py, que sin esto
 *     no puede distinguir un puerto sordo de 120 ms de su propio ruido.
 *   - AHORRAR ANCHO DE BANDA cuando la app no esta mirando la replica (por
 *     ejemplo durante audio), en vez de dejarlo siempre emitiendo.
 */
#ifndef REPLICA_LED_H
#define REPLICA_LED_H

#include "MicroBit.h"

// Arranca la fibra de retransmision (se llama UNA vez en main)
void iniciarReplicaLed();

// REPLICA:OFF / REPLICA:ON. Callar la replica no la mata: la fibra sigue
// viva y solo deja de transmitir, asi que REPLICA:ON la reactiva al instante
// sin perder nada (no hay estado que recuperar, se lee el framebuffer).
void replicaLedSilenciar(bool silenciar);

#endif // REPLICA_LED_H
