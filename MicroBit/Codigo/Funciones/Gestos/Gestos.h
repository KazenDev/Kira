/**
 * Gestos.h - FUNCION: leer el ultimo gesto y la magnitud de aceleracion
 *
 * CODAL ya mantiene un reconocedor de gestos en segundo plano. Esta lectura
 * devuelve su ultimo gesto estable (sacudida, caida libre, inclinaciones,
 * impactos) junto con la magnitud actual en mili-g.
 */
#ifndef FUNCION_GESTOS_H
#define FUNCION_GESTOS_H

struct LecturaGesto
{
    int codigo;       // Codigo numerico del evento CODAL
    int magnitud_mg;  // sqrt(x^2 + y^2 + z^2), en mili-g
};

LecturaGesto leerGesto();

#endif // FUNCION_GESTOS_H
