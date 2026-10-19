/**
 * Lienzo.cpp - Composicion del frame 5x5 (ver Lienzo.h)
 */
#include "Lienzo.h"
#include <math.h>

void lienzoLimpiar(Lienzo &l)
{
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            l.v[y][x] = 0.0f;
}

void lienzoPixel(Lienzo &l, int x, int y, float v)
{
    if (x < 0 || x > 4 || y < 0 || y > 4 || v <= 0.0f) return;
    if (v > l.v[y][x]) l.v[y][x] = v;
}

void lienzoPunto(Lienzo &l, float x, float y, float v)
{
    if (v <= 0.0f) return;
    float fx = floorf(x), fy = floorf(y);
    int x0 = (int)fx, y0 = (int)fy;
    float ax = x - fx, ay = y - fy;

    // Los puntos casi enteros no se reparten: un ojo quieto se ve nitido.
    if (ax < 0.04f) ax = 0.0f; else if (ax > 0.96f) { ax = 0.0f; x0++; }
    if (ay < 0.04f) ay = 0.0f; else if (ay > 0.96f) { ay = 0.0f; y0++; }

    float w00 = (1.0f - ax) * (1.0f - ay);
    float w10 = ax * (1.0f - ay);
    float w01 = (1.0f - ax) * ay;
    float w11 = ax * ay;

    // Suma (no maximo) dentro del punto: es luz que se reparte. Contra lo que
    // ya habia en el lienzo, se queda la mayor (dos rasgos que se tocan no
    // se "queman" sumando).
    if (w00 > 0.0f) lienzoPixel(l, x0,     y0,     v * w00);
    if (w10 > 0.0f) lienzoPixel(l, x0 + 1, y0,     v * w10);
    if (w01 > 0.0f) lienzoPixel(l, x0,     y0 + 1, v * w01);
    if (w11 > 0.0f) lienzoPixel(l, x0 + 1, y0 + 1, v * w11);
}

void lienzoMezclar(Lienzo &dst, const Lienzo &src, float w)
{
    if (w <= 0.0f) return;
    if (w >= 1.0f) { dst = src; return; }
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            dst.v[y][x] = dst.v[y][x] * (1.0f - w) + src.v[y][x] * w;
}

void lienzoEscalar(Lienzo &l, float k)
{
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++)
            l.v[y][x] *= k;
}
