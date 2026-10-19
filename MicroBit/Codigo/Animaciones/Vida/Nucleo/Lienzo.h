/**
 * Lienzo.h - El LIENZO 5x5 donde se compone cada frame de la cara
 *
 * La cara no se escribe pixel a pixel en el display: se COMPONE aca (ojos,
 * cejas, boca, reaccion) y al final se vuelca de una. Eso permite:
 *
 *   - PUNTOS SUBPIXEL: un ojo en x = 1,4 reparte su luz entre la columna 1
 *     (60%) y la 2 (40%). La mirada se DESLIZA en vez de saltar de LED en LED.
 *     El reparto es en luz LINEAL (lo que hace el PWM), asi la luz total del
 *     punto se conserva mientras se mueve.
 *   - MEZCLAR dos caras (la base y una reaccion) con un peso: fundidos sin
 *     cortes entre la emocion y la reaccion.
 *   - Escribir en el display SOLO lo que cambio (lo hace Vida.cpp).
 *
 * Los valores son luz lineal 0..1. Para pensar en "brillo percibido" esta
 * percibido(): el ojo humano no es lineal (gamma ~2), asi que un fundido
 * lineal de PWM se ve como un salto al principio y nada al final.
 */
#ifndef VIDA_LIENZO_H
#define VIDA_LIENZO_H

#include <stdint.h>

struct Lienzo
{
    float v[5][5];   // [y][x], luz lineal 0..1
};

void lienzoLimpiar(Lienzo &l);

// Enciende un pixel entero (si ya habia mas luz, se queda la mayor)
void lienzoPixel(Lienzo &l, int x, int y, float v);

// Un punto en coordenadas reales: reparte la luz entre los 4 pixeles
// vecinos (bilineal). Lo que cae fuera de la matriz se pierde.
void lienzoPunto(Lienzo &l, float x, float y, float v);

// Mezcla: dst = dst*(1-w) + src*w
void lienzoMezclar(Lienzo &dst, const Lienzo &src, float w);

void lienzoEscalar(Lienzo &l, float k);

// Brillo percibido (0..1) -> luz lineal (0..1)
inline float percibido(float p)
{
    if (p <= 0.0f) return 0.0f;
    if (p >= 1.0f) return 1.0f;
    return p * p;
}

// Luz lineal -> valor de pixel de CODAL (0..255)
inline uint8_t lienzoAByte(float v)
{
    if (v <= 0.0f) return 0;
    if (v >= 1.0f) return 255;
    return (uint8_t)(v * 255.0f + 0.5f);
}

#endif // VIDA_LIENZO_H
