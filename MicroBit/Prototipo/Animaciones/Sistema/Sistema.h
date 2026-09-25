/**
 * Sistema.h - STUB MINIMO para el prototipo del bench visual
 *
 * El Codigo/ real de Alegria.cpp solo necesita UNA cosa de este header:
 * `revisarSerial()`. Este stub se lo da sin arrastrar todo Sistema.cpp
 * (que define el despachador completo y el demonio automatico), para que el
 * prototipo pueda CONTROLAR cuando "llega un comando" y medir la latencia.
 *
 * NO es el header de la firmware. Es del prototipo.
 */
#ifndef SISTEMA_H
#define SISTEMA_H

#include "MicroBit.h"

// La emocion activa. El prototipo la usa para saber que dibuja.
enum EmocionActual {
    EM_ALEGRIA, EM_TRISTE, EM_ENOJADO, EM_SORPRENDIDO,
    EM_NEUTRAL, EM_FASTIDIO, EM_MIEDO, EM_CANSADO
};
extern EmocionActual emocionActual;

// El unico simbolo que Alegria.cpp usa de este modulo.
bool revisarSerial();

#endif // SISTEMA_H
