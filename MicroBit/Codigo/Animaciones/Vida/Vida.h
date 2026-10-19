/**
 * Vida.h - El PEGAMENTO entre el motor de vida y la placa
 *
 *   Vida/Nucleo/   el motor puro (sin CODAL): ojos, habla, gestos,
 *                  reacciones. Se testea en el PC (Bench/test_vida.cpp).
 *   Vida/Vida.cpp  esto: lee los sentidos de la placa (acelerometro, logo
 *                  tactil, sacudida), corre UN frame del motor, vuelca el
 *                  lienzo al display (solo lo que cambio) y convierte los
 *                  eventos en sonidos.
 *
 * Reemplaza a las 8 animaciones de reposo y a las 8 bocas de TALK: el bucle
 * principal ya no elige que funcion llamar segun la emocion, llama a
 * vidaFrame() y la personalidad activa decide.
 */
#ifndef VIDA_H
#define VIDA_H

#include "MicroBit.h"
#include "../Sistema/Sistema.h"

extern MicroBit uBit;

// true mientras la IA habla (TALK hasta CALLA u otro comando)
extern bool modoHablar;

void vidaIniciar();
// Pone la emocion (despues de la transicion). conEntrada = gesto + sonido
// de llegada. Si estaba hablando, sigue hablando con la boca nueva.
void vidaPonerEmocion(EmocionActual e, bool conEntrada);
// UN frame (~16 ms): sentidos + motor + display + sonidos. Duerme el resto.
void vidaFrame();

// TALK / CALLA
void iniciarHablar();
void detenerHablar();

// Para las transiciones: dibuja como se ve la emocion en reposo, sin tocar
// el estado de la cara que esta viva.
void vidaDibujarCara(EmocionActual e);

// Comandos de prueba y ajuste
void vidaActividad();
void vidaReaccion(int r);         // 1..7 (ver Reaccion en Cara.h)
void vidaGesto(int idx);
void vidaSentidos(bool activos);
void vidaBrillo(int brillo);      // brillo global 0..255
int  vidaBrilloGlobal();
void vidaClima(int clima);        // 0 normal, 1 calor, 2 frio, -1 automatico
void vidaPicar();                 // como un toque corto de B (PICAR)
void vidaLuz(bool activa);        // LUZ:ON / LUZ:OFF (dormirse si la tapan)
void vidaOido(bool activo);       // OIDO:ON / OIDO:OFF (notar gente por el ruido)
void vidaOidoPausar();            // soltar el microfono para otro modo

// Lo que la IA le pide mostrar (ICONO:, TEXTO:, FLECHA:, TIMER:)
void vidaIcono(const char *nombre, uint32_t ms);
void vidaTexto(const char *texto);
void vidaFlecha(int grados, uint32_t ms);
void vidaTimer(uint32_t segundos);
void vidaDiagnostico();           // VIDA:?

// Boton B fuera de la escucha: toque corto = picarla, mantener = minijuego.
// La llama el bucle principal con el estado del boton en cada frame.
void vidaBotonB(bool b);

// Minijuegos (mantener B o JUEGO:<n>). Mientras hay uno, A y B son del juego
// y no abren el microfono.
bool vidaJugando();
void vidaJugar(int cual);         // 0 = el siguiente de la rotacion
void vidaTerminarJuego();
void vidaBotonesJuego(bool pulsoA, bool pulsoB);

#endif // VIDA_H
