/**
 * Cara.cpp - El motor de vida (ver Cara.h)
 */
#include "Cara.h"

// Aburrimiento: a los 60 s sin que nadie le haga caso empieza a hacer mas
// gestos; a los 150 s se duerme (Eilik: interactivo -> se entretiene sola ->
// duerme). Cualquier toque, sacudida, inclinacion o comando la despierta.
#define ABURRIDA_MS  60000u
#define DORMIDA_MS  150000u

// Fundido de las reacciones con la cara de base
#define REACCION_ENTRA_MS   120.0f
#define REACCION_SALE_MS    350.0f
#define DORMIR_ENTRA_MS    1600.0f   // dormirse es lento, no un corte

// --- Eventos -----------------------------------------------------------------

static void emitirGesto(EstadoCara &c, int i)
{
    c.pendEventos |= EV_GESTO;
    c.pendGesto = (int8_t)i;
}

static void emitirReaccion(EstadoCara &c, uint8_t r)
{
    c.pendEventos |= EV_REACCION;
    c.pendReaccion = r;
}

// --- Gestos ------------------------------------------------------------------

float caraAburrimiento(const EstadoCara &c)
{
    uint32_t quieta = c.t - c.ultimaActividad;
    if (quieta <= ABURRIDA_MS) return 0.0f;
    return limitar((float)(quieta - ABURRIDA_MS) / (float)(DORMIDA_MS - ABURRIDA_MS),
                   0.0f, 1.0f);
}

static void programarGesto(EstadoCara &c, uint32_t desde)
{
    // Aburrida hace el doble de cosas: se entretiene sola
    float media = c.p->gestoMs * (1.0f - 0.5f * caraAburrimiento(c));
    c.proxGesto = desde + (uint32_t)azarLogNormal(c.azar, media, c.p->gestoCv);
}

static void empezarGesto(EstadoCara &c, int i, uint32_t t)
{
    const DefGesto &g = c.p->gestos[i];
    c.gesto = (int8_t)i;
    c.gestoInicio = t;
    c.gestoDur = g.durMin;
    if (g.durMax > g.durMin)
        c.gestoDur += (uint32_t)azarEntero(c.azar, g.durMax - g.durMin + 1);
    if (c.gestoDur < 1) c.gestoDur = 1;
    c.gp = 0.0f;
    c.ultimoUso[i] = t ? t : 1;
    for (int k = 0; k < c.p->nGestos; k++) c.usos[k] *= 0.7f;
    c.usos[i] += 1.0f;
    c.ultimoGesto = (int8_t)i;
    emitirGesto(c, i);
}

// Sorteo por peso, con enfriamiento, penalizando lo usado hace poco y sin
// repetir el ultimo (Vector: "no volver a activarse demasiado pronto").
static int elegirGesto(EstadoCara &c, uint32_t t)
{
    float pesos[MAX_GESTOS];
    float total = 0.0f;
    int candidatos = 0;
    int n = c.p->nGestos;
    for (int i = 0; i < n; i++) {
        const DefGesto &g = c.p->gestos[i];
        bool ok = c.hablando ? (g.flags & G_HABLANDO) : (g.flags & G_REPOSO);
        if (ok && c.ultimoUso[i] != 0 && t - c.ultimoUso[i] < g.enfriarMs)
            ok = false;
        pesos[i] = ok ? (float)g.peso / (1.0f + c.usos[i]) : 0.0f;
        if (ok) candidatos++;
    }
    if (candidatos > 1 && c.ultimoGesto >= 0) pesos[c.ultimoGesto] = 0.0f;
    for (int i = 0; i < n; i++) total += pesos[i];
    if (total <= 0.0f) return -1;
    float r = azarF(c.azar) * total;
    for (int i = 0; i < n; i++) {
        if (pesos[i] <= 0.0f) continue;
        if (r < pesos[i]) return i;
        r -= pesos[i];
    }
    for (int i = n - 1; i >= 0; i--)
        if (pesos[i] > 0.0f) return i;
    return -1;
}

