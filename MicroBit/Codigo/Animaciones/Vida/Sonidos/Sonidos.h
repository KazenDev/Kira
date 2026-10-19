/**
 * Sonidos.h - El PARLANTE de Kira (pegamento con CODAL)
 *
 * Los sonidos los decide el motor (eventos de la cara) y los datos estan en
 * Nucleo/Catalogo. Esto solo los toca, con tres reglas:
 *
 *   1. NUNCA bloquear la cara: SoundExpressions::play() espera si ya hay un
 *      sonido sonando (lock.wait() en SoundEmojiSynthesizer.cpp:117). Por
 *      eso se tocan desde UNA FIBRA propia; el bucle de la cara solo deja el
 *      pedido y sigue.
 *   2. Callarse mientras el microfono escucha (si no, Kira se graba a si
 *      misma) y mientras corre el metronomo (el parlante es suyo).
 *   3. Los gestos sueltos suenan espaciados (6 s minimo): que sea gracioso,
 *      no un ruido constante en la feria.
 *
 * Comandos: SONIDO:ON / SONIDO:OFF, VOLUMEN:<0-255>, SONAR:<n> (prueba).
 */
#ifndef SONIDOS_H
#define SONIDOS_H

#include <stdint.h>

void sonidosIniciar();
void sonidoTocar(uint8_t id);     // emociones y reacciones
void sonidoGesto(uint8_t id);     // gestos sueltos (espaciados)
void sonidoCallar();              // corta lo que este sonando
void sonidoSilenciar(bool silencio);
bool sonidoSilenciado();
void sonidoVolumen(int volumen);  // 0..255
// ¿Esta sonando algo (o por sonar)? El oido no escucha mientras tanto.
bool sonidoSonando();

#endif // SONIDOS_H
