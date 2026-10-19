/**
 * Ruido.h - RUIDO SUAVE 1D (value noise) y RESORTES
 *
 * El ruido es la capa "lenta" de la vida: la respiracion y la deriva de la
 * mirada no son un seno perfecto (eso se lee como maquina) sino una curva
 * continua que nunca se repite. Value noise con interpolacion smoothstep:
 * barato (un hash y una mezcla) y suficiente para una matriz de 5x5.
 *
 * El resorte (sistema de segundo orden) es lo que le da PESO a los ojos:
 * con amortiguamiento < 1 la mirada se pasa un poquito del objetivo y vuelve
 * (overshoot), que es el principio de animacion "follow-through". Con
 * amortiguamiento ~1 llega suave y sin rebote (cansado, triste).
 */
#ifndef VIDA_RUIDO_H
#define VIDA_RUIDO_H

#include <stdint.h>
#include <math.h>

// Hash entero -> [-1, 1]
inline float ruidoHash(int32_t i, uint32_t semilla)
{
    uint32_t h = (uint32_t)i * 374761393u + semilla * 668265263u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (float)(h & 0xFFFF) * (2.0f / 65535.0f) - 1.0f;
}

// Ruido continuo en x (una "ondulacion" por unidad de x), valor en [-1, 1]
inline float ruido1(float x, uint32_t semilla)
{
    float fl = floorf(x);
    int32_t i = (int32_t)fl;
    float f = x - fl;
    float u = f * f * (3.0f - 2.0f * f);
    return ruidoHash(i, semilla) * (1.0f - u) + ruidoHash(i + 1, semilla) * u;
}

struct Muelle
{
    float x;   // posicion
    float v;   // velocidad
};

// Avanza el resorte hacia 'objetivo'. hz = que tan rapido responde,
// zeta = amortiguamiento (0,4 rebota, 1,0 llega sin pasarse).
// Euler semi-implicito: estable mientras 2*pi*hz*dt < 2, y dt se recorta.
inline void muelleAvanzar(Muelle &m, float objetivo, float dt, float hz, float zeta)
{
    if (dt > 0.04f) dt = 0.04f;
    float w = 6.2831853f * hz;
    float a = w * w * (objetivo - m.x) - 2.0f * zeta * w * m.v;
    m.v += a * dt;
    m.x += m.v * dt;
}

inline float limitar(float v, float lo, float hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Curvas de 0..1 -> 0..1
inline float suave(float p)      { p = limitar(p, 0.0f, 1.0f); return p * p * (3.0f - 2.0f * p); }
inline float acelera(float p)    { p = limitar(p, 0.0f, 1.0f); return p * p; }
inline float frena(float p)      { p = limitar(p, 0.0f, 1.0f); return 1.0f - (1.0f - p) * (1.0f - p); }
// Sube y baja (campana) dentro de 0..1, con la subida en 'pico'
inline float campana(float p, float pico)
{
    p = limitar(p, 0.0f, 1.0f);
    if (p < pico) return frena(p / pico);
    return 1.0f - suave((p - pico) / (1.0f - pico));
}

#endif // VIDA_RUIDO_H
