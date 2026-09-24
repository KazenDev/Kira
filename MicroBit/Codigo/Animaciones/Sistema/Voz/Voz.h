/**
 * Voz.h - DIRECTOR DE LOS AROS QUE REACCIONAN A LA VOZ 🗣️🌀
 *
 * Como el orbe de los modos de voz de los agentes de IA: aros centrados
 * que crecen con la fuerza de tu voz y "respiran" en silencio.
 * Alimentados por el nivel REAL del microfono (modulo MicFft, 11kHz).
 *
 * CADA VARIANTE vive en su propia carpeta con su codigo por separado:
 *
 *   Voz/
 *   ├── Voz.h / Voz.cpp           <- el DIRECTOR (selector random)
 *   ├── Aro/    0 🗣️  aro que crece y respira (el clasico)
 *   ├── Sonar/  1 📡  ondas que viajan con cada palabra (ping de radar)
 *   ├── Orbe/   2 ⚡   mancha que tiembla/hierve con la voz
 *   ├── Anillos/ 3 🪐  dos anillos: interno rapido + externo lento
 *   └── Cometa/ 4 ☄️  aro con rastro que persigue el nivel
 *
 * La IA manda "VOZ" y la micro:bit elige UNA AL AZAR. "VOZ0".."VOZ4"
 * fuerzan una variante especifica (para probar cada una).
 *
 * Modo bucle: igual que Loading — iniciarVoz() lo enciende (prende el
 * mic de verdad via micFftIniciar) y el bucle principal (Principal.cpp)
 * llama mostrarVozBucle() una vez por frame (60fps) hasta que llegue
 * otra emocion o STOP. detenerVoz() apaga el microfono DE VERDAD
 * (micFftDetener: stream ADC + corriente del MEMS).
 */
#ifndef VOZ_H
#define VOZ_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Estamos en modo aro-de-voz? (lo mantiene el bucle principal)
extern bool modoVoz;

// "VOZ0".."VOZ4": enciende ESA variante especifica + prende el microfono
void iniciarVoz(int idx);

// "VOZ": enciende una variante al azar (nunca igual dos veces)
void iniciarVoz();

// Lo apaga y apaga el microfono DE VERDAD (seguro siempre)
void detenerVoz();

// Un frame de la variante activa a 60fps (lo llama el bucle principal)
void mostrarVozBucle();

// Helper compartido: nivel de voz suavizado 0..4.5 (ataque rapido,
// release lento). Lo usan todas las variantes para no duplicar logica.
float vozNivelSuave();

#endif // VOZ_H
