/**
 * Tactil.h - FUNCION: leer el logo capacitivo de la micro:bit
 *
 * El logo es un sensor tactil, no un boton mecanico. Devolvemos el estado
 * debounced y la lectura capacitiva cruda para que la IA pueda distinguir
 * "tocado" de "no tocado" sin inventar una cifra fisica de fuerza.
 */
#ifndef FUNCION_TACTIL_H
#define FUNCION_TACTIL_H

struct LecturaTactil
{
    int presionado;  // 1 = tocado, 0 = no tocado
    int lectura;     // lectura capacitiva cruda de CODAL
};

LecturaTactil leerToque();

#endif // FUNCION_TACTIL_H
