/**
 * Miedo.cpp - La personalidad MIEDO
 *
 * Cejas (1,0) y (3,0), ojos en (0,1) y (4,1), boca cuadrada de grito en las
 * filas 2-4. Gestos:
 *
 *   SOBRESALTO   da un respingo para arriba y brilla (gesto de llegada)
 *   ATRAS        mira rapido a un lado y al otro, como si algo la siguiera
 *   ENCOGERSE    se achica y se apaga
 *   CASTANETEO   le castanetean los dientes
 *   PUPILAS      aparecen las pupilas (ojos dilatados)
 */
#include "Miedo.h"
#include "../../Vida/Nucleo/SonidoId.h"

enum { G_SOBRESALTO = 0, G_ATRAS, G_ENCOGERSE, G_CASTANETEO, G_PUPILAS, NG_MIEDO };

static const DefGesto GESTOS[NG_MIEDO] = {
    // durMin durMax peso flags      enfriar sonido
    {  550,   700,   3,   G_AMBOS,   5000,   S_NADA     },   // SOBRESALTO
    {  800,  1100,   3,   G_REPOSO,  5000,   S_NADA     },   // ATRAS
    { 1200,  1600,   2,   G_REPOSO,  8000,   S_NADA     },   // ENCOGERSE
    {  600,   900,   2,   G_AMBOS,   6000,   S_TITIRITA },   // CASTANETEO
    { 1000,  1400,   2,   G_REPOSO,  7000,   S_NADA     },   // PUPILAS
};

static const uint8_t BOCA_ABAJO[3][2] = { {1,4}, {2,4}, {3,4} };

static void dibujar(Lienzo &l, const EstadoCara &c, const Pose &base)
{
    Pose pose = base;
    float q = c.gp;
    float te = (float)(c.t - c.gestoInicio) * 0.001f;
    float ap = c.ojos.apertura;
    float ojosDx = 0.0f;
    float pupilas = 0.0f;
    float castanea = 0.0f;
    float boca = 0.75f;

    switch (c.gesto) {
        case G_SOBRESALTO: {
            float w = campana(q, 0.25f);
            pose.dy -= 0.5f * w;
            pose.brillo *= 1.0f + 0.25f * w;
            break;
        }
        case G_ATRAS:
            ojosDx = 0.5f * sinf(6.2831853f * 2.0f * q) * campana(q, 0.1f);
            break;
        case G_ENCOGERSE: {
            float w = campana(q, 0.35f);
            pose.brillo *= 1.0f - 0.4f * w;
            pose.dy += 0.3f * w;
            boca *= 1.0f - 0.3f * w;
            break;
        }
        case G_CASTANETEO:
            castanea = campana(q, 0.1f);
            break;
        case G_PUPILAS:
            pupilas = 0.5f * campana(q, 0.3f);
            break;
        default: break;
    }

    // Cejas levantadas
    punto(l, 1, 0, 0.8f, pose);
    punto(l, 3, 0, 0.8f, pose);

    Pose po = pose;
    po.dx += ojosDx;
    ojoPunto(l, c, 0, 1, ap, 1.0f, po);
    ojoPunto(l, c, 4, 1, ap, 1.0f, po);
    if (pupilas > 0.0f) {
        punto(l, 1, 1, pupilas * frena(ap), pose);
        punto(l, 3, 1, pupilas * frena(ap), pose);
    }

    // Boca de grito: fila 2 (arriba), 3 (lados) y 4 (abajo). Hablando el
    // hueco del centro se llena con la voz y todo el anillo tiembla.
    float b = c.habla.apertura;
    float arriba = boca;
    if (castanea > 0.0f)
        arriba *= 1.0f - 0.7f * castanea * (sinf(6.2831853f * 12.0f * te) > 0.0f ? 1.0f : 0.0f);
    punto(l, 1, 2, arriba, pose);
    punto(l, 2, 2, arriba, pose);
    punto(l, 3, 2, arriba, pose);
    punto(l, 1, 3, boca, pose);
    punto(l, 3, 3, boca, pose);
    rasgo(l, BOCA_ABAJO, 3, boca * (0.8f + 0.2f * b), pose);
    if (b > 0.03f) punto(l, 2, 3, 0.6f * frena(b), pose);
}

const Personalidad PERS_MIEDO = {
    // parpadeo  cv   cierre sosten abre doble  mirada cv   rX    rY    sX  sY  centro hz    zeta parpado micro
    { 2600, 0.6f,   50,   20,  100, 0.2f,  650,  0.5f, 0.45f, 0.25f, 0, 0,  0.25f, 12.0f, 0.5f, 1.0f, 0.15f },
    // silaba cv    sil/pal hueco pausa min max  ampMin ampMax enfasis
    {  128, 0.35f, 2.2f,   50,   300,  2,  5,  0.45f, 1.0f,  0.2f },
    GESTOS, NG_MIEDO, 2800, 0.5f,
    0.5f, 0.12f,       // respira rapido
    0.9f,
    0.12f,             // tiembla siempre
    G_SOBRESALTO,
    R_SOBRESALTO,      // tocarla la asusta mas
    dibujar
};
