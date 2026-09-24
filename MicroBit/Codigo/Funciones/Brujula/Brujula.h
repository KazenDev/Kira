/**
 * Brujula.h - FUNCION: leer rumbo y fuerza magnetica
 *
 * El magnetometro viene en el chip combinado del sensor de movimiento. El
 * rumbo solo es confiable despues de calibrar; por eso la lectura incluye un
 * flag de calibracion y usa -1 cuando todavia no hay rumbo valido.
 */
#ifndef FUNCION_BRUJULA_H
#define FUNCION_BRUJULA_H

struct LecturaBrujula
{
    int rumbo;        // 0..359 grados, o -1 si falta calibracion
    int campo;        // intensidad magnetica reportada por CODAL
    int calibrada;    // 1 = calibrada, 0 = requiere calibracion
};

LecturaBrujula leerBrujula();

#endif // FUNCION_BRUJULA_H