void caraForzarGesto(EstadoCara &c, int idx, uint32_t t)
{
    if (idx < 0 || idx >= c.p->nGestos) return;
    empezarGesto(c, idx, t);
}

// --- Reacciones --------------------------------------------------------------

static uint8_t prioridad(uint8_t r)
{
    switch (r) {
        case R_MAREO:
        case R_CAIDA:       return 5;
        case R_SOBRESALTO:
        case R_BERRINCHE:
        case R_ENAMORADA:   return 4;
        case R_CARINO:
        case R_PERDON:
        case R_MOLESTA:
        case R_DESLUMBRADA: return 3;
        case R_DORMIR:      return 1;
        case R_NINGUNA:     return 0;
        default:            return 2;
    }
}

static bool reaccionViva(const EstadoCara &c)
{
    if (c.reaccion == R_NINGUNA) return false;
    return c.reaccionFin == 0 || (int32_t)(c.t - c.reaccionFin) < 0;
}

static void reaccionar(EstadoCara &c, uint8_t r, uint32_t t)
{
    if (r == R_NINGUNA || r >= NUM_REACCIONES) return;
    if (r == R_DORMIR && c.hablando) return;
    bool viva = reaccionViva(c);
    if (viva && c.reaccion != r && c.reaccion != R_DORMIR &&
        prioridad(r) < prioridad(c.reaccion))
        return;

    uint32_t dur = duracionReaccion(r);
    if (viva && c.reaccion == r) {
        // La misma otra vez: se alarga, y si ya paso un rato vuelve a sonar
        if (dur) c.reaccionFin = t + dur;
        if (t - c.reaccionInicio > 700) {
            c.reaccionInicio = t;
            emitirReaccion(c, r);
        }
        return;
    }
    c.reaccion = r;
    c.reaccionInicio = t;
    c.reaccionFin = dur ? t + dur : 0;
    c.gesto = -1;
    emitirReaccion(c, r);
}

void caraForzarReaccion(EstadoCara &c, uint8_t r, uint32_t t)
{
    c.t = t;
    reaccionar(c, r, t);
}

void caraActividad(EstadoCara &c, uint32_t t)
{
    c.ultimaActividad = t;
    if (c.reaccion == R_DORMIR) {
        c.t = t;
        // Si se habia dormido por la oscuridad, vuelve a contar desde ahora
        c.dormidaPorOscuro = false;
        c.oscuroDesde = t;
        reaccionar(c, R_DESPERTAR, t);
    }
}

bool caraOfendida(const EstadoCara &c)
{
    return c.ofendidaHasta != 0 && (int32_t)(c.t - c.ofendidaHasta) < 0;
}

// --- Animo ---------------------------------------------------------------------
//
// El modelo de EMO: los mimos dejan FLORES y las molestias LLAMAS, y cada una
// se va sola a los 2 min. Tres flores = se enamora; tres llamas = berrinche y
// queda ofendida. Como en Eilik, las dos se compensan: un mimo apaga una
// llama (la caricia la calma) y una molestia se lleva una flor.

static void quitarPrimero(uint32_t *v, uint8_t &n)
{
    for (int i = 0; i + 1 < n; i++) v[i] = v[i + 1];
    if (n) n--;
}

static void envejecerAnimo(EstadoCara &c, uint32_t t)
{
    while (c.nFlores && t - c.flores[0] > ANIMO_DURA_MS) quitarPrimero(c.flores, c.nFlores);
    while (c.nLlamas && t - c.llamas[0] > ANIMO_DURA_MS) quitarPrimero(c.llamas, c.nLlamas);
    if (c.ofendidaHasta && (int32_t)(t - c.ofendidaHasta) >= 0) c.ofendidaHasta = 0;
}

