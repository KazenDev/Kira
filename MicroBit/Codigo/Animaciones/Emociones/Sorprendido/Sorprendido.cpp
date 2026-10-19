/**
 * Sorprendido.cpp - La personalidad SORPRENDIDO
 *
 * Ojos altos en las columnas 1 y 3 (filas 0-1), boca "o" en (2,3) con un
 * anillo tenue alrededor que le da profundidad. Gestos:
 *
 *   ASOMBRO   el anillo se enciende: "O" grande, la cara sube
 *   DOBLE     parpadeo doble rapido, "no lo puedo creer"
 *   SALTONA   los ojos van y vienen rapido de un costado al otro
 *   CEJAS     la cara entera sube como levantando las cejas (llegada)
 *
 * Hablando: la "o" se ahueca y el anillo se abre con cada silaba.
 */
#include "Sorprendido.h"
#include "../../Vida/Nucleo/SonidoId.h"

enum { G_ASOMBRO = 0, G_DOBLE, G_SALTONA, G_CEJAS, NG_SORPRENDIDO };

static const DefGesto GESTOS[NG_SORPRENDIDO] = {
    // durMin durMax peso flags      enfriar sonido
    {  600,   900,   3,   G_REPOSO,  5000,   S_OH   },   // ASOMBRO
    {  420,   520,   3,   G_AMBOS,   4000,   S_NADA },   // DOBLE
    {  700,   900,   2,   G_REPOSO,  6000,   S_NADA },   // SALTONA
    {  500,   800,   2,   G_AMBOS,   5000,   S_NADA },   // CEJAS
};

static const uint8_t ANILLO[4][2] = { {2,2}, {1,3}, {3,3}, {2,4} };

static void dibujar(Lienzo &l, const EstadoCara &c, const Pose &base)
{
    Pose pose = base;
    float q = c.gp;
    float ap = c.ojos.apertura;
    float anillo = 0.22f;
    float ojosDx = 0.0f;

    switch (c.gesto) {
        case G_ASOMBRO: {
            float w = campana(q, 0.25f);
            anillo = 0.22f + 0.7f * w;
            pose.dy -= 0.25f * w;
            pose.brillo *= 1.0f + 0.15f * w;
            break;
        }
        case G_DOBLE: {
            // Dos cierres rapidos seguidos
            float a = q < 0.5f ? q / 0.5f : (q - 0.5f) / 0.5f;
            ap *= 1.0f - campana(a, 0.4f);
            break;
        }
        case G_SALTONA:
            ojosDx = 0.6f * sinf(6.2831853f * 2.0f * q) * campana(q, 0.15f);
            break;
        case G_CEJAS:
            pose.dy -= 0.4f * campana(q, 0.3f);
            break;
        default: break;
    }

    pose.dy -= 0.12f * c.habla.enfasis;

    Pose po = pose;
    po.dx += ojosDx;
    ojoAlto(l, c, 1, 1, ap, 1.0f, po);
    ojoAlto(l, c, 3, 1, ap, 1.0f, po);

    float b = c.habla.apertura;
    punto(l, 2, 3, 0.9f * (1.0f - 0.55f * b), pose);
    rasgo(l, ANILLO, 4, anillo + 0.65f * frena(b), pose);
}

const Personalidad PERS_SORPRENDIDO = {
    // parpadeo  cv   cierre sosten abre doble  mirada cv   rX   rY    sX  sY      centro hz    zeta parpado micro
    { 3200, 0.7f,   55,   25,  110, 0.3f,  1100, 0.55f, 1.0f, 0.6f, 0, -0.15f, 0.45f, 10.0f, 0.45f, 1.0f, 0.1f },
    // silaba cv   sil/pal hueco pausa min max  ampMin ampMax enfasis
    {  140, 0.3f, 2.4f,   70,   420,  2,  6,  0.5f,  1.0f,  0.25f },
    GESTOS, NG_SORPRENDIDO, 3000, 0.5f,
    0.3f, 0.08f,
    0.95f,
    0.0f,
    G_CEJAS,
    R_COSQUILLAS,
    dibujar
};
