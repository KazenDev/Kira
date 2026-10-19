/**
 * Ojos.h - OJOS PROCEDURALES: parpadeo natural + mirada con saltos (sacadas)
 *
 * Los ojos son lo primero que mira una persona en una cara, y lo que mas
 * delata a una animacion en bucle. Aca nada esta guionado: cada parpadeo y
 * cada mirada se SORTEA con estadistica de ojos reales (ver MICROBIT.md,
 * seccion MOTOR DE VIDA, con las fuentes):
 *
 *   PARPADEO   intervalo log-normal (CV ~0,7). Cierre rapido, apertura ~2x
 *              mas lenta. Algunos incompletos, algunos dobles. Al hablar se
 *              parpadea mas, y se parpadea al cambiar mucho la mirada.
 *   MIRADA     saltos cortos y frecuentes, largos y raros (exponencial, como
 *              en "Eyes Alive", Lee 2002). Arriba/abajo/izq/der el doble de
 *              probables que las diagonales. Nunca dos veces al mismo lugar.
 *              Entre saltos, micro-sacadas de menos de un LED.
 *   RESORTE    la mirada viaja con un resorte: se pasa un poquito y vuelve.
 *
 * Todo es funcion del tiempo que se pasa; el modulo no conoce CODAL.
 */
#ifndef VIDA_OJOS_H
#define VIDA_OJOS_H

#include <stdint.h>
#include "Azar.h"
#include "Ruido.h"

struct ParamOjos
{
    float parpadeoMs;      // intervalo medio entre parpadeos
    float parpadeoCv;      // variacion del intervalo
    float cierreMs;        // fases de UN parpadeo
    float sostenMs;
    float aperturaMs;
    float dobleProb;       // probabilidad de parpadeo doble
    float miradaMs;        // intervalo medio entre saltos de mirada
    float miradaCv;
    float rangoX, rangoY;  // amplitud maxima de la mirada (-1..1)
    float sesgoX, sesgoY;  // hacia donde tiende a mirar (triste: abajo)
    float centroProb;      // probabilidad de volver a "mirarte a vos"
    float hz, zeta;        // resorte de la mirada
    float parpado;         // apertura de base (cansado ~0,55)
    float micro;           // amplitud de las micro-sacadas
};

struct EstadoOjos
{
    Muelle mx, my;
    float objX, objY;          // objetivo del salto actual
    float microX, microY;      // micro-sacada sobre el objetivo
    float ultX[2], ultY[2];    // ultimos dos objetivos (para no repetir)
    uint32_t proxMirada;
    uint32_t proxMicro;

    bool parpadeando;
    uint32_t inicioParpadeo;
    float cierre, sosten, abre; // fases sorteadas de ESTE parpadeo
    float profundidad;          // 1 completo, ~0,6 incompleto
    bool fasesListas;           // ya se sortearon las fases de este parpadeo
    bool dobleTrasEste;
    uint32_t proxParpadeo;
    uint32_t sinParpadearHasta; // inhibicion (sorpresa: ojos fijos)
    uint32_t ultimoFinParpadeo;

    // Salidas del frame
    float x, y;                 // mirada actual
    float apertura;             // 0 cerrado .. 1 abierto (ambos ojos)
    float cierreParpadeo;       // 0..1 solo del parpadeo (sin el parpado)
    bool saltoGrande;           // este frame empezo un salto largo
};

void ojosIniciar(EstadoOjos &e, const ParamOjos &p, uint32_t t, Azar &azar);

// biasX/biasY: sesgo extra del momento (inclinacion de la placa, gestos).
void ojosAvanzar(EstadoOjos &e, const ParamOjos &p, uint32_t t, float dt,
                 bool hablando, float biasX, float biasY, Azar &azar);

void ojosParpadear(EstadoOjos &e, uint32_t t, Azar &azar);
void ojosNoParpadearHasta(EstadoOjos &e, uint32_t t);
// Fuerza un salto de mirada a (x, y) que se sostiene 'ms'
void ojosMirar(EstadoOjos &e, float x, float y, uint32_t t, uint32_t ms);

#endif // VIDA_OJOS_H
