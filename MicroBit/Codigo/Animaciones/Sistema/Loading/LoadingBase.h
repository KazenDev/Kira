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

// El LOTE: cuanto tiempo puede retener el bucle principal un patron de carga.
//
// POR QUE EXISTE. Los 10 patrones corren su ciclo COMPLETO adentro de una
// llamada (hasta 400 frames = 6,4 s). Cada frame chequea el serial, asi que la
// IA nunca espera mas de un frame. PERO el bucle principal queda adentro de
// esa llamada, y lo unico que hace el principal ademas de renderizar es
// atenderBotonesEscucha(): los botones A/B de la escucha manual quedaban
// MUERTOS durante los 6,4 s del loading.
//
// O sea: el puerto estaba bien, los botones no. Y no se arregla achicando los
// 400 frames de cada patron (eso toca 10 archivos y los contadores internos de
// cada uno), sino aca: frameRastro/frameSerial devuelven true cuando el lote
// se agota, y el patron hace su `return` de siempre. El corte es
// INVISIBLE porque el estado de todos los patrones vive en `static`, asi que
// el siguiente frame del lote sigue exactamente donde quedo.
//
// El costo de acortar el lote es despertar el principal mas seguido, que es
// barato (un readUntil y una lectura de botones). El costo de NO hacerlo es que
// A y B no responden mientras la IA piensa.
#define LOTE_MS 250

// LOTES EXENTOS: para golpes dramaticos de UN SOLO disparo.
//
// El lote resuelve un problema de los patrones CONTINUOS (sus 400 frames se
// corren enteros en una llamada). Pero un golpe de un disparo no se puede
// partir a la mitad sin arruinarlo, y el corte no perdona: ver rayo() en
// Lluvia.cpp, que con el corte normal no llegaba a mostrar el trueno entero
// NUNCA y se comia el 120 ms de puerto sordo de paso.
//
// Con esto en true, frameRastro/frameSerial dejan de cortar por lote pero
// SIGUEN cortando por comando serial: el puerto no queda sordo ni un frame.
// El que exenta tiene que atender los botones el mismo (ver frameRayo() en
// Lluvia.cpp), porque si no A/B quedan muertos durante el golpe.
extern bool loteExento;

// Arranca un lote nuevo. La llama mostrarLoadingBucle() en cada pasada.
void loteIniciar();

// Baja el brillo de TODOS los pixeles encendidos (rastro que se desvanece solo).
// porciento: cuanto queda por frame (78 = rastro corto, 88 = rastro largo).
void decaerRastro(int porciento = 78);

// Un frame con rastro: decae + si llego comando serial, avisa para abortar.
// Cada patron puede pedir SU rastro (ej: cometa usa 80 para cola larga).
// Tambien devuelve true si se agoto el lote (ver LOTE_MS).
bool frameRastro(int porciento = 78);

// Un frame SIN rastro (patrones que no decaen): solo chequea serial
// (y el lote). Devuelve true si llego algo o si el lote se agoto.
bool frameSerial();

// Geometrias compartidas
extern const uint8_t RING[16][2];     // borde (16 posiciones)
extern const uint8_t SPIRAL[25][2];   // espiral: borde -> interior -> centro

// Enciende un anillo: 0 = centro, 1 = cuadrado 3x3, 2 = borde
void setAnillo(int radius, int brightness);

#endif // LOADINGBASE_H
