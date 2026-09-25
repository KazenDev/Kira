/**
 * Sorprendido.h - Modulo de la emocion SORPRENDIDO 😲
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * La sorpresa es OJOS ABIERTOS y BOCA DE "o":
 *   - Los ojos se DILATAN: miran fijo, bien abiertos y brillantes
 *   - La boca "o" se ABRE en un diamante grande (como "WOW") y se cierra
 *   - Parpadea RARISIMO: casi nunca (el asombrado mira fijo)
 *   - De vez en cuando un PULSO DE ASOMBRO: todo el brillo sube de
 *     golpe y vuelve (el susto)
 *
 * PATRON DE FRAME: animarSorprendido() muestra UN frame (~16 ms) y vuelve,
 * como las otras cinco ya migradas. El bucle de Principal.cpp la llama ~60
 * veces por segundo y el estado vive en el RELOJ, no en una cadena de
 * sleep(). Antes eran 725 ms de franja sorda.
 *
 * OJO: esta es la unica con CICLO VARIABLE. El pulso sale cada 2-3 ciclos, asi
 * que el ciclo dura 2630 ms cuando hay pulso y 1880 ms cuando no. Se mide con
 * dos divisores constantes (no con uno variable) para no introducir una
 * division real por frame. Ver Sorprendido.cpp.
 */
#ifndef SORPRENDIDO_H
#define SORPRENDIDO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra UN frame de la animacion de sorpresa (bucle en Principal.cpp)
void animarSorprendido();

// Para las TRANSICIONES (y CALLA): dibuja la cara en reposo y ancla el ciclo
// de la animacion en este instante.
void mostrarCaraSorprendido();

#endif // SORPRENDIDO_H
