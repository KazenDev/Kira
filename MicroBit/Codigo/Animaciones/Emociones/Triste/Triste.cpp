/**
 * Triste.cpp - La personalidad TRISTE
 *
 * Ojos en (1,1) y (3,1) mirando hacia abajo, boca en arco caido. Gestos:
 *
 *   LAGRIMA     se junta bajo un ojo y cae por la mejilla con gravedad
 *               (manda "LAGRIMA" por serial, como antes)
 *   SUSPIRO     la cara se apaga y se hunde un poco, y vuelve
 *   ESPERANZA   levanta la mirada un momento y la boca casi se endereza
 *   LABIO       el labio tiembla, a punto de llorar
 *   CAIDA       los ojos se cierran despacio, pesados (gesto de llegada)
 *
 * Hablando: el arco se abre hacia abajo, apenas (habla bajito y lento).
 */
#include "Triste.h"
#include "../../Vida/Nucleo/SonidoId.h"

enum { G_LAGRIMA = 0, G_SUSPIRO, G_ESPERANZA, G_LABIO, G_CAIDA, NG_TRISTE };

static const DefGesto GESTOS[NG_TRISTE] = {
    // durMin durMax peso flags      enfriar sonido
    { 2200,  2800,   3,   G_AMBOS,   9000,   S_LAGRIMA },   // LAGRIMA
    { 1700,  2300,   3,   G_REPOSO,  8000,   S_SUSPIRO },   // SUSPIRO
    { 1200,  1700,   2,   G_REPOSO, 10000,   S_NADA    },   // ESPERANZA
    {  800,  1200,   2,   G_AMBOS,   6000,   S_NADA    },   // LABIO
    { 1300,  1800,   2,   G_AMBOS,   9000,   S_NADA    },   // CAIDA
};

static const uint8_t ARCO[3][2] = { {1,3}, {2,3}, {3,3} };
static const uint8_t INTERIOR[3][2] = { {1,4}, {2,4}, {3,4} };

static void lagrima(Lienzo &l, const EstadoCara &c, float q, const Pose &pose)
{
    float x = ((c.gestoInicio >> 4) & 1) ? 1.0f : 3.0f;
    if (q < 0.18f) {
        // Se junta bajo el ojo
        punto(l, x, 1.7f, 0.55f * frena(q / 0.18f), pose);
        return;
    }
    // Cae con gravedad (acelera) y se apaga al llegar abajo
    float caida = (q - 0.18f) / 0.82f;
    float y = 1.7f + 2.9f * acelera(caida);
    float v = 0.55f * (1.0f - suave((caida - 0.7f) / 0.3f));
    punto(l, x - 0.25f * caida, y, v, pose);
}

static void dibujar(Lienzo &l, const EstadoCara &c, const Pose &base)
{
    Pose pose = base;
    float q = c.gp;
    float ap = c.ojos.apertura;
    float ojosDy = 0.0f;
    float esquinas = 0.7f;
    float tiembla = 0.0f;

    switch (c.gesto) {
        case G_SUSPIRO: {
            float w = campana(q, 0.45f);
            pose.brillo *= 1.0f - 0.35f * w;
            pose.dy += 0.3f * w;
            ap *= 1.0f - 0.4f * w;
            break;
        }
        case G_ESPERANZA: {
            float w = campana(q, 0.3f);
            ojosDy = -0.75f * w;
            esquinas = 0.7f * (1.0f - 0.65f * w);
            break;
        }
        case G_LABIO:
            tiembla = campana(q, 0.2f);
            break;
        case G_CAIDA:
            ap *= 1.0f - 0.7f * campana(q, 0.5f);
            break;
        default: break;
    }

    Pose po = pose;
    po.dy += ojosDy;
    ojoPunto(l, c, 1, 1, ap, 0.85f, po);
    ojoPunto(l, c, 3, 1, ap, 0.85f, po);

    if (c.gesto == G_LAGRIMA) lagrima(l, c, q, pose);

    // Boca en arco caido; el labio tiembla rapido y chiquito
    Pose pb = pose;
    if (tiembla > 0.0f) {
        float te = (float)(c.t - c.gestoInicio) * 0.001f;
        pb.dy += 0.14f * tiembla * sinf(6.2831853f * 9.0f * te);
        pb.brillo *= 1.0f - 0.25f * tiembla * (0.5f + 0.5f * sinf(6.2831853f * 7.0f * te));
    }
    float b = c.habla.apertura;
    rasgo(l, ARCO, 3, 0.75f * (1.0f - 0.25f * b), pb);
    punto(l, 0, 4, esquinas, pb);
    punto(l, 4, 4, esquinas, pb);
    if (b > 0.03f) rasgo(l, INTERIOR, 3, 0.6f * frena(b), pb);
}

const Personalidad PERS_TRISTE = {
    // parpadeo  cv   cierre sosten abre doble  mirada cv   rX   rY    sX  sY     centro hz   zeta parpado micro
    { 6500, 0.7f,  120,   90,  320, 0.05f, 3800, 0.5f, 0.6f, 0.3f, 0, 0.55f, 0.3f,  2.8f, 0.95f, 0.9f, 0.06f },
    // silaba cv   sil/pal hueco pausa min max  ampMin ampMax enfasis
    {  215, 0.3f, 2.3f,  110,   650,  3,  6,  0.25f, 0.65f, 0.08f },
    GESTOS, NG_TRISTE, 5000, 0.5f,
    0.14f, 0.18f,      // respira lento y hondo
    0.7f,              // intensidad: mas apagada
    0.0f,
    G_CAIDA,           // llega con los ojos pesados
    R_COSQUILLAS,      // tocarla la anima un poquito
    dibujar
};
