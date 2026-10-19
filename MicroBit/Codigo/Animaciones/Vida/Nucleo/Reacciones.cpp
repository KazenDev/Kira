/**
 * Reacciones.cpp - Las REACCIONES a los sentidos (tocar, sacudir, golpe...)
 *
 * Son caras completas que se FUNDEN encima de la emocion de base (Cara.cpp
 * maneja el peso: entran en 120 ms y se van en 350 ms). Cada una es funcion
 * del tiempo desde que empezo, igual que todo el motor.
 *
 *   COSQUILLAS  ojos ^^ y boca abierta riendo "ja-ja-ja" (~6,5 Hz) que rebota
 *   CARINO      un corazon que late (lub-dub)
 *   GRUNIDO     cejas de enojo y la cabeza que dice "no"
 *   SOBRESALTO  ojos enormes, boca "O" y un salto para arriba
 *   DESPERTAR   ojos cerrados que se abren de golpe y se acomodan
 *   MAREO       ojos que giran en circulos opuestos y boca ondulada
 *   DORMIR      parpados cerrados, respiracion lenta y una "z" que sube
 *
 * Animo (como la tratan):
 *   TOQUE       la picaron: "¿mm?", levanta una ceja
 *   MOLESTA     "¡ey!": ceno y la cabeza que dice no
 *   BERRINCHE   el volcan: tiembla entera, grita y le sale humo
 *   OFENDIDA    "hmph": cierra los ojos y gira la cara
 *   ENAMORADA   un corazon chiquito que crece y late, con brillitos
 *   PERDON      ojos ^^ y sonrisa: te perdono
 *
 * Clima y entorno:
 *   ESTORNUDO   "a-a-a..." (se le cierran los ojos) "¡CHIS!" (y salpica)
 *   TIRITAR     "brrr": tiembla y le castanetean los dientes
 *   ACALORADA   jadea con la boca abierta, se abanica y suda
 *   DESLUMBRADA volvio la luz: entrecierra los ojos y se acostumbra
 *   CAIDA       "¡aaah!": ojos enormes, boca de grito
 */
#include "Cara.h"

uint32_t duracionReaccion(uint8_t r)
{
    switch (r) {
        case R_COSQUILLAS: return 1700;
        case R_CARINO:     return 2600;
        case R_GRUNIDO:    return 1100;
        case R_SOBRESALTO: return 1200;
        case R_DESPERTAR:  return 1500;
        case R_MAREO:      return 2600;
        case R_TOQUE:      return 700;
        case R_MOLESTA:    return 1200;
        case R_BERRINCHE:  return 2400;
        case R_OFENDIDA:   return 1400;
        case R_ENAMORADA:  return 2800;
        case R_PERDON:     return 1600;
        case R_ESTORNUDO:  return 1700;
        case R_TIRITAR:    return 1600;
        case R_ACALORADA:  return 2400;
        case R_DESLUMBRADA: return 1500;
        case R_CAIDA:      return 1500;
        default:           return 0;    // DORMIR: hasta que alguien la despierte
    }
}

static void px(Lienzo &l, float x, float y, float v, float dx, float dy)
{
    lienzoPunto(l, x + dx, y + dy, v);
}

static void cosquillas(Lienzo &l, float te)
{
    // La risa es fuerte el primer segundo y despues queda la sonrisa
    float fuerza = 1.0f - suave((te - 0.9f) / 0.8f);
    float risa = 0.5f + 0.5f * sinf(6.2831853f * 6.5f * te);
    float dy = -0.3f * fabsf(sinf(3.14159265f * 6.5f * te)) * fuerza;

    // Ojos ^ ^
    px(l, 1, 0, 1.0f, 0, dy);  px(l, 3, 0, 1.0f, 0, dy);
    px(l, 0, 1, 0.9f, 0, dy);  px(l, 2, 1, 0.9f, 0, dy);  px(l, 4, 1, 0.9f, 0, dy);
    // Boca abierta: la fila de arriba fija, la de abajo se abre con cada "ja"
    for (int x = 0; x < 5; x++) px(l, (float)x, 3, x == 0 || x == 4 ? 0.7f : 0.9f, 0, dy);
    float abajo = 0.3f + 0.7f * (fuerza * risa + (1.0f - fuerza) * 0.6f);
    px(l, 1, 4, abajo, 0, dy);  px(l, 2, 4, abajo, 0, dy);  px(l, 3, 4, abajo, 0, dy);
}

