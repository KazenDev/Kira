/**
 * Alegria.cpp - La personalidad ALEGRIA
 *
 * La cara de siempre (ojos en (1,1) y (3,1), sonrisa completa) pero viva:
 * mira para todos lados con un resorte que rebota un poco (alegre = agil),
 * parpadea seguido y sin ritmo, y cada tanto hace uno de sus gestos:
 *
 *   GUINO        un ojo (al azar cual) se cierra y se abre
 *   FELIZ        los ojos se vuelven ^ ^ un ratito
 *   REBOTE       la cara entera da un saltito (gesto de llegada)
 *   MEJILLAS     las esquinas de la sonrisa se encienden y suben
 *   MIRA_ARRIBA  mira para arriba con una risita, como recordando algo
 *
 * Hablando: los "dientes" (fila 3) se abren con cada silaba y en las
 * silabas fuertes la cara asiente un poquito para arriba.
 */
#include "Alegria.h"
#include "../../Vida/Nucleo/SonidoId.h"

enum { G_GUINO = 0, G_FELIZ, G_REBOTE, G_MEJILLAS, G_MIRA_ARRIBA, NG_ALEGRIA };

static const DefGesto GESTOS[NG_ALEGRIA] = {
    // durMin durMax peso flags      enfriar sonido
    {  380,   520,   3,   G_AMBOS,   5000,   S_NADA   },   // GUINO
    {  700,  1100,   3,   G_AMBOS,   7000,   S_NADA   },   // FELIZ
    {  450,   600,   2,   G_AMBOS,   6000,   S_NADA   },   // REBOTE
    {  800,  1200,   2,   G_REPOSO,  8000,   S_NADA   },   // MEJILLAS
    {  900,  1400,   2,   G_REPOSO, 10000,   S_RISITA },   // MIRA_ARRIBA
};

static const uint8_t SONRISA[3][2] = { {1,4}, {2,4}, {3,4} };
static const uint8_t DIENTES[3][2] = { {1,3}, {2,3}, {3,3} };

static void dibujar(Lienzo &l, const EstadoCara &c, const Pose &base)
{
    Pose pose = base;
    float q = c.gp;
    float aI = c.ojos.apertura, aD = c.ojos.apertura;
    float felices = 0.0f;       // 0 ojos punto .. 1 ojos ^ ^
    float mejillas = 0.75f;
    float subeComisura = 0.0f;
    float ojosDy = 0.0f;

    switch (c.gesto) {
        case G_GUINO: {
            float w = campana(q, 0.35f);
            if ((c.gestoInicio >> 4) & 1) aI *= 1.0f - w; else aD *= 1.0f - w;
            break;
        }
        case G_FELIZ:
            felices = campana(q, 0.2f);
            break;
        case G_REBOTE:
            // Salto y un rebotito al caer (follow-through)
            if (q < 0.65f) pose.dy -= 0.5f * sinf(3.14159265f * q / 0.65f);
            else           pose.dy += 0.12f * sinf(3.14159265f * (q - 0.65f) / 0.35f);
            break;
        case G_MEJILLAS: {
            float w = campana(q, 0.4f);
            mejillas = 0.75f + 0.25f * w;
            subeComisura = 0.45f * w;
            break;
        }
        case G_MIRA_ARRIBA: {
            float w = campana(q, 0.3f);
            ojosDy = -0.55f * w;
            subeComisura = 0.35f * w;
            felices = 0.5f * w;
            break;
        }
        default: break;
    }

    // En la silaba fuerte asiente un poquito
    pose.dy -= 0.15f * c.habla.enfasis;

    // Ojos: punto que mira, fundido con ^ ^ si esta "feliz"
    Pose po = pose;
    po.dy += ojosDy;
    po.brillo *= 1.0f - felices;
    ojoPunto(l, c, 1, 1, aI, 1.0f, po);
    ojoPunto(l, c, 3, 1, aD, 1.0f, po);
    if (felices > 0.0f) {
        Pose pf = pose;
        pf.brillo *= felices;
        punto(l, 1, 0, 1.0f, pf);  punto(l, 3, 0, 1.0f, pf);
        punto(l, 0, 1, 0.8f, pf);  punto(l, 2, 1, 0.8f, pf);  punto(l, 4, 1, 0.8f, pf);
    }

    // Sonrisa: comisuras (que pueden subir) + curva de abajo
    punto(l, 0, 3.0f - subeComisura, mejillas, pose);
    punto(l, 4, 3.0f - subeComisura, mejillas, pose);
    float b = c.habla.apertura;
    rasgo(l, SONRISA, 3, 0.8f + 0.2f * b, pose);

    // Hablando: los dientes se abren con cada silaba
    if (b > 0.03f) rasgo(l, DIENTES, 3, 0.95f * frena(b), pose);
}

const Personalidad PERS_ALEGRIA = {
    // parpadeo  cv   cierre sosten abre doble  mirada cv   rX   rY    sX  sY     centro hz   zeta parpado micro
    { 4500, 0.75f,  75,   40,  160, 0.12f, 1900, 0.55f, 0.9f, 0.55f, 0, -0.05f, 0.55f, 6.0f, 0.55f, 1.0f, 0.12f },
    // silaba cv   sil/pal hueco pausa min max  ampMin ampMax enfasis
    {  145, 0.3f, 2.4f,   70,   450,  3,  7,  0.45f, 1.0f,  0.18f },
    GESTOS, NG_ALEGRIA, 3500, 0.5f,
    0.25f, 0.10f,      // respira
    0.95f,             // intensidad
    0.0f,              // temblor
    G_REBOTE,          // llega dando un saltito
    R_COSQUILLAS,
    dibujar
};