static void sumarFlor(EstadoCara &c, uint32_t t)
{
    if (caraOfendida(c) && c.nLlamas == 0) {
        c.ofendidaHasta = 0;
        reaccionar(c, R_PERDON, t);
        return;
    }
    if (c.nLlamas) {
        quitarPrimero(c.llamas, c.nLlamas);
        if (caraOfendida(c) && c.nLlamas == 0) {
            c.ofendidaHasta = 0;
            reaccionar(c, R_PERDON, t);
        }
        return;
    }
    if (c.nFlores < ANIMO_MAX) c.flores[c.nFlores++] = t;
    if (c.nFlores >= ANIMO_MAX) {
        c.nFlores = 0;
        reaccionar(c, R_ENAMORADA, t);
    }
}

static void sumarLlama(EstadoCara &c, uint32_t t)
{
    if (c.nFlores) {
        quitarPrimero(c.flores, c.nFlores);
        return;
    }
    if (c.nLlamas < ANIMO_MAX) c.llamas[c.nLlamas++] = t;
    if (c.nLlamas >= ANIMO_MAX) {
        c.nLlamas = 0;
        c.ofendidaHasta = t + 25000;
        c.ofendidaLado = azarProb(c.azar, 0.5f) ? 1.0f : -1.0f;
        reaccionar(c, R_BERRINCHE, t);
    }
}

// --- Clima ---------------------------------------------------------------------

static void entrarClima(EstadoCara &c, uint8_t nuevo, uint32_t t)
{
    c.clima = nuevo;
    c.climaCandidato = nuevo;
    if (nuevo == CLIMA_CALOR) reaccionar(c, R_ACALORADA, t);
    if (nuevo == CLIMA_FRIO)  reaccionar(c, R_TIRITAR, t);
    c.proxClima = t + (uint32_t)azarRango(c.azar, 20000.0f, 45000.0f);
    c.proxGota = t + 2500;
}

void caraForzarClima(EstadoCara &c, int clima, uint32_t t)
{
    c.t = t;
    if (clima < 0 || clima > CLIMA_FRIO) {
        c.climaForzado = -1;   // vuelve el termometro
        c.climaDesde = t;
        return;
    }
    c.climaForzado = (int8_t)clima;
    if (c.clima != clima) entrarClima(c, (uint8_t)clima, t);
}

static void actualizarClima(EstadoCara &c, uint32_t t)
{
    uint8_t obj = CLIMA_NORMAL;
    if (c.climaForzado >= 0) {
        obj = (uint8_t)c.climaForzado;
    } else if (c.tempLista) {
        switch (c.clima) {
            case CLIMA_CALOR: obj = c.temp <= CALOR_SALE ? CLIMA_NORMAL : CLIMA_CALOR; break;
            case CLIMA_FRIO:  obj = c.temp >= FRIO_SALE  ? CLIMA_NORMAL : CLIMA_FRIO;  break;
            default:
                obj = c.temp >= CALOR_ENTRA ? CLIMA_CALOR :
                      (c.temp <= FRIO_ENTRA ? CLIMA_FRIO : CLIMA_NORMAL);
        }
    }
    if (obj == c.clima) {
        c.climaCandidato = obj;
    } else if (obj != c.climaCandidato) {
        c.climaCandidato = obj;
        c.climaDesde = t;
    } else if (t - c.climaDesde > 8000) {
        // 8 s seguidos del otro lado del umbral: el clima cambio de verdad
        entrarClima(c, obj, t);
    }

    // Reacciones de clima de vez en cuando (no mientras habla)
    if (c.clima != CLIMA_NORMAL && !c.hablando && c.reaccion == R_NINGUNA &&
        (int32_t)(t - c.proxClima) >= 0) {
        if (c.clima == CLIMA_FRIO) {
            // Se alternan casi siempre: dos estornudos seguidos son raros
            bool estornuda = c.ultimaDeFrio == R_ESTORNUDO ? azarProb(c.azar, 0.15f)
                                                            : azarProb(c.azar, 0.85f);
            c.ultimaDeFrio = estornuda ? (uint8_t)R_ESTORNUDO : (uint8_t)R_TIRITAR;
            reaccionar(c, c.ultimaDeFrio, t);
        } else
            reaccionar(c, R_ACALORADA, t);
        c.proxClima = t + (uint32_t)azarRango(c.azar, 30000.0f, 75000.0f);
    }
}

