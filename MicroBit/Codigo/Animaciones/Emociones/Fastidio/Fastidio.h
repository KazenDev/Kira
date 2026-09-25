/**
 * Fastidio.h - Modulo de la emocion FASTIDIO 😤 (en reposo)
 *
 * La cara "ya me canse de esto":
 *   - Respira FASTIDIADA (el suspiro del que aguanta el mal humor)
 *   - Los ojos DESPLAZADOS: mirando de reojo, NO espejados a proposito
 *   - El borde interno de las cejas TIEMBLA (la irritacion que sube)
 *   - Parpadeo DURO (el que intenta mantener la calma)
 *   - "Tsk": el centro del labio pulsa
 *
 * PATRON DE FRAME: animarFastidio() muestra UN frame (~16 ms) y vuelve, como
 * las otras siete ya migradas. El bucle de Principal.cpp la llama ~60 veces
 * por segundo y el estado vive en el RELOJ, no en una cadena de sleep().
 * Antes eran 650 ms de franja sorda (la respiracion son 700 ms sin un solo
 * revisarSerial(), y es la mayor proporcion del ciclo). Ver Fastidio.cpp.
 *
 * La asimetria de los ojos es lo que hace que se lea fastidio y no "mirando a
 * dos lados": el desprecio se define justamente por asimetria, y el "mirar de
 * reojo" (echar los ojos para atras) es el gesto clasico del menosprecio.
 */
#ifndef FASTIDIO_H
#define FASTIDIO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Muestra UN frame de la animacion de fastidio (bucle en Principal.cpp)
void animarFastidio();

// Para las TRANSICIONES (y CALLA): dibuja la cara en reposo y ancla el ciclo
// de la animacion en este instante.
void mostrarCaraFastidio();

#endif // FASTIDIO_H
