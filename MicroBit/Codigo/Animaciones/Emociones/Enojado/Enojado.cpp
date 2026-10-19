/**
 * Enojado.cpp - La personalidad ENOJADO
 *
 * Cejas (0,0) y (4,0) bajando hacia los ojos (1,1) y (3,1): la V del ceno.
 * Boca de dientes apretados. Gestos:
 *
 *   TEMBLOR     la cara entera tiembla de rabia (gesto de llegada)
 *   RESOPLIDO   "hmph": sube el brillo de golpe, entrecierra los ojos
 *   FRUNCE      las cejas se juntan en una barra pesada
 *   FULMINA     mirada fulminante: cejas que aprietan, ojos que no parpadean
 *   DIENTES     rechina los dientes
 */
#include "Enojado.h"
#include "../../Vida/Nucleo/SonidoId.h"

enum { G_TEMBLOR = 0, G_RESOPLIDO, G_FRUNCE, G_FULMINA, G_DIENTES, NG_ENOJADO };

static const DefGesto GESTOS[NG_ENOJADO] = {
    // durMin durMax peso flags      enfriar sonido
    {  500,   700,   3,   G_AMBOS,   5000,   S_NADA },   // TEMBLOR
    {  900,  1200,   3,   G_REPOSO,  6000,   S_HMPH },   // RESOPLIDO
    {  900,  1400,   3,   G_AMBOS,   5000,   S_NADA },   // FRUNCE
    { 1500,  2200,   2,   G_REPOSO,  9000,   S_NADA },   // FULMINA
    {  700,  1000,   2,   G_REPOSO,  7000,   S_NADA },   // DIENTES
};

static const uint8_t LABIO[3][2] = { {1,3}, {2,3}, {3,3} };

static void dibujar(Lienzo &l, const EstadoCara &c, const Pose &base)
{
    Pose pose = base;
    float q = c.gp;
    float te = (float)(c.t - c.gestoInicio) * 0.001f;
    float ap = c.ojos.apertura;
    float cejaInterna = 0.0f;
    float cejaDy = 0.0f;
    float rechina = 0.0f;

    switch (c.gesto) {
        case G_TEMBLOR:
            pose.dx += 0.3f * sinf(6.2831853f * 14.0f * te) * campana(q, 0.2f);
            break;
        case G_RESOPLIDO: {
            float w = campana(q, 0.25f);
            pose.brillo *= 1.0f + 0.3f * w;
            ap *= 1.0f - 0.35f * w;
            break;
        }
        case G_FRUNCE:
            cejaInterna = 0.8f * campana(q, 0.3f);
            ap *= 1.0f - 0.2f * campana(q, 0.3f);
            break;
        case G_FULMINA: {
            float w = campana(q, 0.2f);
            cejaDy = 0.3f * w;
            if (ap < 0.85f * w) ap = 0.85f * w;   // no parpadea
            pose.brillo *= 1.0f + 0.15f * w;
            break;
        }
        case G_DIENTES:
            rechina = campana(q, 0.15f);
            break;
        default: break;
    }

    // En la silaba fuerte la cara pega un sacudon
    pose.dx += 0.22f * c.habla.enfasis * sinf((float)c.t * 0.06f);

    // Cejas en V
    Pose pc = pose;
    pc.dy += cejaDy;
    punto(l, 0, 0, 1.0f, pc);
    punto(l, 4, 0, 1.0f, pc);
    if (cejaInterna > 0.0f) {
        punto(l, 1, 0, cejaInterna, pc);
        punto(l, 3, 0, cejaInterna, pc);
    }

    ojoPunto(l, c, 1, 1, ap, 1.0f, pose);
    ojoPunto(l, c, 3, 1, ap, 1.0f, pose);

    // Dientes apretados: .XXX. / XX.XX. Al hablar ladra: el centro de abajo
    // se abre y el de arriba se apaga (la boca "golpea" hacia abajo).
    float b = c.habla.apertura;
    rasgo(l, LABIO, 3, 0.8f, pose);
    if (b > 0.03f) punto(l, 2, 3, 0.8f * (1.0f - 0.6f * b), pose);
    float r = 1.0f;
    if (rechina > 0.0f)
        r = 1.0f - 0.5f * rechina * (0.5f + 0.5f * sinf(6.2831853f * 8.0f * te));
    punto(l, 0, 4, 0.75f * r, pose);
    punto(l, 1, 4, 0.75f, pose);
    punto(l, 3, 4, 0.75f, pose);
    punto(l, 4, 4, 0.75f * (rechina > 0.0f ? 2.0f - r : 1.0f), pose);
    if (b > 0.03f) punto(l, 2, 4, 0.9f * frena(b), pose);
}

const Personalidad PERS_ENOJADO = {
    // parpadeo  cv   cierre sosten abre doble  mirada cv   rX   rY    sX  sY     centro hz   zeta parpado micro
    { 9500, 0.6f,   50,   25,  100, 0.03f, 3200, 0.5f, 0.35f, 0.2f, 0, 0.1f, 0.85f, 9.0f, 0.7f, 0.85f, 0.05f },
    // silaba cv    sil/pal hueco pausa min max  ampMin ampMax enfasis
    {  150, 0.25f, 2.2f,   60,   400,  2,  6,  0.6f,  1.0f,  0.35f },
    GESTOS, NG_ENOJADO, 3800, 0.5f,
    0.4f, 0.22f,       // respira rapido y fuerte
    1.0f,
    0.0f,
    G_TEMBLOR,
    R_GRUNIDO,         // "no me toques"
    dibujar
};
