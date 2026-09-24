/**
 * Loading.cpp - El DIRECTOR de las animaciones de carga
 *
 * Cada patron vive en su PROPIA CARPETA con su propio codigo:
 *   Loading/Cometa/Cometa.cpp, Loading/Espiral/Espiral.cpp, ...
 * Aqui solo se REGISTRAN en el array PATRONES y se elige al azar.
 * Los helpers compartidos (rastro, geometrias) viven en LoadingBase.
 *
 * PARA AGREGAR UN PATRON NUEVO (el #7, #8, ...):
 *   1. Crear Loading/MiPatron/MiPatron.h + MiPatron.cpp
 *   2. Incluir su header aca abajo
 *   3. Meter animarMiPatron() en PATRONES y subir NUM_LOADINGS
 */
#include "Loading.h"
#include "LoadingBase.h"
#include "Cometa/Cometa.h"
#include "Espiral/Espiral.h"
#include "Pulso/Pulso.h"
#include "Lluvia/Lluvia.h"
#include "Barra/Barra.h"
#include "../MicFft/MicFft.h"
#include "Carrera/Carrera.h"
#include "Flechas/Flechas.h"
#include "Arena/Arena.h"
#include "Onda/Onda.h"
#include "Luz/Luz.h"

// Registro de patrones (todos con la misma firma: void animarXxx(int))
static const int NUM_LOADINGS = 10;
typedef void (*PatronLoading)(int);
static const PatronLoading PATRONES[NUM_LOADINGS] = {
    animarCometa,    // 0: luz orbitando con rastro
    animarEspiral,   // 1: espiral adentro/afuera
    animarPulso,     // 2: borde que late
    animarLluvia,    // 3: gotas cayendo con rastro
    animarBarra,     // 4: luz cruzando ida y vuelta
    animarCarrera,   // 5: dos luces en carrera (persecucion) 🏃
    animarFlechas,   // 6: spinner de 4 flechas girando 🔄
    animarArena,     // 7: sandbox de arena interactivo 🏜️🌀
    animarOnda,      // 8: onda senoidal interactiva 🌊
    animarLuz        // 9: circulo de carga con sensor de luz 🕯️📊
};

// Muestra UN patron especifico (indice 0..NUM_LOADINGS-1)
void mostrarLoadingPatron(int idx, int cycles)
{
    uBit.display.setBrightness(180);
    uBit.display.image.clear();
    if (idx >= 0 && idx < NUM_LOADINGS) {
        // Solo Barra usa el microfono: al mostrar otro patron lo apagamos
        if (PATRONES[idx] != animarBarra) micFftDetener();
        PATRONES[idx](cycles);
    }
    uBit.display.setBrightness(90);
}

// Muestra un patron ALEATORIO (la micro:bit nunca carga igual dos veces)
void mostrarLoadingRandom(int cycles)
{
    mostrarLoadingPatron(uBit.random(NUM_LOADINGS), cycles);
}

// Muestra TODOS los patrones en secuencia (para previsualizar)
void mostrarLoadingTodos(int cycles)
{
    for (int i = 0; i < NUM_LOADINGS; i++)
        mostrarLoadingPatron(i, cycles);
}

// ---------------------------------------------------------------------------
// MODO BUCLE: el loading se repite hasta que llegue otra emocion o STOP
// ---------------------------------------------------------------------------
bool modoLoading = false;
static int patronActual = 0;   // el patron elegido al azar al iniciar

// LOAD0..LOAD9: activa el bucle con UN patron especifico. Si ya hay otro
// cargando, cambia a este (siempre hace switch, no se queda con el anterior).
void iniciarLoading(int idx)
{
    if (idx < 0 || idx >= NUM_LOADINGS) return;
    // Si cambiamos a un patron que NO usa el microfono, lo apagamos
    // (Barra se reactiva solo al entrar). Seguro siempre.
    if (PATRONES[idx] != animarBarra) micFftDetener();
    patronActual = idx;
    modoLoading = true;
    uBit.display.setBrightness(180);
    uBit.display.image.clear();   // arranque limpio (UNA vez, no por pasada)
}

// LOADING: bucle con un patron al azar. Si ya esta cargando, no reinicia.
void iniciarLoading()
{
    if (modoLoading) return;
    iniciarLoading(uBit.random(NUM_LOADINGS));
}

// Cualquier otra emocion (HAPPY, SAD, STOP, TALK...): lo apaga
// y de paso apaga el microfono si Barra lo estaba usando.
void detenerLoading()
{
    modoLoading = false;
    micFftDetener();
}

// Un ciclo del patron actual SIN limpiar la pantalla entre pasadas:
// asi la orbita del cometa (y otros) es CONTINUA, sin el "salto" de
// reinicio que se veia al volver a ejecutar el patron desde cero.
void mostrarLoadingBucle()
{
    if (patronActual >= 0 && patronActual < NUM_LOADINGS)
        PATRONES[patronActual](1);
}
