/**
 * Ojos.cpp - Parpadeo y mirada procedurales (ver Ojos.h)
 */
#include "Ojos.h"

// Direcciones de las sacadas con su peso (Lee, Badler & Badler 2002: las
// cuatro cardinales suman ~70%, cada diagonal ~7%).
static const float DIR_X[8] = { 1, 0, -1, 0, 0.7071f, -0.7071f, -0.7071f, 0.7071f };
static const float DIR_Y[8] = { 0, -1, 0, 1, -0.7071f, -0.7071f, 0.7071f, 0.7071f };
static const uint8_t DIR_PESO[8] = { 17, 18, 17, 20, 7, 7, 7, 7 };

static int sortearDireccion(Azar &azar)
{
    int r = azarEntero(azar, 100);
    for (int i = 0; i < 8; i++) {
        if (r < DIR_PESO[i]) return i;
        r -= DIR_PESO[i];
    }
    return 0;
}

static void programarParpadeo(EstadoOjos &e, const ParamOjos &p, uint32_t t,
                              bool hablando, Azar &azar)
{
    // Hablando se parpadea mas (Doughty 2001: conversacion 10-32/min contra
    // 8-21/min en silencio).
    float media = p.parpadeoMs * (hablando ? 0.72f : 1.0f);
    e.proxParpadeo = t + (uint32_t)azarLogNormal(azar, media, p.parpadeoCv);
}

void ojosParpadear(EstadoOjos &e, uint32_t t, Azar &azar)
{
    if (e.parpadeando) return;
    e.parpadeando = true;
    e.inicioParpadeo = t;
    e.fasesListas = false;
    // La mitad de los parpadeos espontaneos son incompletos (Kwon 2013)
    e.profundidad = azarProb(azar, 0.3f) ? azarRango(azar, 0.55f, 0.8f) : 1.0f;
}

void ojosNoParpadearHasta(EstadoOjos &e, uint32_t t)
{
    e.sinParpadearHasta = t;
}

void ojosMirar(EstadoOjos &e, float x, float y, uint32_t t, uint32_t ms)
{
    e.objX = x;
    e.objY = y;
    e.microX = e.microY = 0.0f;
    e.proxMirada = t + ms;
    e.proxMicro = t + ms / 2;
}

void ojosIniciar(EstadoOjos &e, const ParamOjos &p, uint32_t t, Azar &azar)
{
    e.mx.x = p.sesgoX; e.mx.v = 0.0f;
    e.my.x = p.sesgoY; e.my.v = 0.0f;
    e.objX = p.sesgoX;
    e.objY = p.sesgoY;
    e.microX = e.microY = 0.0f;
    e.ultX[0] = e.ultX[1] = e.objX;
    e.ultY[0] = e.ultY[1] = e.objY;
    e.proxMirada = t + (uint32_t)azarRango(azar, 300.0f, 900.0f);
    e.proxMicro = t + 500;

    e.parpadeando = false;
    e.inicioParpadeo = 0;
    e.cierre = p.cierreMs;
    e.sosten = p.sostenMs;
    e.abre = p.aperturaMs;
    e.profundidad = 1.0f;
    e.fasesListas = true;
    e.dobleTrasEste = false;
    e.sinParpadearHasta = t;
    e.ultimoFinParpadeo = t;
    programarParpadeo(e, p, t, false, azar);

    e.x = e.mx.x;
    e.y = e.my.x;
    e.apertura = p.parpado;
    e.cierreParpadeo = 0.0f;
    e.saltoGrande = false;
}