// --- Sentidos ----------------------------------------------------------------

static void sentir(EstadoCara &c, const Estimulos &e, uint32_t t)
{
    if (!c.sentidosActivos) {
        c.inclX *= 0.9f;
        c.inclY *= 0.9f;
        c.toqueAntes = false;
        c.sacudiendo = false;
        return;
    }

    // INCLINACION: se mide contra la pose de reposo, que se adapta sola en
    // ~3 s. Asi da igual si la placa esta acostada o parada en su soporte:
    // los ojos siguen el MOVIMIENTO y despues se acostumbran a la pose nueva.
    if (e.hayAcel) {
        if (!c.baseLista) {
            c.baseX = c.prevX = e.ax;
            c.baseY = c.prevY = e.ay;
            c.baseZ = c.prevZ = e.az;
            c.baseLista = true;
        }
        float tiron = fabsf(e.ax - c.prevX) + fabsf(e.ay - c.prevY) + fabsf(e.az - c.prevZ);
        c.prevX = e.ax; c.prevY = e.ay; c.prevZ = e.az;

        float k = limitar(c.dt * 0.35f, 0.0f, 1.0f);
        c.baseX += (e.ax - c.baseX) * k;
        c.baseY += (e.ay - c.baseY) * k;
        c.baseZ += (e.az - c.baseZ) * k;

        float dx = (e.ax - c.baseX) / 650.0f;
        float dy = (e.ay - c.baseY) / 650.0f;
        c.inclX = limitar(dx, -1.0f, 1.0f);
        c.inclY = limitar(dy, -1.0f, 1.0f);
        if (fabsf(dx) + fabsf(dy) > 0.35f)
            caraActividad(c, t);

        // GOLPE: un tiron brusco que no es una sacudida (la sacudida es
        // sostenida y la detecta CODAL)
        if (tiron > 1500.0f && !c.sacudiendo && t - c.ultimoGolpe > 2500) {
            c.ultimoGolpe = t;
            caraActividad(c, t);
            reaccionar(c, R_SOBRESALTO, t);
        }
    }

    // SACUDIDA sostenida -> mareo, que dura 2,6 s despues de soltarla
    if (e.sacudida) {
        if (!c.sacudiendo) {
            c.sacudiendo = true;
            c.sacudidaDesde = t;
        }
        caraActividad(c, t);
        if (t - c.sacudidaDesde > 250) {
            if (c.reaccion == R_MAREO && reaccionViva(c)) {
                c.reaccionFin = t + duracionReaccion(R_MAREO);
            } else {
                reaccionar(c, R_MAREO, t);
                if (c.reaccion == R_MAREO) sumarLlama(c, t);   // marearla molesta
            }
        }
    } else {
        c.sacudiendo = false;
    }

    // CAIDA LIBRE: la tiraron o se cayo
    if (e.caida && t - c.ultimaCaida > 2000) {
        c.ultimaCaida = t;
        caraActividad(c, t);
        reaccionar(c, R_CAIDA, t);
    }

    // TOQUE en el logo = mimos. El tercer toque seguido es carino (flor); si
    // le haces cosquillas sin parar, al sexto se harta (llama). Mantener el
    // dedo es una caricia: carino, y una flor por caricia.
    if (e.toque && !c.toqueAntes) {
        c.toqueDesde = t;
        c.caricia = false;
        if (t - c.ultimoToque > 8000) c.toquesSeguidos = 0;
        if (c.toquesSeguidos < 250) c.toquesSeguidos++;
        c.ultimoToque = t;
        bool dormida = c.reaccion == R_DORMIR;
        caraActividad(c, t);
        if (dormida) {
            sumarFlor(c, t);                      // despertarla con un mimo
        } else if (caraOfendida(c)) {
            reaccionar(c, R_OFENDIDA, t);         // "hmph" (acariciala)
        } else if (c.clima == CLIMA_FRIO) {
            reaccionar(c, R_CARINO, t);           // la abrigaste
            sumarFlor(c, t);
        } else if (c.toquesSeguidos >= 6) {
            reaccionar(c, R_MOLESTA, t);          // ya basta de cosquillas
            sumarLlama(c, t);
            c.toquesSeguidos = 0;
        } else if (c.toquesSeguidos == 3) {
            reaccionar(c, R_CARINO, t);
            sumarFlor(c, t);
        } else {
            reaccionar(c, c.p->reaccionToque, t);
        }
    }
    if (e.toque && t - c.toqueDesde > 1300) {
        if (!caraOfendida(c)) {
            if (c.reaccion == R_CARINO && reaccionViva(c)) {
                if (c.reaccionFin - t < 900) c.reaccionFin = t + 900;
            } else {
                reaccionar(c, R_CARINO, t);
            }
        }
        if (!c.caricia) {
            c.caricia = true;
            sumarFlor(c, t);                      // puede perdonarla o enamorarla
        }
    }
    c.toqueAntes = e.toque;

    // PICARLA con B: "¿mm?", "¿mm?", "¡ey!" (y cada "¡ey!" es una llama:
    // picarla cinco veces seguidas termina en berrinche)
    if (e.picar) {
        if (t - c.ultimaPicada > 4000) c.picadas = 0;
        if (c.picadas < 250) c.picadas++;
        c.ultimaPicada = t;
        bool dormida = c.reaccion == R_DORMIR;
        caraActividad(c, t);
        if (dormida)
            sumarLlama(c, t);                     // despertarla de un pinchazo
        else if (caraOfendida(c))
            reaccionar(c, R_OFENDIDA, t);
        else if (c.picadas <= 2)
            reaccionar(c, R_TOQUE, t);
        else {
            reaccionar(c, R_MOLESTA, t);
            sumarLlama(c, t);
        }
    }

    // TERMOMETRO
    if (e.hayTemp) {
        c.temp = e.temp;
        c.tempLista = true;
    }

    // LUZ: taparla (oscuridad sostenida) le da sueno y se duerme; cuando
    // vuelve la luz se despierta deslumbrada. La base se adapta sola a la
    // luz del lugar (feria, aula, casa).
    //
    // OJO: el sensor son los propios LEDs y es POCO sensible: con la luz
    // normal de una pieza de noche marca 0 (medido en la placa). "Oscuro" se
    // mide contra la base, y solo si la base alcanza para notar una mano
    // (LUZ_MINIMA). Si el lugar ya es oscuro para el sensor, esta reaccion
    // se apaga sola en vez de dormirla para siempre.
    if (e.hayLuz) {
        const float LUZ_MINIMA = 12.0f;
        float L = (float)e.luz;
        if (!c.luzLista) {
            c.luzBase = L;
            c.luzLista = true;
        }
        bool sirve = c.luzBase >= LUZ_MINIMA;
        bool osc = sirve && L < c.luzBase * 0.3f;
        if (!osc) c.luzBase += (L - c.luzBase) * 0.02f;
        if (osc && !c.oscuro) {
            c.oscuro = true;
            c.oscuroDesde = t;
        } else if (!osc && c.oscuro) {
            c.oscuro = false;
            if (c.dormidaPorOscuro && c.reaccion == R_DORMIR) {
                c.dormidaPorOscuro = false;
                c.ultimaActividad = t;
                reaccionar(c, R_DESLUMBRADA, t);
            }
            c.dormidaPorOscuro = false;
        }
        if (c.oscuro && sirve && !c.dormidaPorOscuro && !c.hablando &&
            t - c.oscuroDesde > 2500 && c.reaccion != R_DORMIR) {
            c.dormidaPorOscuro = true;
            reaccionar(c, R_DORMIR, t);
        }
    }
}