static void carino(Lienzo &l, float te)
{
    // Latido lub-dub cada 0,9 s
    float f = fmodf(te, 0.9f);
    float latido = 0.0f;
    if (f < 0.12f)                    latido = frena(f / 0.12f);
    else if (f < 0.20f)               latido = 1.0f - 0.6f * ((f - 0.12f) / 0.08f);
    else if (f < 0.30f)               latido = 0.4f + 0.5f * frena((f - 0.20f) / 0.10f);
    else                              latido = 0.9f * (1.0f - suave((f - 0.30f) / 0.6f));
    float v = 0.45f + 0.55f * latido;

    static const uint8_t CORAZON[16][2] = {
        {1,0},{3,0},
        {0,1},{1,1},{2,1},{3,1},{4,1},
        {0,2},{1,2},{2,2},{3,2},{4,2},
        {1,3},{2,3},{3,3},
        {2,4}
    };
    for (int i = 0; i < 16; i++) {
        float borde = (CORAZON[i][0] == 0 || CORAZON[i][0] == 4 || CORAZON[i][1] == 4) ? 0.85f : 1.0f;
        px(l, CORAZON[i][0], CORAZON[i][1], v * borde, 0, 0);
    }
}

static void grunido(Lienzo &l, float te, float dur)
{
    float q = te / dur;
    float dx = 0.55f * sinf(6.2831853f * 3.2f * te) * (1.0f - suave(q));
    // Cejas \ /, ojos, dientes apretados
    px(l, 0, 0, 1.0f, dx, 0);  px(l, 4, 0, 1.0f, dx, 0);
    px(l, 1, 1, 1.0f, dx, 0);  px(l, 3, 1, 1.0f, dx, 0);
    px(l, 1, 3, 0.8f, dx, 0);  px(l, 2, 3, 0.8f, dx, 0);  px(l, 3, 3, 0.8f, dx, 0);
    float diente = 0.5f + 0.5f * fabsf(sinf(6.2831853f * 4.0f * te));
    px(l, 0, 4, 0.8f, dx, 0);  px(l, 2, 4, 0.8f * diente, dx, 0);  px(l, 4, 4, 0.8f, dx, 0);
}

static void sobresalto(Lienzo &l, float te)
{
    // Salto para arriba y cae con un rebotito
    float dy = -0.85f * campana(te / 0.45f, 0.25f);
    float flash = 1.0f - 0.25f * suave(te / 0.5f);
    float tiembla = te < 0.4f ? 0.15f * sinf(6.2831853f * 18.0f * te) : 0.0f;
    // Ojos enormes (dos pixeles de alto)
    for (int y = 0; y < 2; y++) {
        px(l, 1, (float)y, flash, tiembla, dy);
        px(l, 3, (float)y, flash, -tiembla, dy);
    }
    // Boca "O"
    float o = 0.85f * flash;
    px(l, 2, 2.6f, o, 0, dy);  px(l, 1, 3.4f, o, 0, dy);
    px(l, 3, 3.4f, o, 0, dy);  px(l, 2, 4.2f, o, 0, dy);
}

static void despertar(Lienzo &l, float te)
{
    if (te < 0.25f) {
        // Todavia dormida: parpados cerrados
        px(l, 0, 1, 0.15f, 0, 0);  px(l, 1, 1, 0.15f, 0, 0);
        px(l, 3, 1, 0.15f, 0, 0);  px(l, 4, 1, 0.15f, 0, 0);
        px(l, 2, 3, 0.3f, 0, 0);
        return;
    }
    float q = (te - 0.25f);
    // Ojos que se abren de golpe (altos) y despues se achican a lo normal
    float alto = 1.0f - suave((q - 0.35f) / 0.5f);
    float dy = -0.4f * campana(q / 0.4f, 0.3f);
    // Dos parpadeos rapidos para "enfocar"
    float parp = 1.0f;
    if (q > 0.55f && q < 0.67f) parp = 0.1f;
    if (q > 0.85f && q < 0.95f) parp = 0.3f;
    px(l, 1, 1, parp, 0, dy);
    px(l, 3, 1, parp, 0, dy);
    px(l, 1, 0, alto * parp, 0, dy);
    px(l, 3, 0, alto * parp, 0, dy);
    px(l, 2, 3, 0.7f, 0, dy);
    px(l, 2, 4, 0.5f * alto, 0, dy);
}

