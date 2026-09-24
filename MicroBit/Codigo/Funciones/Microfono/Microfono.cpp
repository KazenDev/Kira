/**
 * Microfono.cpp - FUNCION: lectura puntual del microfono para la IA
 *
 * Reutiliza MicFft, que ya normaliza cinco bandas de frecuencia. Si el micro
 * no estaba activo, se enciende solo durante una ventana breve y se vuelve a
 * apagar. Si ya lo estaban usando LOAD4/VOZ, no se lo interrumpe.
 *
 * La escucha manual tiene prioridad: si la persona esta manteniendo una
 * captura con ESCUCHAR, el sensor devuelve ocupado para no cortar el audio.
 */
#include "Microfono.h"
#include "../../Animaciones/Sistema/MicFft/MicFft.h"
#include "../../Animaciones/Sistema/Grabar/Grabar.h"
#include "../../Animaciones/Sistema/Escuchar/Escuchar.h"

extern MicroBit uBit;

static int escalaBanda(float valor)
{
    // MicFft entrega 0..4.5; para la IA lo llevamos a una escala 0..100.
    int n = (int)(valor * (100.0f / 4.5f) + 0.5f);
    if (n < 0) n = 0;
    if (n > 100) n = 100;
    return n;
}

LecturaMicrofono leerMicrofono()
{
    LecturaMicrofono l;
    l.nivel = 0;
    l.ventanas = 0;
    l.estado = -1;
    for (int i = 0; i < 5; i++) l.bandas[i] = 0;

    // No robamos el microfono de una captura manual. El lock del backend
    // evita normalmente esta colision, pero el boton A/B puede estar usandolo
    // sin una peticion HTTP pendiente.
    if (grabandoSerial || modoEscuchar) {
        l.estado = 0;
        return l;
    }

    bool estabaActivo = micFftActivo();
    if (!estabaActivo) micFftIniciar();
    if (!micFftActivo()) return l;

    // Esperamos como maximo 100 ms a que el sink produzca una ventana nueva.
    int antes = micFftConteo();
    for (int i = 0; i < 20 && micFftConteo() <= antes; i++)
        uBit.sleep(5);

    int despues = micFftConteo();
    l.ventanas = despues - antes;
    if (l.ventanas <= 0) {
        if (!estabaActivo) micFftDetener();
        return l;
    }

    const float *b = micFftBandas();
    for (int i = 0; i < 5; i++) {
        l.bandas[i] = escalaBanda(b[i]);
        if (l.bandas[i] > l.nivel) l.nivel = l.bandas[i];
    }
    l.estado = 1;

    // No dejamos el micro prendido por una simple consulta. Si ya estaba
    // activo para una animacion, esa animacion conserva su estado normal.
    if (!estabaActivo) micFftDetener();
    return l;
}
