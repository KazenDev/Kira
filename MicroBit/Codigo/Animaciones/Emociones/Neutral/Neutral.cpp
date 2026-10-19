/**
 * Neutral.cpp - La personalidad NEUTRAL
 *
 * Parpados en (0,1) y (4,1), pupilas en (1,1) y (3,1), boca (1..3, 3).
 * Gestos:
 *
 *   PEEK        se apagan los parpados y quedan las pupilas mirando fijo
 *   MEH         solo baja la comisura izquierda (asimetria = mas humano)
 *   LENTA       mira despacio de un costado al otro, sin interes
 *   SILBIDO     boca en "o" chiquita y mirada arriba, silbando (aburrida)
 *   ESCEPTICA   entrecierra un ojo mas que el otro: "mmm, seguro..."
 *
 * Hablando: la boca recta se separa en dos lineas con cada silaba.
 */
#include "Neutral.h"
#include "../../Vida/Nucleo/SonidoId.h"

enum { G_PEEK = 0, G_MEH, G_LENTA, G_SILBIDO, G_ESCEPTICA, NG_NEUTRAL };

static const DefGesto GESTOS[NG_NEUTRAL] = {
    // durMin durMax peso flags      enfriar sonido
    {  600,   900,   2,   G_REPOSO,  6000,   S_NADA    },   // PEEK
    {  800,  1200,   3,   G_AMBOS,   5000,   S_NADA    },   // MEH
    { 2200,  2800,   2,   G_REPOSO,  9000,   S_NADA    },   // LENTA
    { 1400,  2000,   1,   G_REPOSO, 15000,   S_SILBIDO },   // SILBIDO
    {  900,  1300,   2,   G_AMBOS,   7000,   S_NADA    },   // ESCEPTICA
};

static const uint8_t ABAJO[3][2] = { {1,4}, {2,4}, {3,4} };

static void dibujar(Lienzo &l, const EstadoCara &c, const Pose &base)
{
    Pose pose = base;
    float q = c.gp;
    float aI = c.ojos.apertura, aD = c.ojos.apertura;
    float parpados = 0.5f;
    float ojosDx = 0.0f, ojosDy = 0.0f;
    float meh = 0.0f;
    float silba = 0.0f;

    switch (c.gesto) {
        case G_PEEK:
            parpados *= 1.0f - campana(q, 0.2f);
            break;
        case G_MEH:
            meh = campana(q, 0.3f);
            break;
        case G_LENTA:
            // Un vaiven lento: a un lado, al otro, y de vuelta
            ojosDx = 0.75f * sinf(6.2831853f * q) * campana(q, 0.15f);
            break;
        case G_SILBIDO: {
            silba = campana(q, 0.15f);
            ojosDy = -0.4f * silba;
            ojosDx = 0.35f * silba;
            break;
        }
        case G_ESCEPTICA: {
            float w = campana(q, 0.3f);
            if ((c.gestoInicio >> 4) & 1) aI *= 1.0f - 0.5f * w;
            else                          aD *= 1.0f - 0.5f * w;
            break;
        }
        default: break;
    }

    Pose po = pose;
    po.dx += ojosDx;
    po.dy += ojosDy;
    ojoPunto(l, c, 1, 1, aI, 1.0f, po);
    ojoPunto(l, c, 3, 1, aD, 1.0f, po);
    // Los parpados no se mueven con la mirada: son el borde del ojo
    punto(l, 0, 1, parpados * frena(aI), pose);
    punto(l, 4, 1, parpados * frena(aD), pose);

    float b = c.habla.apertura;
    if (silba > 0.0f) {
        // La boca recta se cierra en una "o" chiquita en el centro
        Pose ps = pose;
        ps.brillo *= 1.0f - silba;
        punto(l, 1, 3, 0.75f, ps);
        punto(l, 3, 3, 0.75f, ps);
        punto(l, 2, 3, 0.75f + 0.2f * silba, pose);
    } else {
        punto(l, 1, 3 + 0.8f * meh, 0.75f, pose);
        punto(l, 2, 3, 0.75f * (1.0f - 0.3f * b), pose);
        punto(l, 3, 3, 0.75f, pose);
    }
    if (b > 0.03f) rasgo(l, ABAJO, 3, 0.75f * frena(b), pose);
}

const Personalidad PERS_NEUTRAL = {
    // parpadeo  cv   cierre sosten abre doble  mirada cv   rX   rY    sX  sY  centro hz   zeta parpado micro
    { 5200, 0.8f,   80,   45,  170, 0.1f,  1700, 0.6f, 0.8f, 0.4f, 0, 0,  0.55f, 6.0f, 0.7f, 1.0f, 0.1f },
    // silaba cv   sil/pal hueco pausa min max  ampMin ampMax enfasis
    {  160, 0.3f, 2.4f,   70,   500,  3,  8,  0.4f,  0.9f,  0.12f },
    GESTOS, NG_NEUTRAL, 4500, 0.5f,
    0.2f, 0.08f,
    0.85f,
    0.0f,
    -1,                // sin gesto de llegada: neutral es neutral
    R_COSQUILLAS,
    dibujar
};