static void mareo(Lienzo &l, const EstadoCara &c, float te)
{
    // Gira rapido mientras la sacuden y se va frenando al soltarla
    float resto = 1.0f;
    if (c.reaccionFin) {
        int32_t falta = (int32_t)(c.reaccionFin - c.t);
        resto = limitar((float)falta / (float)duracionReaccion(R_MAREO), 0.0f, 1.0f);
    }
    float vueltas = (0.6f + 1.6f * resto) * te;
    float a = 6.2831853f * vueltas;
    float r = 0.6f;
    float dx = 0.35f * sinf(6.2831853f * 0.9f * te);
    // Los dos ojos giran en sentidos OPUESTOS: cara de mareado
    px(l, 1.0f + r * cosf(a),  1.0f + r * sinf(a), 1.0f, dx, 0);
    px(l, 3.0f + r * cosf(-a + 1.0f), 1.0f + r * sinf(-a + 1.0f), 1.0f, dx, 0);
    // Boca ondulada
    for (int x = 0; x < 5; x++) {
        float y = 3.55f + 0.45f * sinf((float)x * 1.5f + te * 7.0f);
        px(l, (float)x, y, 0.75f, dx, 0);
    }
}

static void dormir(Lienzo &l, float te)
{
    float resp = 0.5f + 0.5f * sinf(6.2831853f * 0.2f * te);   // respira lento
    float parp = 0.10f + 0.06f * resp;
    px(l, 0, 1, parp, 0, 0);  px(l, 1, 1, parp, 0, 0);
    px(l, 3, 1, parp, 0, 0);  px(l, 4, 1, parp, 0, 0);
    px(l, 2, 3, 0.12f + 0.18f * resp, 0, 0);

    // Una "z" (un puntito) que sube en diagonal cada 2,8 s
    float f = fmodf(te, 2.8f);
    if (f < 1.8f) {
        float q = f / 1.8f;
        float v = 0.45f * campana(q, 0.3f);
        px(l, 3.4f + q * 1.0f, 2.3f - q * 2.5f, v, 0, 0);
    }
}

static void toque(Lienzo &l, const EstadoCara &c, float te)
{
    float q = te / 0.7f;
    // Mas fuerte si es el segundo pinchazo
    float fuerza = c.picadas >= 2 ? 1.0f : 0.6f;
    float dy = -0.35f * fuerza * campana(q, 0.2f);
    px(l, 1, 1, 1.0f, 0, dy);
    px(l, 3, 1, 1.0f, 0, dy);
    px(l, 3, 0, 0.7f * campana(q, 0.3f), 0, dy);     // ceja levantada: "¿eh?"
    px(l, 2, 3, 0.75f, 0, dy);                        // boquita "o"
}

static void molesta(Lienzo &l, float te)
{
    float q = te / 1.2f;
    float dx = 0.35f * sinf(6.2831853f * 5.0f * te) * (1.0f - suave(q));
    float flash = 1.0f - 0.2f * suave(q / 0.3f);
    px(l, 0, 0, flash, dx, 0);  px(l, 4, 0, flash, dx, 0);
    px(l, 1, 0.45f, 0.6f, dx, 0);  px(l, 3, 0.45f, 0.6f, dx, 0);   // ceno
    px(l, 1, 1, flash, dx, 0);  px(l, 3, 1, flash, dx, 0);
    float grr = 0.5f + 0.5f * fabsf(sinf(6.2831853f * 6.0f * te));
    px(l, 1, 3, 0.8f, dx, 0);  px(l, 2, 3, 0.8f * grr, dx, 0);  px(l, 3, 3, 0.8f, dx, 0);
}

