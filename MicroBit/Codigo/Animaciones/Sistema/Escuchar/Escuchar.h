/**
 * Escuchar.h - MODO ESCUCHA GPT 🎙️🌀
 *
 * Como el modo de voz de los agentes de IA: el micro:bit "escucha" en
 * vivo y corta SOLO cuando dejas de hablar (VAD real en el firmware).
 *
 * Flujo:
 *   ESCUCHAR -> escucharIniciar():
 *     1. Prende el aro de voz (iniciarVoz -> mic FFT + visual)
 *     2. Arranca el stream de audio crudo (grabarIniciar -> AUDIO:START)
 *        La PC recibe los samples EN VIVO mientras hablas.
 *   El bucle principal llama escucharVad() una vez por frame (60fps):
 *     - detecta cuando empezaste a hablar (nivel FFT > umbral sostenido)
 *     - cuando hay silencio > 1.5s despues de haber hablado -> corta solo
 *     - timeouts: nadie hablo en 7s / maximo 15s
 *   Al cortar (escucharDetener): AUDIO:END + apaga aro y mic de verdad.
 *
 * El server calcula el nivel RMS de los samples que recibe y lo manda
 * por SSE a la web -> el ORBE de la pantalla reacciona a tu voz igual
 * que el aro fisico del micro:bit.
 */
#ifndef ESCUCHAR_H
#define ESCUCHAR_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Estamos escuchando ahora? (lo mantiene el bucle principal)
extern bool modoEscuchar;

// "ESCUCHAR": prende el aro + arranca el stream de audio en vivo
void escucharIniciar();

// Corta la escucha: AUDIO:END + apaga aro y microfono DE VERDAD
void escucharDetener();

// VAD: un frame de deteccion de voz/silencio (lo llama el bucle)
void escucharVad();

// Un frame del ARO de la escucha a 60fps (reacciona a tu voz, respira
// en silencio). Usa el nivel del sink de grabacion, no el FFT.
void escucharFrame();

#endif // ESCUCHAR_H
