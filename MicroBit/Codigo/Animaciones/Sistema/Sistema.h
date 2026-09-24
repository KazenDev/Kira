/**
 * Sistema.h - Modulo de SISTEMA
 *
 * Cosas de "infraestructura": el receptor serial de emociones, la demo
 * automatica y el estado de la emocion activa. Las ANIMACIONES DE CARGA
 * estan en su propia carpeta: Animaciones/Sistema/Loading/ (Loading.h).
 */
#ifndef SISTEMA_H
#define SISTEMA_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// --- Estado global: emocion actualmente activa ---
// Lo mantiene procesarComando y lo usa el bucle principal (Principal.cpp)
// para saber QUE renderizar en cada pasada. Asi las emociones estaticas
// (triste, enojado...) NO se pisan con la alegria al siguiente ciclo.
enum EmocionActual { EM_ALEGRIA, EM_TRISTE, EM_ENOJADO, EM_SORPRENDIDO, EM_NEUTRAL, EM_FASTIDIO, EM_MIEDO, EM_CANSADO };
extern EmocionActual emocionActual;   // definido en Sistema.cpp

// --- Comunicacion con la IA ---
// Comandos: "HAPPY", "SAD", "ANGRY", "SURPRISED", "NEUTRAL",
// "FASTIDIO" (o "ANNOYED"), "MIEDO" (o "SCARED"), "CANSADO" (o "TIRED"),
// "TALK" (habla hasta que llegue otra emocion o STOP), "LOADING"
// (patron de carga ALEATORIO), "LOADALL" (preview de todos los patrones),
// "BLINK", "TEST" (demo), "TRANS" (transicion aleatoria de prueba),
// "TRANSALL" (preview de las 3 transiciones), "CANCELAR" (descarta la
// escucha manual) y "STOP" (vuelve a la alegria en reposo). La escucha
// manual se controla con A (abrir/enviar) y B (cancelar).
// SENSORES (tool calling de la IA, lecturas puras que no tocan la cara):
//   SENSOR:TEMP  -> TEMP:24      (grados Celsius)
//   SENSOR:LUZ   -> LUZ:120      (0..255)
//   SENSOR:BOTON -> BOTON:1:0    (A:B, 1=presionado)
//   SENSOR:ACCEL -> ACCEL:x:y:z:pitch:roll
//   SENSOR:MIC   -> MIC:nivel:b0:b1:b2:b3:b4:ventanas
//   SENSOR:BAT   -> BAT:bateria_mV:vin_mV:fuente
void procesarComando(ManagedString cmd);  // recibe "HAPPY", "SAD", etc.
// Lee el serial: si llego un comando completo, lo procesa y devuelve true.
// Lo llaman el bucle principal Y las animaciones largas (para que se
// interrumpan solas cuando la IA manda algo -> respuesta inmediata).
bool revisarSerial();
void demoAutomatica();                    // muestra todas las emociones

#endif // SISTEMA_H
