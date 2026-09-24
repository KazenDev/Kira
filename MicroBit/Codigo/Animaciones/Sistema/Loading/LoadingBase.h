/**
 * LoadingBase.h - Base COMPARTIDA de las animaciones de carga
 *
 * Lo que TODOS los patrones usan vive UNA sola vez aca:
 *   - decaerRastro(): el rastro de luz (decay exponencial, ~55 fps)
 *   - frameRastro():  un frame con rastro + chequeo serial
 *   - frameSerial():  un frame sin rastro, solo chequea serial
 *   - RING / SPIRAL:  geometrias del borde y la espiral
 *   - setAnillo():    enciende un anillo completo (0=centro, 1=3x3, 2=borde)
 *
 * Si quieres el rastro mas largo o mas corto, se cambia en UN solo lugar.
 */
#ifndef LOADINGBASE_H
#define LOADINGBASE_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Baja el brillo de TODOS los pixeles encendidos (rastro que se desvanece solo).
// porciento: cuanto queda por frame (78 = rastro corto, 88 = rastro largo).
void decaerRastro(int porciento = 78);

// Un frame con rastro: decae + si llego comando serial, avisa para abortar.
// Cada patron puede pedir SU rastro (ej: cometa usa 80 para cola larga).
bool frameRastro(int porciento = 78);

// Un frame SIN rastro (patrones que no decaen): solo chequea serial
bool frameSerial();

// Geometrias compartidas
extern const uint8_t RING[16][2];     // borde (16 posiciones)
extern const uint8_t SPIRAL[25][2];   // espiral: borde -> interior -> centro

// Enciende un anillo: 0 = centro, 1 = cuadrado 3x3, 2 = borde
void setAnillo(int radius, int brightness);

#endif // LOADINGBASE_H
