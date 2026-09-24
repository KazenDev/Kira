/**
 * Escuchar.h - ESCUCHA MANUAL DEL MICRO:BIT 🎙️🔘
 *
 * El microfono queda abierto desde que se pulsa A hasta que se vuelve a
 * pulsar A. No se corta por silencio ni por VAD:
 *
 *   A (flanco) -> abre el microfono y muestra el aro reactivo al volumen.
 *   A (flanco) -> cierra la captura y envia AUDIO:END (el backend transcribe).
 *   B (flanco) -> cancela y envia AUDIO:CANCEL (el backend descarta el audio).
 *
 * El comando de la PC ESCUCHAR usa exactamente la misma ruta. El VAD queda
 * como API de compatibilidad, pero ya no decide cuando termina un turno.
 */
#ifndef ESCUCHAR_H
#define ESCUCHAR_H

#include "MicroBit.h"

extern MicroBit uBit;

// Estamos escuchando ahora? (lo mantiene el bucle principal y los botones)
extern bool modoEscuchar;

// Prepara la espera manual: muestra el aro, pero todavía no prende el micro.
void escucharArmar();

// ¿El microfono está transmitiendo samples ahora?
bool escucharMicroActivo();

// Abre el micro y arranca la captura manual.
void escucharIniciar();

// Cierra la captura y marca AUDIO:END para enviar el audio al backend.
void escucharDetener();

// Cierra la captura y marca AUDIO:CANCEL para descartarla.
void escucharCancelar();

// Compatibility no-op: el corte manual reemplazo al VAD automatico.
void escucharVad();

// Un frame del aro de escucha a 60fps, usando el nivel real del microfono.
void escucharFrame();

#endif // ESCUCHAR_H