// --- Ciclo de vida -----------------------------------------------------------

static void reiniciarCara(EstadoCara &c, const Personalidad *p, uint32_t t, bool conEntrada)
{
    c.p = p;
    c.t = c.t0 = c.tPrev = t;
    c.dt = 0.016f;

    float viejaX = c.ojos.x, viejaY = c.ojos.y;
    ojosIniciar(c.ojos, p->ojos, t, c.azar);
    // La mirada sigue donde estaba: el cambio de emocion no la teletransporta
    c.ojos.mx.x = c.ojos.x = viejaX;
    c.ojos.my.x = c.ojos.y = viejaY;

    c.gesto = -1;
    c.gp = 0.0f;
    c.ultimoGesto = -1;
    for (int i = 0; i < MAX_GESTOS; i++) {
        c.ultimoUso[i] = 0;
        c.usos[i] = 0.0f;
    }
    programarGesto(c, t + 600);

    c.reaccion = R_NINGUNA;
    c.reaccionPeso = 0.0f;
    c.reaccionFin = 0;
    c.ultimaActividad = t;

    if (conEntrada) {
        c.pendEventos |= EV_ENTRADA;
        if (p->gestoEntrada >= 0 && p->gestoEntrada < p->nGestos)
            empezarGesto(c, p->gestoEntrada, t);
    }
}

