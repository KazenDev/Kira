/**
 * Habla.cpp - Prosodia sintetica de la boca (ver Habla.h)
 */
#include "Habla.h"
#include "Ruido.h"

enum FaseHabla { F_SILABA = 0, F_HUECO, F_PAUSA, F_RELLENO };

static int sortearSilabas(const ParamHabla &p, Azar &azar)
{
    // 1..4 silabas alrededor de la media (las palabras de 2-3 son las mas
    // comunes en espanol)
    int n = (int)(p.silabasPalabra + azarNormal(azar) * 0.9f + 0.5f);
    return n < 1 ? 1 : (n > 4 ? 4 : n);
}

static void nuevaSilaba(EstadoHabla &h, const ParamHabla &p, uint32_t t, Azar &azar)
{
    h.fase = F_SILABA;
    h.inicio = t;
    h.dur = limitar(azarLogNormal(azar, p.silabaMs, p.silabaCv),
                    p.silabaMs * 0.55f, p.silabaMs * 1.8f);
    h.pico = azarRango(azar, p.ampMin, p.ampMax);
    if (azarProb(azar, p.enfasisProb)) {
        h.pico = 1.0f;
        h.dur *= 1.25f;            // la silaba fuerte tambien es mas larga
        h.enfasis = 1.0f;
    }
}

static void nuevaFrase(EstadoHabla &h, const ParamHabla &p, Azar &azar)
{
    int span = (int)p.palabrasMax - (int)p.palabrasMin + 1;
    h.palabrasRestantes = p.palabrasMin + azarEntero(azar, span < 1 ? 1 : span);
    h.silabasRestantes = sortearSilabas(p, azar);
}

void hablaIniciar(EstadoHabla &h, const ParamHabla &p, uint32_t t, Azar &azar)
{
    h.activa = true;
    h.apertura = 0.0f;
    h.enfasis = 0.0f;
    h.finFrase = false;
    nuevaFrase(h, p, azar);
    nuevaSilaba(h, p, t, azar);
}

void hablaDetener(EstadoHabla &h)
{
    h.activa = false;
    h.finFrase = false;
}

void hablaAvanzar(EstadoHabla &h, const ParamHabla &p, uint32_t t, float dt, Azar &azar)
{
    h.finFrase = false;
    h.enfasis -= dt * 2.5f;
    if (h.enfasis < 0.0f) h.enfasis = 0.0f;

    if (!h.activa) {
        // La boca se cierra suave al callar (~100 ms), no de golpe
        h.apertura -= dt * 9.0f;
        if (h.apertura < 0.0f) h.apertura = 0.0f;
        return;
    }

    float trans = (float)(t - h.inicio);
    if (trans >= h.dur) {
        if (h.fase == F_SILABA) {
            h.silabasRestantes--;
            if (h.silabasRestantes > 0) {
                nuevaSilaba(h, p, t, azar);
            } else {
                h.palabrasRestantes--;
                h.inicio = t;
                if (h.palabrasRestantes > 0) {
                    h.fase = F_HUECO;
                    h.dur = p.huecoMs * azarRango(azar, 0.6f, 1.5f);
                } else if (azarProb(azar, 0.06f)) {
                    // "eeh..." una pausa llena: la boca queda entreabierta
                    h.fase = F_RELLENO;
                    h.dur = azarRango(azar, 130.0f, 300.0f);
                } else {
                    h.fase = F_PAUSA;
                    h.dur = limitar(azarLogNormal(azar, p.pausaMs, 0.35f),
                                    150.0f, 900.0f);
                    h.finFrase = true;
                }
            }
        } else {
            if (h.fase == F_PAUSA || h.fase == F_RELLENO)
                nuevaFrase(h, p, azar);
            else
                h.silabasRestantes = sortearSilabas(p, azar);
            nuevaSilaba(h, p, t, azar);
        }
        trans = 0.0f;
    }

    float obj;
    float q = trans / h.dur;
    switch (h.fase) {
        case F_SILABA:
            // Ataque rapido (35%) y cierre mas lento; entre silabas de la
            // misma palabra la boca no llega a cerrarse del todo.
            if (q < 0.35f) obj = h.pico * frena(q / 0.35f);
            else           obj = h.pico * (1.0f - 0.82f * suave((q - 0.35f) / 0.65f));
            break;
        case F_RELLENO: obj = 0.22f; break;
        default:        obj = 0.0f;  break;
    }
    // Suavizado de un frame: los cambios de silaba no "saltan"
    float k = dt * 30.0f;
    if (k > 1.0f) k = 1.0f;
    h.apertura += (obj - h.apertura) * k;
}
