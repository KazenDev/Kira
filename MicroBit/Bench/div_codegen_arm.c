/**
 * div_codegen_arm.c - ¿GCC convierte /constante en multiplicacion en un M0+?
 *
 * El M0+ no tiene UMULL ni UDIV, asi que la respuesta NO es obvia: depende de
 * si el compilador puede PROBAR el rango del dividendo. Con rango desconocido
 * GCC no puede usar el truco del reciproco magico (necesariaia multiplicar
 * 32x32->64) y emite una llamada a __aeabi_idiv. Con rango acotado puede
 * usar una constante magica mas chica que SI cabe en 32 bits.
 *
 * Los tres casos de abajo reproducen el codigo REAL de Morfosis.cpp con sus
 * mismos tipos y sus mismos rangos.
 *
 *   arm-none-eabi-gcc -O2 -mcpu=cortex-m0plus -mthumb -c div_codegen_arm.c -o /tmp/div.o
 *   arm-none-eabi-objdump -d /tmp/div.o | grep -c aeabi_idiv
 */
#include <stdint.h>

#define FRAMES 125

/* (1) RANGO DESCONOCIDO: es lo que da el test ingenuo. */
int div_rango_desconocido(int n)
{
    const int dur = FRAMES / 2;       /* 62 */
    return n / dur;
}

/* (2) RANGO ACOTADO: como en Morfosis, donde t viene de f - retardo con
 *     f en [0,125] y retardo en [0,62], o sea t en [-62,125]. */
int div_rango_acotado(int n)
{
    const int dur = FRAMES / 2;       /* 62 */
    if (n < -62 || n > 125) n = 0;   /* el bound que GCC necesita ver */
    return n / dur;
}

/* (3) RANGO DESCONOCIDO y DIVISOR DE RUNTIME: esto si no hay forma de
 *     evitarlo, y es el caso del retardo de Morfosis. */
int div_runtime(int n, int d)
{
    return n / d;
}

/* (4) POTENCIA DE DOS: siempre un shift, single cycle. */
int div_potencia_dos(int n)
{
    return n / 32;
}