void caraIniciar(EstadoCara &c, const Personalidad *p, uint32_t t,
                 uint32_t semilla, bool conEntrada)
{
    azarSembrar(c.azar, semilla);
    c.semilla = semilla;
    c.ojos.x = p->ojos.sesgoX;
    c.ojos.y = p->ojos.sesgoY;
    c.habla.activa = false;
    c.habla.apertura = 0.0f;
    c.habla.enfasis = 0.0f;
    c.habla.finFrase = false;
    c.hablando = false;
    c.respira = 0.0f;

    c.toqueAntes = false;
    c.toqueDesde = 0;
    c.toquesSeguidos = 0;
    c.ultimoToque = 0;
    c.sacudidaDesde = 0;
    c.sacudiendo = false;
    c.ultimoGolpe = 0;
    c.baseLista = false;
    c.inclX = c.inclY = 0.0f;
    c.sentidosActivos = true;
    c.ultimaCaida = 0;

    c.nFlores = c.nLlamas = 0;
    c.ofendidaHasta = 0;
    c.ofendidaLado = 1.0f;
    c.picadas = 0;
    c.ultimaPicada = 0;
    c.caricia = false;
    c.despertoBrusco = false;

    c.clima = CLIMA_NORMAL;
    c.climaForzado = -1;
    c.climaCandidato = CLIMA_NORMAL;
    c.climaDesde = t;
    c.temp = 0;
    c.tempLista = false;
    c.proxClima = t + 30000;
    c.proxGota = t + 3000;
    c.gotaInicio = 0;
    c.ultimaDeFrio = R_TIRITAR;   // (al entrar al frio ya tirita)

    c.luzBase = 0.0f;
    c.luzLista = false;
    c.oscuro = false;
    c.oscuroDesde = t;
    c.dormidaPorOscuro = false;

    c.eventos = c.pendEventos = 0;
    c.eventoGesto = c.pendGesto = -1;
    c.eventoReaccion = c.pendReaccion = 0;

    reiniciarCara(c, p, t, conEntrada);
}

void caraCambiar(EstadoCara &c, const Personalidad *p, uint32_t t, bool conEntrada)
{
    reiniciarCara(c, p, t, conEntrada);
    if (c.hablando) hablaIniciar(c.habla, p->habla, t, c.azar);
}

void caraHablar(EstadoCara &c, bool hablar, uint32_t t)
{
    if (hablar == c.hablando) return;
    c.hablando = hablar;
    caraActividad(c, t);
    if (hablar) {
        hablaIniciar(c.habla, c.p->habla, t, c.azar);
        // Al empezar a hablar te mira (despues aparta la vista a ratos)
        ojosMirar(c.ojos, c.p->ojos.sesgoX, c.p->ojos.sesgoY, t,
                  (uint32_t)azarRango(c.azar, 500.0f, 1100.0f));
        if (c.reaccion == R_DORMIR) c.reaccionFin = t;
        if (c.gesto >= 0 && !(c.p->gestos[c.gesto].flags & G_HABLANDO)) {
            c.gesto = -1;
            programarGesto(c, t + 400);
        }
    } else {
        hablaDetener(c.habla);
        // Al terminar de hablar: un parpadeo (cierre de turno) y sigue
        ojosParpadear(c.ojos, t, c.azar);
        if (c.gesto >= 0 && !(c.p->gestos[c.gesto].flags & G_REPOSO))
            c.gesto = -1;
        programarGesto(c, t + 700);
    }
}

