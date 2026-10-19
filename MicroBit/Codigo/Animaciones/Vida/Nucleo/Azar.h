/**
 * Azar.h - El AZAR del motor de vida (determinista con semilla)
 *
 * Un xorshift32 propio en vez de uBit.random(): asi el NUCLEO no depende de
 * CODAL y se puede testear en el PC con una semilla fija (el test reproduce
 * exactamente la misma "vida" cada vez). En la placa la semilla sale del RNG
 * de hardware al arrancar, asi que dos encendidos nunca viven igual.
 *
 * Por que log-normal: los intervalos naturales (entre parpadeos, entre
 * miradas) NO son uniformes. Los parpadeos humanos tienen un coeficiente de
 * variacion de ~0,7-0,9 (Johnston 2013): muchos intervalos cortos y algunos
 * muy largos. Un uniforme se siente "maquina" justamente porque no tiene cola.
 */
#ifndef VIDA_AZAR_H
#define VIDA_AZAR_H

#include <stdint.h>
#include <math.h>

struct Azar
{
    uint32_t s;
};

inline void azarSembrar(Azar &a, uint32_t semilla)
{
    a.s = semilla ? semilla : 0x9E3779B9u;   // xorshift no puede arrancar en 0
}

inline uint32_t azarU32(Azar &a)
{
    uint32_t x = a.s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    a.s = x ? x : 0x9E3779B9u;
    return a.s;
}

// [0, 1)
inline float azarF(Azar &a)
{
    return (float)(azarU32(a) >> 8) * (1.0f / 16777216.0f);
}

inline float azarRango(Azar &a, float lo, float hi)
{
    return lo + (hi - lo) * azarF(a);
}

// [0, n)
inline int azarEntero(Azar &a, int n)
{
    return n <= 0 ? 0 : (int)(azarU32(a) % (uint32_t)n);
}

inline bool azarProb(Azar &a, float p)
{
    return azarF(a) < p;
}

// Normal estandar aproximada: suma de 4 uniformes (Irwin-Hall), centrada y
// escalada. Sobra para animacion y no usa log/sqrt por muestra.
inline float azarNormal(Azar &a)
{
    float s = azarF(a) + azarF(a) + azarF(a) + azarF(a);
    return (s - 2.0f) * 1.7320508f;
}

// Log-normal con MEDIA y coeficiente de variacion dados (no mu/sigma, que
// son incomodos de pensar). Recortada a [media/4, media*3] para que un
// intervalo raro no deje la cara congelada.
inline float azarLogNormal(Azar &a, float media, float cv)
{
    float s2 = logf(1.0f + cv * cv);
    float mu = logf(media) - 0.5f * s2;
    float v = expf(mu + sqrtf(s2) * azarNormal(a));
    if (v < media * 0.25f) v = media * 0.25f;
    if (v > media * 3.0f)  v = media * 3.0f;
    return v;
}

#endif // VIDA_AZAR_H
