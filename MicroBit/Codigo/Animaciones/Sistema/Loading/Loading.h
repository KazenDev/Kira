/**
 * Loading.h - ANIMACIONES DE CARGA (patrones de luz con rastro real)
 *
 * CADA patron vive en su propia carpeta con su codigo por separado:
 *
 *   Loading/
 *   ├── Loading.h / Loading.cpp        <- el DIRECTOR (selector random)
 *   ├── LoadingBase.h / LoadingBase.cpp<- lo COMPARTIDO (rastro, geometrias)
 *   ├── Cometa/   0 ☄️   ├── Espiral/ 1 🌀   ├── Pulso/ 2 💓
 *   ├── Lluvia/   3 🌧️   ├── Barra/   4 📊   ├── Carrera/ 5 🏎️
 *   ├── Flechas/  6 🔄 (spinner)    ├── Arena/  7 ⏳ (reloj de arena)
 *   ├── Onda/     8 🌊 (senoidal)  └── Luz/    9 🕯️📊 (carga con luz)
 *
 * La IA manda "LOADING" y la micro:bit elige UNO AL AZAR (nunca carga
 * igual dos veces). "LOADALL" muestra todos en secuencia (preview).
 */
#ifndef LOADING_H
#define LOADING_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// --- Modo BUCLE: el loading se repite hasta que llegue otra emocion ---
// Igual que TALK: iniciarLoading() lo enciende, detenerLoading() lo apaga,
// y el bucle principal (Principal.cpp) repite el patron elegido hasta
// entonces. Asi la IA manda LOADING mientras "piensa" y el aro/patron
// sigue girando hasta que llegue la respuesta real (o STOP).
extern bool modoLoading;          // definido en Loading.cpp
void iniciarLoading(int idx);     // LOAD0..LOAD9: bucle continuo con ESE patron
void iniciarLoading();            // LOADING: bucle con un patron al azar
void detenerLoading();            // lo apaga (vuelve a la emocion activa)
void mostrarLoadingBucle();       // un ciclo del patron actual (lo llama el bucle)

// --- Modo "de un tiro" (finito, para pruebas) ---
void mostrarLoadingRandom(int cycles = 3);
void mostrarLoadingPatron(int idx, int cycles = 2);
void mostrarLoadingTodos(int cycles = 1);

#endif // LOADING_H
