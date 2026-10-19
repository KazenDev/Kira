/**
 * Cansado.cpp - La personalidad CANSADO
 *
 * Ojos (1,1) y (3,1) entrecerrados y tenues, boca chica. Gestos:
 *
 *   BOSTEZO   la boca se abre en una "O" grande, los ojos se cierran
 *   CABECEO   se queda dormida: los ojos se cierran y la cara baja... y de
 *             golpe se despierta (sube con un rebote)
 *   PERDIDA   la mirada se va despacio a un costado, en la nada
 *   PESADO    los parpados caen y suben (gesto de llegada)
 */
#include "Cansado.h"
#include "../../Vida/Nucleo/SonidoId.h"

enum { G_BOSTEZO = 0, G_CABECEO, G_PERDIDA, G_PESADO, NG_CANSADO };

static const DefGesto GESTOS[NG_CANSADO] = {
    // durMin durMax peso flags      enfriar sonido
    { 1800,  2400,   3,   G_REPOSO, 12000,   S_BOSTEZO },   // BOSTEZO
    { 2000,  2600,   3,   G_REPOSO,  9000,   S_NADA    },   // CABECEO
    { 1500,  2200,   2,   G_REPOSO,  8000,   S_NADA    },   // PERDIDA
    {  900,  1200,   2,   G_AMBOS,   6000,   S_NADA    },   // PESADO
};

static void dibujar(Lienzo &l, const EstadoCara &c, const Pose &base)
{
    Pose pose = base;
    float q = c.gp;
    float tt = (float)(c.t - c.t0) * 0.001f;
    // Los parpados derivan solos, como luchando contra el sueno
    float ap = c.ojos.apertura * (0.88f + 0.12f * ruido1(tt * 0.3f, c.semilla + 31u));
    float bostezo = 0.0f;
    float ojosDx = 0.0f;

    switch (c.gesto) {
        case G_BOSTEZO: {
            bostezo = campana(q, 0.45f);
            ap *= 1.0f - 0.9f * bostezo;
            pose.dy -= 0.15f * bostezo;
            break;
        }
        case G_CABECEO:
            if (q < 0.6f) {
                float w = acelera(q / 0.6f);
                ap *= 1.0f - w;
                pose.dy += 0.45f * w;
            } else if (q < 0.72f) {
                // Se despierta de golpe: sube pasandose
                float w = (q - 0.6f) / 0.12f;
                pose.dy += 0.45f - 0.7f * frena(w);
                ap = c.ojos.apertura * 1.3f;
            } else {
                float w = (q - 0.72f) / 0.28f;
                pose.dy += -0.25f * (1.0f - suave(w));
                ap = c.ojos.apertura * (1.3f - 0.3f * suave(w));
            }
            break;
        case G_PERDIDA:
            ojosDx = -0.7f * campana(q, 0.4f);
            break;
        case G_PESADO:
            ap *= 1.0f - 0.75f * campana(q, 0.5f);
            break;
        default: break;
    }

    Pose po = pose;
    po.dx += ojosDx;
    ojoPunto(l, c, 1, 1, ap, 1.0f, po);
    ojoPunto(l, c, 3, 1, ap, 1.0f, po);

    // Boca chica; el bostezo la abre en "O" hacia abajo
    float b = c.habla.apertura;
    punto(l, 1, 3, 0.6f, pose);
    punto(l, 3, 3, 0.6f, pose);
    punto(l, 2, 3, 0.6f * (1.0f - bostezo), pose);
    if (bostezo > 0.0f) {
        punto(l, 2, 2, 0.5f * bostezo, pose);
        punto(l, 1, 4, 0.6f * bostezo, pose);
        punto(l, 2, 4, 0.6f * bostezo, pose);
        punto(l, 3, 4, 0.6f * bostezo, pose);
    }
    if (b > 0.03f) punto(l, 2, 4, 0.5f * frena(b), pose);
}

const Personalidad PERS_CANSADO = {
    // parpadeo  cv    cierre sosten abre doble  mirada cv   rX    rY    sX  sY     centro hz    zeta parpado micro
    { 3800, 0.65f,  180,  160,  450, 0.08f, 4200, 0.5f, 0.55f, 0.3f, 0, 0.25f, 0.5f,  2.2f, 1.0f, 0.55f, 0.04f },
    // silaba cv   sil/pal hueco pausa min max  ampMin ampMax enfasis
    {  230, 0.3f, 2.3f,  140,   700,  2,  5,  0.25f, 0.6f,  0.06f },
    GESTOS, NG_CANSADO, 4200, 0.5f,
    0.12f, 0.2f,       // respira lento y hondo
    0.6f,              // apagadita
    0.0f,
    G_PESADO,
    R_DESPERTAR,
    dibujar
};
