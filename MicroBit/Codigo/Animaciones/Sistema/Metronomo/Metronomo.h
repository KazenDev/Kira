/**
 * Metronomo.h - METRONOMO REAL para musicos 🎵⏱️
 *
 * El micro:bit marca el pulso como el metronomo de Maelzel (1815, el
 * que uso Beethoven): un PENDULO visual que vaivéa una vez por pulso
 * (la fila del medio barre de lado a lado) + un CLICK por el parlante
 * (agudo en el acento del compas, grave en los demas pulsos).
 *
 * Funciona DESCONECTADO de la PC (botones) o controlado por la IA:
 *
 *   A         = tempo -5 BPM        (suelto, sin PC)
 *   B         = tempo +5 BPM        (suelto, sin PC)
 *   A + B     = detener (vuelve a la alegria)
 *
 *   METRO:<bpm>:<acento>   -> arranca o cambia el tempo (METRO:120:4)
 *   METRO:STOP             -> corta y vuelve a la alegria
 *   Respuesta: ACK:<comando> (la manda el despachador, Sistema.cpp)
 *
 * Ciencia de la casa: el timing usa el RELOJ ABSOLUTO del sistema
 * (uBit.systemTime, cuarzo de 64 MHz del nRF52833). El proximo pulso
 * se ancla SIEMPRE al anterior + intervalo, asi el error de cada frame
 * NO se acumula: es un oscilador isocrono de verdad (jitter ~1-2 ms,
 * imperceptible para un musico a estos BPM).
 */
#ifndef METRONOMO_H
#define METRONOMO_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// ¿Estamos en modo metronomo? (lo mantiene el bucle principal)
extern bool modoMetro;

// "METRO:<bpm>:<acento>": arranca (o cambia el tempo EN CALIENTE, sin
// perder el pulso). bpm: 20..250 | acento: 0 = sin acento (pulsos
// parejos), 1..5 = acento cada N pulsos (4 = compas de cuatro).
void metroIniciar(int bpm, int acento);

// Corta el metronomo: mata la fibra de pulsos y calla un click en vuelo.
void metroDetener();

// Un frame del pendulo a 60fps (lo llama el bucle principal). Tambien
// atiende los BOTONES A/B (ajuste de tempo suelto, sin PC).
void metroFrame();

#endif // METRONOMO_H
