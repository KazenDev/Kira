/**
 * Juegos.h - MINIJUEGOS de 10-30 s (mantener B para jugar)
 *
 * Pensados para la feria: arrancan al instante, se entienden SIN
 * instrucciones mirando la matriz a un metro, y terminan solos en una cara
 * de Kira (gana, pierde, se sorprende). Se rotan: cada vez que mantenes B
 * sale el siguiente.
 *
 *   DADO      "rueda" con desaceleracion y cae en una cara de 1 a 6.
 *             B o sacudirla = tirar de nuevo.
 *   PPT       piedra-papel-tijera contra Kira: la matriz va mostrando las
 *             tres opciones y apretas A o B cuando sale la tuya. Despues la
 *             cuenta "pie-dra, pa-pel..." y Kira muestra la suya.
 *   REFLEJOS  Kira espera concentrada... ¡FLASH! y apretas lo mas rapido
 *             que puedas. Si apretas antes: "¡tramposo!". El tiempo sale en
 *             estrellas (fila de abajo) y en la cara.
 *
 * Puro como el resto del nucleo: el pegamento le pasa los botones y la
 * sacudida, y recibe el lienzo, un sonido y un aviso para el serial.
 */
#ifndef VIDA_JUEGOS_H
#define VIDA_JUEGOS_H

#include <stdint.h>
#include "Azar.h"
#include "Lienzo.h"

enum Juego
{
    J_NINGUNO = 0,
    J_DADO,
    J_PPT,
    J_REFLEJOS,
    NUM_JUEGOS
};

struct EntradaJuego
{
    bool pulsoA;      // flanco de A (se apreto en este frame)
    bool pulsoB;      // flanco de B
    bool toque;       // flanco del logo
    bool sacudida;    // CODAL detecta sacudida ahora
};

struct EstadoJuego
{
    uint8_t  juego;
    uint8_t  fase;
    uint32_t faseDesde;
    Azar     azar;

    int      valor;          // dado: la cara | ppt: lo de Kira
    int      jugador;        // ppt: lo del jugador
    int      resultado;      // ppt: 1 gana el jugador, 0 empate, -1 gana Kira
    uint32_t espera;         // reflejos: cuanto espera antes del flash
    uint32_t reaccionMs;     // reflejos: lo que tardo el jugador
    uint32_t proxCambio;     // dado/ppt: proximo cambio de cara
    uint32_t paso;           // dado: intervalo actual (desacelera)

    // Salidas del frame
    uint8_t  sonido;         // SonidoId a tocar (0 = nada)
    uint8_t  sonidoInicio;   // el jingle de arranque, para el primer frame
    bool     hayAviso;
    char     aviso[40];      // "JUEGO:DADO:4", etc.
};

void juegoIniciar(EstadoJuego &j, uint8_t juego, uint32_t t, uint32_t semilla);
// UN frame del juego. Devuelve false cuando el juego termino.
bool juegoFrame(EstadoJuego &j, uint32_t t, const EntradaJuego &e, Lienzo &out);
bool juegoActivo(const EstadoJuego &j);
void juegoTerminar(EstadoJuego &j);

#endif // VIDA_JUEGOS_H
