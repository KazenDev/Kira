/**
 * MicFft.h - ANALIZADOR DE ESPECTRO REAL 🎚️⚡
 *
 * Se conecta al pipeline de audio de CODAL (uBit.audio.splitter) como un
 * DataSink mas. Recibe los samples del microfono (8-bit signed, 11kHz),
 * acumula 128, aplica ventana Hann, corre un FFT radix-2 de 128 puntos y
 * calcula 5 bandas de frecuencia reales (bajos -> agudos).
 *
 * Uso:
 *   micFftIniciar();          // conectar el sink + PRENDER el microfono
 *   const float *b = micFftBandas();  // 5 valores 0..4.5
 *   micFftDetener();          // apagar TODO (stream ADC + corriente del mic)
 *
 * El microfono NO queda encendido para siempre: al salir de la animacion
 * que lo usa (o al boot) se llama micFftDetener(), que corta el stream
 * del ADC (dataWanted NOT_WANTED) y la corriente del mic fisico
 * (runmic=0). micFftIniciar() lo reactiva completo si se vuelve a pedir.
 */
#ifndef MICFFT_H
#define MICFFT_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Conecta el sink FFT al splitter de audio + PRENDE el microfono
// (idempotente; reactiva todo si estaba apagado con micFftDetener)
void micFftIniciar();

// Apaga el microfono DE VERDAD (stream ADC + corriente). Seguro siempre.
void micFftDetener();

// Las 5 bandas de frecuencia, normalizadas 0..4.5 (listas para pintar)
const float *micFftBandas();

// FFTs procesados desde que inicio (debug: confirma que el stream fluye)
int micFftConteo();

// ¿El microfono + FFT estan activos ahora? Lo consulta el sensor puntual
// para no apagar una animacion que ya este usando el microfono.
bool micFftActivo();

#endif // MICFFT_H
