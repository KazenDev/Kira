/**
 * Enojado.h - Modulo de la emocion ENOJADO 😠
 *
 * Una emocion = una carpeta con su animacion compleja y unica.
 * El enojo es TERSO y BRUSCO, todo lo contrario a la tristeza:
 *   - Respira AGITADA: el brillo sube/baja RAPIDO (mas que la alegria)
 *   - Cejas que se FRUNCEN en oleadas: se tensan (se engrosan hacia
 *     el centro) y se relajan, cada vez mas seguidas
 *   - Parpadeo BRUSCO: se cierran rapido, cerrados un instante
 *   - La mandibula APRIETA: los dientes apretados (boca seria)
 *
 * PATRON DE FRAME: animarEnojado() muestra UN frame (~16 ms) y vuelve, como
 * las otras ya migradas (Alegria, Triste, Cansado, Miedo). El bucle de
 * Principal.cpp la llama ~60 veces por segundo y el estado vive en el RELOJ,
 * no en una cadena de sleep(). Antes eran 912 ms de franja sorda (las cejas
 * son 1.110 ms sin un solo revisarSerial()), la mayor de las ocho. Ver
 * Enojado.cpp.
 */
#ifndef ENOJADO_H
#define ENOJADO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra UN frame de la animacion de enojo (bucle en Principal.cpp)
void animarEnojado();

// Para las TRANSICIONES (y CALLA): dibuja la cara en reposo y ancla el ciclo
// de la animacion en este instante.
void mostrarCaraEnojado();

#endif // ENOJADO_H