static void berrinche(Lienzo &l, float te)
{
    float q = te / 2.4f;
    float furia = 1.0f - suave((q - 0.7f) / 0.3f);
    float dx = 0.45f * furia * sinf(6.2831853f * 11.0f * te);
    float dy = -0.25f * furia * fabsf(sinf(6.2831853f * 2.5f * te));
    // Ceno en barra, ojos encendidos
    for (int x = 0; x < 5; x++) px(l, (float)x, 0, x == 2 ? 0.0f : 1.0f, dx, dy);
    px(l, 1, 1, 1.0f, dx, dy);  px(l, 3, 1, 1.0f, dx, dy);
    // Boca de grito que tiembla
    float grito = 0.6f + 0.4f * fabsf(sinf(6.2831853f * 7.0f * te));
    px(l, 1, 3, grito, dx, dy);  px(l, 2, 3, grito, dx, dy);  px(l, 3, 3, grito, dx, dy);
    px(l, 1, 4, grito, dx, dy);  px(l, 3, 4, grito, dx, dy);  px(l, 2, 4, 0.5f * grito, dx, dy);
    // Humo: dos bocanadas que suben por los costados
    for (int k = 0; k < 2; k++) {
        float f = fmodf(te + k * 0.35f, 0.7f) / 0.7f;
        float x = k == 0 ? 0.0f : 4.0f;
        px(l, x, 2.2f - 2.6f * f, 0.5f * campana(f, 0.3f) * furia, 0, 0);
    }
}

static void ofendida(Lienzo &l, const EstadoCara &c, float te)
{
    float q = te / 1.4f;
    // Gira la cara (hacia el lado de su enojo) y levanta el menton
    float giro = c.ofendidaLado * 0.7f * campana(q, 0.25f);
    float dy = -0.3f * campana(q, 0.25f);
    // Ojos cerrados: dos rayitas
    px(l, 0, 1, 0.65f, giro, dy);  px(l, 1, 1, 0.65f, giro, dy);
    px(l, 3, 1, 0.65f, giro, dy);  px(l, 4, 1, 0.65f, giro, dy);
    // Trompita
    px(l, 2, 3, 0.8f, giro, dy);
    px(l, 1.5f, 3.4f, 0.45f, giro, dy);
}

static void enamorada(Lienzo &l, float te)
{
    float q = te / 2.8f;
    static const uint8_t CHICO[6][2] = { {1,1},{3,1},{1,2},{2,2},{3,2},{2,3} };
    static const uint8_t GRANDE[16][2] = {
        {1,0},{3,0},
        {0,1},{1,1},{2,1},{3,1},{4,1},
        {0,2},{1,2},{2,2},{3,2},{4,2},
        {1,3},{2,3},{3,3},
        {2,4}
    };
    float f = fmodf(te, 0.8f);
    float latido = f < 0.12f ? frena(f / 0.12f) : 1.0f - suave((f - 0.12f) / 0.6f);
    if (q < 0.25f) {
        float v = frena(q / 0.25f);
        for (int i = 0; i < 6; i++) px(l, CHICO[i][0], CHICO[i][1], v, 0, 0);
    } else {
        float v = 0.5f + 0.5f * latido;
        for (int i = 0; i < 16; i++) px(l, GRANDE[i][0], GRANDE[i][1], v, 0, 0);
        // Brillitos en las esquinas libres
        float b1 = sinf(6.2831853f * 3.0f * te) > 0.6f ? 0.6f : 0.0f;
        float b2 = sinf(6.2831853f * 3.0f * te + 2.0f) > 0.6f ? 0.6f : 0.0f;
        px(l, 0, 0, b1, 0, 0);  px(l, 4, 0, b2, 0, 0);
        px(l, 0, 4, b2, 0, 0);  px(l, 4, 4, b1, 0, 0);
    }
}