static void sortearMirada(EstadoOjos &e, const ParamOjos &p, uint32_t t,
                          bool hablando, Azar &azar)
{
    float tx, ty;
    // Al hablar se aparta la vista mas seguido (hablante ~60% mirando,
    // oyente ~80%: Maran 2020, Schmalzle 2024), pero las apartadas son
    // cortas (0,5-1 s, Ishi 2021).
    float centro = p.centroProb - (hablando ? 0.15f : 0.0f);
    bool alCentro = azarProb(azar, centro);

    for (int intento = 0; intento < 3; intento++) {
        if (alCentro) {
            tx = p.sesgoX;
            ty = p.sesgoY;
        } else {
            // Magnitud exponencial: saltos chicos frecuentes, grandes raros
            float m = -0.6f * logf(1.0f - azarF(azar) * 0.95f);
            if (m > 1.0f) m = 1.0f;
            if (m < 0.35f) m = 0.35f + azarF(azar) * 0.25f;
            int d = sortearDireccion(azar);
            tx = p.sesgoX + DIR_X[d] * m * p.rangoX;
            ty = p.sesgoY + DIR_Y[d] * m * p.rangoY;
        }
        tx = limitar(tx, -1.0f, 1.0f);
        ty = limitar(ty, -1.0f, 1.0f);
        // Nunca dos veces seguidas al mismo lugar (salvo volver al centro)
        float d0 = fabsf(tx - e.ultX[0]) + fabsf(ty - e.ultY[0]);
        float d1 = fabsf(tx - e.ultX[1]) + fabsf(ty - e.ultY[1]);
        if (alCentro || (d0 > 0.18f && d1 > 0.18f)) break;
    }

    float salto = fabsf(tx - e.objX) + fabsf(ty - e.objY);
    e.saltoGrande = salto > 0.6f;

    e.ultX[1] = e.ultX[0]; e.ultY[1] = e.ultY[0];
    e.ultX[0] = tx;        e.ultY[0] = ty;
    e.objX = tx;
    e.objY = ty;
    e.microX = e.microY = 0.0f;

    float espera = azarLogNormal(azar, p.miradaMs, p.miradaCv);
    if (hablando && !alCentro) espera *= 0.45f;   // apartadas cortas
    e.proxMirada = t + (uint32_t)espera;
}

void ojosAvanzar(EstadoOjos &e, const ParamOjos &p, uint32_t t, float dt,
                 bool hablando, float biasX, float biasY, Azar &azar)
{
    e.saltoGrande = false;

    // --- Mirada ---------------------------------------------------------
    if ((int32_t)(t - e.proxMirada) >= 0) {
        sortearMirada(e, p, t, hablando, azar);
        // Parpadeo evocado por la mirada: acompana a ~la mitad de los
        // saltos grandes (Evinger 1994).
        if (e.saltoGrande && azarProb(azar, 0.5f) &&
            (int32_t)(t - e.sinParpadearHasta) >= 0 &&
            t - e.ultimoFinParpadeo > 350)
            ojosParpadear(e, t, azar);
    }
    if ((int32_t)(t - e.proxMicro) >= 0) {
        e.microX = azarRango(azar, -p.micro, p.micro);
        e.microY = azarRango(azar, -p.micro, p.micro) * 0.6f;
        e.proxMicro = t + (uint32_t)azarRango(azar, 500.0f, 1200.0f);
    }
    float ox = limitar(e.objX + e.microX + biasX, -1.0f, 1.0f);
    float oy = limitar(e.objY + e.microY + biasY, -1.0f, 1.0f);
    muelleAvanzar(e.mx, ox, dt, p.hz, p.zeta);
    muelleAvanzar(e.my, oy, dt, p.hz, p.zeta);
    e.x = limitar(e.mx.x, -1.25f, 1.25f);
    e.y = limitar(e.my.x, -1.25f, 1.25f);

    // --- Parpadeo -------------------------------------------------------
    if (!e.parpadeando && (int32_t)(t - e.proxParpadeo) >= 0) {
        if ((int32_t)(t - e.sinParpadearHasta) >= 0)
            ojosParpadear(e, t, azar);
        else
            e.proxParpadeo = e.sinParpadearHasta + 200;
    }

    float c = 0.0f;
    if (e.parpadeando) {
        if (!e.fasesListas) {
            e.fasesListas = true;
            // Recien empezado: sortear sus fases (+-20%), asi ni dos
            // parpadeos duran lo mismo.
            e.cierre = p.cierreMs * azarRango(azar, 0.8f, 1.2f);
            e.sosten = p.sostenMs * azarRango(azar, 0.7f, 1.3f);
            e.abre   = p.aperturaMs * azarRango(azar, 0.8f, 1.2f);
            e.dobleTrasEste = azarProb(azar, p.dobleProb);
        }
        float trans = (float)(t - e.inicioParpadeo);
        if (trans < e.cierre)
            c = acelera(trans / e.cierre);
        else if (trans < e.cierre + e.sosten)
            c = 1.0f;
        else if (trans < e.cierre + e.sosten + e.abre)
            c = 1.0f - frena((trans - e.cierre - e.sosten) / e.abre);
        else {
            c = 0.0f;
            e.parpadeando = false;
            e.ultimoFinParpadeo = t;
            if (e.dobleTrasEste) {
                e.dobleTrasEste = false;
                e.proxParpadeo = t + (uint32_t)azarRango(azar, 90.0f, 160.0f);
            } else {
                programarParpadeo(e, p, t, hablando, azar);
            }
        }
        c *= e.profundidad;
    }
    e.cierreParpadeo = c;
    e.apertura = p.parpado * (1.0f - c);
}