void caraFrame(EstadoCara &c, uint32_t t, const Estimulos &est, Lienzo &out)
{
    const Personalidad &p = *c.p;

    c.eventos = c.pendEventos;
    c.eventoGesto = c.pendGesto;
    c.eventoReaccion = c.pendReaccion;
    c.pendEventos = 0;
    c.pendGesto = -1;
    c.pendReaccion = 0;

    uint32_t paso = t - c.tPrev;
    c.dt = limitar((float)paso * 0.001f, 0.001f, 0.05f);
    c.tPrev = t;
    c.t = t;

    envejecerAnimo(c, t);
    sentir(c, est, t);
    actualizarClima(c, t);

    if (!c.hablando && c.reaccion == R_NINGUNA && t - c.ultimaActividad > DORMIDA_MS)
        reaccionar(c, R_DORMIR, t);

    // Peso de la reaccion: entra rapido, sale suave
    if (c.reaccion != R_NINGUNA) {
        float msDt = c.dt * 1000.0f;
        if (reaccionViva(c)) {
            float entra = c.reaccion == R_DORMIR ? DORMIR_ENTRA_MS : REACCION_ENTRA_MS;
            c.reaccionPeso += msDt / entra;
        } else {
            c.reaccionPeso -= msDt / REACCION_SALE_MS;
        }
        c.reaccionPeso = limitar(c.reaccionPeso, 0.0f, 1.0f);
        if (!reaccionViva(c) && c.reaccionPeso <= 0.0f) {
            c.reaccion = R_NINGUNA;
            programarGesto(c, t + 500);
        }
    }

    // Ofendida: no te mira (la mirada se va a un costado y arriba)
    float sesgoX = c.inclX * 0.9f, sesgoY = c.inclY * 0.6f;
    if (caraOfendida(c)) {
        sesgoX += c.ofendidaLado * 0.9f;
        sesgoY -= 0.3f;
    }
    ojosAvanzar(c.ojos, p.ojos, t, c.dt, c.hablando, sesgoX, sesgoY, c.azar);
    // Con calor los parpados pesan
    if (c.clima == CLIMA_CALOR) c.ojos.apertura *= 0.78f;

    hablaAvanzar(c.habla, p.habla, t, c.dt, c.azar);
    if (c.habla.finFrase) {
        c.eventos |= EV_FIN_FRASE;
        // Se parpadea en las pausas y se vuelve a mirar al que escucha
        if (azarProb(c.azar, 0.6f)) ojosParpadear(c.ojos, t, c.azar);
        if (azarProb(c.azar, 0.5f))
            ojosMirar(c.ojos, p.ojos.sesgoX, p.ojos.sesgoY, t,
                      (uint32_t)azarRango(c.azar, 600.0f, 1400.0f));
    }

    // Gestos
    if (c.gesto >= 0) {
        c.gp = (float)(t - c.gestoInicio) / (float)c.gestoDur;
        if (c.gp >= 1.0f) {
            c.gesto = -1;
            c.gp = 0.0f;
            programarGesto(c, t);
        }
    }
    if (c.gesto < 0 && c.reaccion == R_NINGUNA && (int32_t)(t - c.proxGesto) >= 0) {
        int i = elegirGesto(c, t);
        if (i >= 0) empezarGesto(c, i, t);
        else programarGesto(c, t);
    }

    // Respiracion: un seno con ruido encima (nunca dos respiros iguales)
    float tt = (float)(t - c.t0) * 0.001f;
    c.respira = 0.65f * sinf(6.2831853f * p.respiraHz * tt) +
                0.35f * ruido1(tt * 0.35f, c.semilla + 11u);

    Pose pose;
    pose.dx = pose.dy = 0.0f;
    if (p.temblor > 0.0f) {
        pose.dx = p.temblor * ruido1(tt * 11.0f, c.semilla + 3u);
        pose.dy = p.temblor * 0.6f * ruido1(tt * 13.0f, c.semilla + 5u);
    }
    pose.brillo = 1.0f - p.respiraProf * (0.5f + 0.5f * c.respira);
    if (c.clima == CLIMA_FRIO) {
        // Tirita a rafagas (nunca un temblor parejo: se ve como falla)
        float rafaga = limitar(ruido1(tt * 0.45f, c.semilla + 41u) * 2.2f + 0.3f, 0.0f, 1.0f);
        pose.dx += 0.14f * rafaga * sinf(6.2831853f * 9.0f * tt);
        pose.brillo *= 0.92f;
    }

    lienzoLimpiar(out);
    p.dibujar(out, c, pose);
    lienzoEscalar(out, p.intensidad);

    // Calor: una gota de sudor cae por el costado cada 4-8 s
    if (c.clima == CLIMA_CALOR) {
        if ((int32_t)(t - c.proxGota) >= 0) {
            c.gotaInicio = t;
            c.proxGota = t + (uint32_t)azarRango(c.azar, 4000.0f, 8000.0f);
        }
        uint32_t g = t - c.gotaInicio;
        if (c.gotaInicio && g < 1600) {
            float q = (float)g / 1600.0f;
            float x = ((c.gotaInicio >> 5) & 1) ? 4.0f : 0.0f;
            lienzoPunto(out, x, 0.2f + 3.2f * acelera(q), 0.45f * campana(q, 0.25f));
        }
    }

    if (c.reaccion != R_NINGUNA && c.reaccionPeso > 0.0f) {
        Lienzo r;
        lienzoLimpiar(r);
        dibujarReaccion(r, c, c.reaccion, c.reaccionInicio);
        lienzoMezclar(out, r, suave(c.reaccionPeso));
    }
}