static void perdon(Lienzo &l, float te)
{
    float q = te / 1.6f;
    float dy = -0.25f * fabsf(sinf(3.14159265f * 3.0f * te)) * (1.0f - q);
    px(l, 1, 0, 1.0f, 0, dy);  px(l, 3, 0, 1.0f, 0, dy);
    px(l, 0, 1, 0.85f, 0, dy); px(l, 2, 1, 0.85f, 0, dy); px(l, 4, 1, 0.85f, 0, dy);
    px(l, 0, 3, 0.8f, 0, dy);  px(l, 4, 3, 0.8f, 0, dy);
    px(l, 1, 4, 0.85f, 0, dy); px(l, 2, 4, 0.85f, 0, dy); px(l, 3, 4, 0.85f, 0, dy);
}

static void estornudo(Lienzo &l, float te)
{
    float q = te / 1.7f;
    if (q < 0.55f) {
        // "a... a... a...": se le cierran los ojos, abre la boca y se echa
        // para atras, cada vez mas
        float w = q / 0.55f;
        float dy = -0.35f * w;
        float ojo = 1.0f - 0.75f * w;
        px(l, 1, 1 + 0.3f * w, ojo, 0, dy);
        px(l, 3, 1 + 0.3f * w, ojo, 0, dy);
        px(l, 2, 3, 0.4f + 0.5f * w, 0, dy);
        px(l, 1, 3.3f, 0.5f * w, 0, dy);
        px(l, 3, 3.3f, 0.5f * w, 0, dy);
    } else if (q < 0.7f) {
        // "¡CHIS!": golpe para adelante, todo prendido, y salpica
        float w = (q - 0.55f) / 0.15f;
        float dy = 0.5f * frena(w);
        px(l, 0, 1, 1.0f, 0, dy);  px(l, 1, 1, 1.0f, 0, dy);
        px(l, 3, 1, 1.0f, 0, dy);  px(l, 4, 1, 1.0f, 0, dy);
        px(l, 2, 3, 1.0f, 0, dy);
        px(l, 2.0f - 2.0f * w, 3.6f + 0.6f * w, 0.5f, 0, 0);
        px(l, 2.0f + 2.0f * w, 3.6f + 0.6f * w, 0.5f, 0, 0);
        px(l, 2.0f, 4.0f, 0.4f * (1.0f - w), 0, 0);
    } else {
        // Se recupera: parpadea y sorbe
        float w = (q - 0.7f) / 0.3f;
        float ojo = (w > 0.3f && w < 0.45f) ? 0.15f : 1.0f;
        px(l, 1, 1, ojo, 0, 0);
        px(l, 3, 1, ojo, 0, 0);
        px(l, 1, 3, 0.6f, 0, 0);  px(l, 2, 3, 0.6f, 0, 0);  px(l, 3, 3, 0.6f, 0, 0);
    }
}

static void tiritar(Lienzo &l, float te)
{
    float q = te / 1.6f;
    float fuerza = campana(q, 0.15f);
    float dx = 0.3f * fuerza * sinf(6.2831853f * 12.0f * te);
    px(l, 1, 1, 0.85f, dx, 0);
    px(l, 3, 1, 0.85f, dx, 0);
    // Dientes que castanetean: los de los costados y el del medio alternan
    bool fase = sinf(6.2831853f * 10.0f * te) > 0.0f;
    px(l, 1, 3, fase ? 0.85f : 0.3f, dx, 0);
    px(l, 3, 3, fase ? 0.85f : 0.3f, dx, 0);
    px(l, 2, 3, fase ? 0.3f : 0.85f, dx, 0);
    px(l, 1, 4, 0.4f, dx, 0);  px(l, 2, 4, 0.4f, dx, 0);  px(l, 3, 4, 0.4f, dx, 0);
}

