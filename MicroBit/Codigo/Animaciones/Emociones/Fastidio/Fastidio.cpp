/**
 * Fastidio.cpp - La personalidad FASTIDIO
 *
 * Parpados pesados en la fila 0, ojos de reojo: (0,1)+(3,1) mirando a la
 * izquierda o (1,1)+(4,1) a la derecha. El lado cambia solo cada tanto
 * (ruido lento), deslizando los ojos. Gestos:
 *
 *   GIRO      giro de ojos: suben, cruzan al otro lado y vuelven (llegada)
 *   TSK       la comisura derecha se levanta un instante
 *   CEJA      el borde interno de las cejas tiembla
 *   SUSPIRO   "aaay": se apaga y los parpados caen
 *   DURO      un parpadeo duro y seco, de enfasis
 */
#include "Fastidio.h"
#include "../../Vida/Nucleo/SonidoId.h"

enum { G_GIRO = 0, G_TSK, G_CEJA, G_SUSPIRO, G_DURO, NG_FASTIDIO };

static const DefGesto GESTOS[NG_FASTIDIO] = {
    // durMin durMax peso flags      enfriar sonido
    {  900,  1200,   3,   G_AMBOS,   7000,   S_UGH     },   // GIRO
    {  250,   400,   3,   G_AMBOS,   4000,   S_NADA    },   // TSK
    {  700,  1000,   2,   G_AMBOS,   5000,   S_NADA    },   // CEJA
    { 1200,  1600,   2,   G_REPOSO,  9000,   S_SUSPIRO },   // SUSPIRO
    {  140,   190,   2,   G_AMBOS,   4000,   S_NADA    },   // DURO
};

static void dibujar(Lienzo &l, const EstadoCara &c, const Pose &base)
{
    Pose pose = base;
    float q = c.gp;
    float te = (float)(c.t - c.gestoInicio) * 0.001f;
    float tt = (float)(c.t - c.t0) * 0.001f;
    float ap = c.ojos.apertura;

    // Lado del reojo: -1 izquierda, +1 derecha. Ruido lento saturado: casi
    // siempre en un lado, y cada ~10-20 s cruza al otro deslizandose.
    float lado = limitar(ruido1(tt * 0.07f, c.semilla + 21u) * 5.0f, -1.0f, 1.0f);
    if (fabsf(lado) < 0.05f) lado = 0.05f;

    float ojosDy = 0.0f;
    float tsk = 0.0f;
    float cejaIn = 0.5f;

    switch (c.gesto) {
        case G_GIRO:
            // Arco: sube y cruza al otro lado, y vuelve
            lado *= cosf(6.2831853f * q);
            ojosDy = -0.95f * sinf(3.14159265f * q);
            cejaIn = 0.5f + 0.3f * sinf(3.14159265f * q);
            break;
        case G_TSK:
            tsk = campana(q, 0.3f);
            break;
        case G_CEJA:
            cejaIn = 0.5f * (1.0f - 0.8f * campana(q, 0.1f) *
                     (0.5f + 0.5f * sinf(6.2831853f * 10.0f * te)));
            break;
        case G_SUSPIRO: {
            float w = campana(q, 0.4f);
            pose.brillo *= 1.0f - 0.3f * w;
            ap *= 1.0f - 0.5f * w;
            break;
        }
        case G_DURO:
            ap *= 1.0f - campana(q, 0.4f);
            break;
        default: break;
    }

    // Parpados pesados (cejas bajas)
    punto(l, 0, 0, 0.5f, pose);
    punto(l, 4, 0, 0.5f, pose);
    punto(l, 1, 0, cejaIn, pose);
    punto(l, 3, 0, cejaIn, pose);

    Pose po = pose;
    po.dy += ojosDy;
    ojoPunto(l, c, 0.5f + 0.5f * lado, 1, ap, 1.0f, po);
    ojoPunto(l, c, 3.5f + 0.5f * lado, 1, ap, 1.0f, po);

    // Boca recta, con la comisura del "tsk"; habla con la boca casi quieta
    float b = c.habla.apertura;
    punto(l, 1, 3, 0.7f, pose);
    punto(l, 2, 3, 0.7f, pose);
    punto(l, 3 + 0.4f * tsk, 3 - 0.7f * tsk, 0.7f, pose);
    if (b > 0.03f) punto(l, 2, 4, 0.55f * frena(b), pose);
}

const Personalidad PERS_FASTIDIO = {
    // parpadeo  cv   cierre sosten abre doble  mirada cv   rX    rY    sX  sY     centro hz   zeta parpado micro
    { 5000, 0.7f,   45,   50,  120, 0.05f, 2600, 0.55f, 0.3f, 0.3f, 0, -0.1f, 0.3f,  5.0f, 0.8f, 0.8f, 0.05f },
    // silaba cv   sil/pal hueco pausa min max  ampMin ampMax enfasis
    {  170, 0.3f, 2.3f,   80,   520,  3,  7,  0.3f,  0.7f,  0.15f },
    GESTOS, NG_FASTIDIO, 3800, 0.5f,
    0.18f, 0.1f,
    0.85f,
    0.0f,
    G_GIRO,
    R_GRUNIDO,
    dibujar
};
