/**
 * Catalogo.h - Los SONIDOS de Kira, como datos (puro, sin CODAL)
 *
 * Cada sonido es una frase de "notas" del sintetizador de CODAL (onda,
 * volumen, frecuencia que viaja de inicio a fin, duracion, curva, vibrato).
 * Este modulo las CODIFICA al formato de texto de SoundExpressions (72
 * caracteres por segmento, separados por coma).
 *
 * Tres trampas del sintetizador de CODAL que este modulo esquiva (leidas en
 * SoundSynthesizerEffects.cpp):
 *
 *   - El "vibrato" de CODAL (fx 1) NO tiembla: alterna la frecuencia entre
 *     f y f*parametro, y el parametro es entero (x2 = una octava). Suena a
 *     chicharra. Aca el vibrato se ARMA A MANO: la nota se parte en tramos
 *     cortos que suben y bajan unos pocos % (un temblor de verdad).
 *   - La "curva" (forma 2) es sin(paso en grados): con 128 pasos se pasa del
 *     objetivo y vuelve. Con 90 pasos llega justo, frenando suave.
 *   - Las formas 5 y 6 no "suben/bajan exponencial": SUMAN la frecuencia
 *     final a la inicial. No se usan.
 *
 * Por que suenan asi (ver MICROBIT.md, MOTOR DE VIDA):
 *   - tono que SUBE = positivo, que BAJA = negativo (Komatsu 2005)
 *   - el RITMO pesa mas que la melodia (Read & Belpaeme 2012): alegria
 *     rapida y saltada, tristeza lenta y ligada, enojo golpeado y grave
 *   - cada sonido tiene varias variantes y se toca con +-8% de tono y +-10%
 *     de duracion al azar: nunca suena dos veces igual.
 */
#ifndef VIDA_CATALOGO_H
#define VIDA_CATALOGO_H

#include <stdint.h>

struct Nota
{
    uint8_t  onda;       // 0 seno, 1 sierra, 2 triangulo, 3 cuadrada, 4 ruido
    uint16_t vol;        // 0..1023 (0 = silencio: sirve de pausa)
    uint16_t volFin;
    uint16_t frec;       // Hz
    uint16_t frecFin;
    uint16_t dur;        // ms
    uint8_t  forma;      // 0 fija, 1 lineal, 2 curva (frena al llegar)
    uint8_t  vib;        // vibrato: profundidad en % (0 = sin vibrato)
    uint8_t  vibHz;      // vibrato: velocidad (0 = 9 Hz)
};

#define MAX_NOTAS 7          // notas por variante en la tabla
#define MAX_SEGMENTOS 24     // segmentos ya expandidos (vibrato incluido)
#define CHARS_NOTA 72
// Lo maximo que ocupa un sonido codificado (con las comas y el '\0')
#define MAX_CODIGO (MAX_SEGMENTOS * (CHARS_NOTA + 1))

// Codifica 'n' notas con un factor de tono y de duracion. Devuelve el
// largo escrito (sin el '\0') o -1 si no entra.
int codificarNotas(const Nota *notas, int n, float tono, float dur,
                   char *salida, int capacidad);

int catalogoVariantes(uint8_t id);
int catalogoNotas(uint8_t id, int v, const Nota **notas);

// Elige variante y factores a partir de tres numeros al azar en [0,1) y
// codifica. Devuelve el largo o -1.
int catalogoCodificar(uint8_t id, float azarVariante, float azarTono,
                      float azarDur, char *salida, int capacidad);

// Duracion en ms de una variante (sin el factor de azar)
int catalogoDuracion(uint8_t id, int v);

#endif // VIDA_CATALOGO_H