static void acalorada(Lienzo &l, float te)
{
    // Se abanica (se mece) y jadea "ha-ha-ha" con la boca abierta
    float dx = 0.25f * sinf(6.2831853f * 1.2f * te);
    float jadeo = 0.5f + 0.5f * sinf(6.2831853f * 4.0f * te);
    px(l, 1, 1.3f, 0.6f, dx, 0);
    px(l, 3, 1.3f, 0.6f, dx, 0);
    px(l, 1, 3, 0.8f, dx, 0);  px(l, 2, 3, 0.8f, dx, 0);  px(l, 3, 3, 0.8f, dx, 0);
    px(l, 2, 4, 0.25f + 0.6f * jadeo, dx, 0);          // la lengua afuera
    // Gotas por los dos costados
    float f = fmodf(te, 1.2f) / 1.2f;
    px(l, 0, 0.2f + 3.0f * acelera(f), 0.45f * campana(f, 0.25f), 0, 0);
    float g = fmodf(te + 0.6f, 1.2f) / 1.2f;
    px(l, 4, 0.2f + 3.0f * acelera(g), 0.45f * campana(g, 0.25f), 0, 0);
}

static void deslumbrada(Lienzo &l, float te)
{
    float q = te / 1.5f;
    // Al principio: ojos apretados (rayitas) y todo muy prendido; despues
    // los ojos se abren de a poco
    float apriete = 1.0f - suave((q - 0.2f) / 0.6f);
    float flash = 1.0f - 0.3f * suave(q / 0.4f);
    px(l, 0, 1, 0.8f * apriete * flash, 0, 0);
    px(l, 4, 1, 0.8f * apriete * flash, 0, 0);
    px(l, 1, 1, flash, 0, 0);
    px(l, 3, 1, flash, 0, 0);
    px(l, 1, 0, 0.6f * apriete, 0, 0);
    px(l, 3, 0, 0.6f * apriete, 0, 0);
    // Boca torcida "ugh"
    px(l, 1, 3.3f, 0.7f, 0, 0);  px(l, 2, 3, 0.7f, 0, 0);  px(l, 3, 3.3f, 0.7f, 0, 0);
}

static void caida(Lienzo &l, float te)
{
    float q = te / 1.5f;
    float tiembla = 0.18f * sinf(6.2831853f * 15.0f * te) * (1.0f - suave(q));
    float dy = -0.3f * sinf(6.2831853f * 2.0f * te) * (1.0f - q);
    // Ojos enormes
    px(l, 1, 0, 1.0f, tiembla, dy);  px(l, 1, 1, 1.0f, tiembla, dy);
    px(l, 3, 0, 1.0f, -tiembla, dy); px(l, 3, 1, 1.0f, -tiembla, dy);
    // Boca de grito grande
    float g = 0.8f;
    px(l, 1, 2.6f, g, 0, dy);  px(l, 2, 2.6f, g, 0, dy);  px(l, 3, 2.6f, g, 0, dy);
    px(l, 1, 3.5f, g, 0, dy);  px(l, 3, 3.5f, g, 0, dy);
    px(l, 1, 4.3f, g, 0, dy);  px(l, 2, 4.3f, g, 0, dy);  px(l, 3, 4.3f, g, 0, dy);
}

void dibujarReaccion(Lienzo &l, const EstadoCara &c, uint8_t r, uint32_t desde)
{
    float te = (float)(c.t - desde) * 0.001f;
    switch (r) {
        case R_COSQUILLAS: cosquillas(l, te); break;
        case R_CARINO:     carino(l, te); break;
        case R_GRUNIDO:    grunido(l, te, (float)duracionReaccion(R_GRUNIDO) * 0.001f); break;
        case R_SOBRESALTO: sobresalto(l, te); break;
        case R_DESPERTAR:  despertar(l, te); break;
        case R_MAREO:      mareo(l, c, te); break;
        case R_DORMIR:     dormir(l, te); break;
        case R_TOQUE:      toque(l, c, te); break;
        case R_MOLESTA:    molesta(l, te); break;
        case R_BERRINCHE:  berrinche(l, te); break;
        case R_OFENDIDA:   ofendida(l, c, te); break;
        case R_ENAMORADA:  enamorada(l, te); break;
        case R_PERDON:     perdon(l, te); break;
        case R_ESTORNUDO:  estornudo(l, te); break;
        case R_TIRITAR:    tiritar(l, te); break;
        case R_ACALORADA:  acalorada(l, te); break;
        case R_DESLUMBRADA: deslumbrada(l, te); break;
        case R_CAIDA:      caida(l, te); break;
        default: break;
    }
}
