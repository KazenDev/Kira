/**
 * Pantalla.h - Lo que Kira MUESTRA en sus LEDs cuando la IA se lo pide
 *
 * Son "capas" que tapan la cara un rato y despues la devuelven:
 *
 *   ICONO    un dibujo (corazon, sol, lluvia, estrella, luna...) que aparece
 *            con un fundido y late suave. Ej: mientras dice el pronostico.
 *   TEXTO    una palabra que pasa de derecha a izquierda (sin bloquear: la
 *            cara y el puerto siguen vivos).
 *   FLECHA   una flecha que apunta EN VIVO hacia un rumbo (el norte, una
 *            ciudad) aunque la giren: usa la brujula de la placa.
 *   TIMER    un temporizador: la cara sigue viva y cada 15 s muestra un
 *            instante cuanto falta; los ultimos 5 s cuenta 5-4-3-2-1 y al
 *            final suena y avisa (para que Kira lo anuncie en voz alta).
 *
 * Puro como el resto del nucleo: las letras se las pasa el pegamento (la
 * fuente de CODAL) y el rumbo de la brujula tambien.
 */
#ifndef VIDA_PANTALLA_H
#define VIDA_PANTALLA_H

#include <stdint.h>
#include "Lienzo.h"

enum IconoKira
{
    IC_CORAZON = 0, IC_SOL, IC_NUBE, IC_LLUVIA, IC_ESTRELLA, IC_LUNA,
    IC_SI, IC_NO, IC_CASA, IC_MUSICA, IC_RAYO, IC_PREGUNTA, IC_TERMOMETRO,
    NUM_ICONOS
};

enum CapaPantalla { CAPA_NADA = 0, CAPA_ICONO, CAPA_TEXTO, CAPA_FLECHA, CAPA_ALARMA };

// Eventos (bits) que el pegamento traduce a sonido / serial
enum EventoPantalla { EVP_TIMER_FIN = 1, EVP_SIN_BRUJULA = 2 };

// La forma de una letra: 5 filas, bit (0x10 >> columna) = encendido
typedef const uint8_t *(*FuenteGlifo)(char c);

struct EstadoPantalla
{
    uint8_t  capa;
    uint32_t inicio, fin;
    uint8_t  icono;
    char     texto[34];
    float    rumboObjetivo;     // FLECHA: grados desde el norte
    bool     timerActivo;
    uint32_t timerInicio, timerFin;
    uint8_t  eventos;
};

void pantallaIniciar(EstadoPantalla &p);
int  pantallaIconoPorNombre(const char *nombre);   // -1 si no existe
void pantallaIcono(EstadoPantalla &p, int icono, uint32_t t, uint32_t ms);
void pantallaTexto(EstadoPantalla &p, const char *texto, uint32_t t);
void pantallaFlecha(EstadoPantalla &p, float rumbo, uint32_t t, uint32_t ms);
void pantallaTimer(EstadoPantalla &p, uint32_t segundos, uint32_t t);
void pantallaCancelar(EstadoPantalla &p);
bool pantallaOcupada(const EstadoPantalla &p, uint32_t t);

// UN frame. rumboKira < 0 = brujula sin calibrar. Devuelve true si la capa
// tapa la cara en este frame (y dibujo en 'out').
bool pantallaFrame(EstadoPantalla &p, uint32_t t, float rumboKira, FuenteGlifo glifo,
                   Lienzo &out);

#endif // VIDA_PANTALLA_H