// --- Pinceles ----------------------------------------------------------------

void ojoPunto(Lienzo &l, const EstadoCara &c, float bx, float by,
              float apertura, float nivel, const Pose &pose)
{
    apertura = limitar(apertura, 0.0f, 1.0f);
    if (apertura < 0.05f) return;
    float x = bx + c.ojos.x * 0.95f + pose.dx;
    // El parpado baja: un ojo entrecerrado se ve un poco mas abajo
    float y = by + c.ojos.y * 0.55f + pose.dy + (1.0f - apertura) * 0.3f;
    // El ojo no se sale de la matriz: en el borde se queda pegado (si no,
    // mirar hacia afuera lo apagaria)
    x = limitar(x, 0.0f, 4.0f);
    y = limitar(y, 0.0f, 4.0f);
    lienzoPunto(l, x, y, nivel * pose.brillo * frena(apertura));
}

void ojoAlto(Lienzo &l, const EstadoCara &c, float bx, float by,
             float apertura, float nivel, const Pose &pose)
{
    apertura = limitar(apertura, 0.0f, 1.0f);
    float x = bx + c.ojos.x * 0.7f + pose.dx;
    float y = by + c.ojos.y * 0.35f + pose.dy;
    float arriba = frena(limitar(apertura * 2.0f - 1.0f, 0.0f, 1.0f));
    float abajo  = frena(limitar(apertura * 2.0f, 0.0f, 1.0f));
    lienzoPunto(l, x, y - 1.0f, nivel * pose.brillo * arriba);
    lienzoPunto(l, x, y, nivel * pose.brillo * abajo);
}

void rasgo(Lienzo &l, const uint8_t (*pts)[2], int n, float nivel, const Pose &pose)
{
    float v = nivel * pose.brillo;
    for (int i = 0; i < n; i++)
        lienzoPunto(l, pts[i][0] + pose.dx, pts[i][1] + pose.dy, v);
}

void punto(Lienzo &l, float x, float y, float nivel, const Pose &pose)
{
    lienzoPunto(l, x + pose.dx, y + pose.dy, nivel * pose.brillo);
}
