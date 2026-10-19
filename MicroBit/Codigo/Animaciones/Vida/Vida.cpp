/**
 * Vida.cpp - Pegamento del motor de vida con CODAL (ver Vida.h)
 */
#include "Vida.h"
#include <stdio.h>
#include <math.h>
#include "Nucleo/Cara.h"
#include "Nucleo/SonidoId.h"
#include "../Emociones/Emociones.h"
#include "Nucleo/Juegos.h"
#include "Nucleo/Oido.h"
#include "Nucleo/Pantalla.h"
#include "../Sistema/MicFft/MicFft.h"
#include "Sonidos/Sonidos.h"
#include "../Sistema/Loading/Loading.h"
#include "../Sistema/Voz/Voz.h"
#include "../Sistema/Escuchar/Escuchar.h"
#include "../Sistema/Metronomo/Metronomo.h"

bool modoHablar = false;

// Minijuego en curso (mantener B). Mientras dura, A y B son del juego.
static EstadoJuego juego;
static uint8_t proximoJuego = J_DADO;
static bool pendA = false, pendB = false;   // flancos para el juego
static bool pendPicar = false;              // toque corto de B para la cara
static bool toqueAntesJuego = false;

// La cara viva. static: son ~400 bytes que no pueden vivir en la pila del
// bucle principal (la pila util es ~2 KB y las transiciones ya usan 580).
static EstadoCara cara;
static Lienzo lienzo;
static bool lista = false;

// Brillo global del display. Antes las caras corrian a 75-100 con pixeles a
// 255; ahora el brillo se reparte POR PIXEL (escala de grises con gamma) y el
// global queda mas alto para que se vea en una feria con luz.
static int brilloGlobal = 160;

static uint32_t ultimaAcel = 0;
static uint32_t ultimaTemp = 0;
static uint32_t ultimaLuz = 0;
// La luz se mide con la propia matriz de LEDs y CODAL tiene que cambiar el
// modo del display para eso. Apagada por defecto: LUZ:ON la activa.
static bool luzActiva = false;
static uint32_t nanVistos = 0;

// LO QUE MUESTRA (ver Nucleo/Pantalla.h): iconos, texto, flecha, temporizador
static EstadoPantalla pantalla;
static bool pantallaLista = false;
static Lienzo capa;
static float rumboLeido = -1.0f;
static uint32_t ultimoRumbo = 0;

static const uint8_t *glifoCodal(char c)
{
    static BitmapFont fuente = BitmapFont::getSystemFont();
    return fuente.get(c);
}

static void asegurarPantalla()
{
    if (!pantallaLista) {
        pantallaIniciar(pantalla);
        pantallaLista = true;
    }
}

void vidaIcono(const char *nombre, uint32_t ms)
{
    asegurarPantalla();
    pantallaIcono(pantalla, pantallaIconoPorNombre(nombre), uBit.systemTime(), ms);
}

void vidaTexto(const char *texto)
{
    asegurarPantalla();
    pantallaTexto(pantalla, texto, uBit.systemTime());
}

void vidaFlecha(int grados, uint32_t ms)
{
    asegurarPantalla();
    pantallaFlecha(pantalla, (float)grados, uBit.systemTime(), ms);
}

void vidaTimer(uint32_t segundos)
{
    asegurarPantalla();
    pantallaTimer(pantalla, segundos, uBit.systemTime());
}

// EL OIDO (ver Nucleo/Oido.h): el microfono prendido midiendo SOLO el nivel
// de ruido, para notar a la gente. Lo usa la cara mientras esta en pantalla;
// los modos que necesitan el microfono (escucha manual, ecualizador,
// grabador) lo toman y al volver la cara el oido lo vuelve a prender.
static DetectorOido oido;
static bool oidoActivo = true;
static bool oidoListo = false;
static uint32_t ultimaOida = 0;
static int ultimoConteoFft = -1;
static uint32_t ultimoAmbiente = 0;
static uint32_t finSordera = 0;      // un ratito despues de que Kira suene
static float movimiento = 0.0f;      // cuanto la estan moviendo (EMA)
static float prevAx = 0, prevAy = 0, prevAz = 0;

// El boton B fuera de la escucha
static bool bAntes = false;
static uint32_t bDesde = 0;
static bool bLargo = false;

static void asegurarLista()
{
    if (lista) return;
    caraIniciar(cara, personalidadDe(emocionActual), uBit.systemTime(),
                (uint32_t)uBit.random(0x7FFFFFFF) ^ 0x5EED1234u, false);
    lista = true;
}

void vidaIniciar()
{
    sonidosIniciar();
    caraIniciar(cara, personalidadDe(emocionActual), uBit.systemTime(),
                (uint32_t)uBit.random(0x7FFFFFFF) ^ 0x5EED1234u, true);
    lista = true;
    sonidoTocar(S_HOLA);
}

void vidaPonerEmocion(EmocionActual e, bool conEntrada)
{
    asegurarLista();
    caraCambiar(cara, personalidadDe(e), uBit.systemTime(), conEntrada);
}

void iniciarHablar()
{
    asegurarLista();
    modoHablar = true;
    // La boca de la emocion ACTUAL (si la transicion todavia no termino,
    // vidaPonerEmocion la cambia al terminar y el habla sigue).
    caraHablar(cara, true, uBit.systemTime());
}

void detenerHablar()
{
    modoHablar = false;
    if (lista) caraHablar(cara, false, uBit.systemTime());
}

void vidaActividad()
{
    if (lista) caraActividad(cara, uBit.systemTime());
}

void vidaReaccion(int r)
{
    asegurarLista();
    caraForzarReaccion(cara, (uint8_t)r, uBit.systemTime());
}

void vidaGesto(int idx)
{
    asegurarLista();
    caraForzarGesto(cara, idx, uBit.systemTime());
}

void vidaSentidos(bool activos)
{
    asegurarLista();
    cara.sentidosActivos = activos;
}

void vidaBrillo(int brillo)
{
    if (brillo < 10) brillo = 10;
    if (brillo > 255) brillo = 255;
    brilloGlobal = brillo;
}

int vidaBrilloGlobal()
{
    return brilloGlobal;
}

void vidaClima(int clima)
{
    asegurarLista();
    caraForzarClima(cara, clima, uBit.systemTime());
}

void vidaPicar()
{
    pendPicar = true;
}

void vidaLuz(bool activa)
{
    luzActiva = activa;
}

void vidaOido(bool activo)
{
    oidoActivo = activo;
    if (!activo) micFftDetener();
}

void vidaOidoPausar()
{
    // Otro modo necesita el microfono (escucha manual, grabador): se lo
    // soltamos. El oido lo retoma solo cuando la cara vuelve a la pantalla.
    micFftDetener();
    ultimoConteoFft = -1;
}

// Nivel de ruido 0..100: el PROMEDIO de las 5 bandas del FFT. Medido en la
// placa, la banda mas fuerte sola saltaba de 8 a 76 en dos segundos en una
// pieza tranquila: demasiado nerviosa para notar gente.
static float nivelRuido()
{
    const float *b = micFftBandas();
    float s = 0.0f;
    for (int i = 0; i < 5; i++) s += b[i];
    float n = (s / 5.0f) * (100.0f / 4.5f);
    return n > 100.0f ? 100.0f : n;
}

// Una muestra del oido. Devuelve el EventoOido de este frame.
static int oir(uint32_t ahora)
{
    if (!oidoActivo) return OIDO_NADA;
    if (!micFftActivo()) {
        micFftIniciar();
        ultimoConteoFft = -1;
        oidoListo = false;      // retoma el microfono: vuelve a ajustarse
    }
    if (!oidoListo) {
        oidoIniciar(oido, ahora);
        oidoListo = true;
    }
    if (ahora - ultimaOida < 50) return OIDO_NADA;
    ultimaOida = ahora;
    int conteo = micFftConteo();
    if (conteo == ultimoConteoFft) return OIDO_NADA;   // sin ventana nueva
    ultimoConteoFft = conteo;
    // Sordo mientras Kira suena (sus sonidos o su voz) y medio segundo despues
    if (modoHablar || sonidoSonando()) finSordera = ahora + 600;
    bool sordo = (int32_t)(finSordera - ahora) > 0;
    return oidoMuestra(oido, nivelRuido(), ahora, sordo);
}

// Cada 2 s: cuanto ruido hay y cuanto la mueven, para que la IA sepa donde
// esta parada (el bloque "tu entorno ahora" del backend)
static void reportarAmbiente(uint32_t ahora)
{
    if (ahora - ultimoAmbiente < 2000) return;
    ultimoAmbiente = ahora;
    char linea[48];
    snprintf(linea, sizeof(linea), "AMBIENTE:%d:%d:%d:%d\n",
             (int)oido.rapido, (int)oido.base, (int)oidoActividad(oido),
             (int)movimiento);
    avisarPC(linea);
}

// VIDA:? -> que esta haciendo la cara (diagnostico por serial)
void vidaDiagnostico()
{
    asegurarLista();
    float suma = 0;
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++) suma += lienzo.v[y][x];
    char s[160];
    snprintf(s, sizeof(s),
        "VIDA:reac=%d peso=%d gesto=%d clima=%d temp=%d luz=%d base=%d osc=%d "
        "flores=%d llamas=%d ofendida=%d suma=%d nan=%lu brillo=%d juego=%d\n",
        cara.reaccion, (int)(cara.reaccionPeso * 100), cara.gesto, cara.clima,
        cara.temp, luzActiva ? 1 : 0, (int)cara.luzBase, cara.oscuro ? 1 : 0,
        cara.nFlores, cara.nLlamas, caraOfendida(cara) ? 1 : 0,
        (int)(suma * 100), (unsigned long)nanVistos, uBit.display.getBrightness(),
        juegoActivo(juego) ? juego.juego : 0);
    uBit.serial.send(s);
}

// La cara esta en pantalla (ningun otro modo la tapa)
static bool caraVisible()
{
    return !modoLoading && !modoVoz && !modoEscuchar && !modoMetro;
}

bool vidaJugando()
{
    return juegoActivo(juego);
}

void vidaJugar(int cual)
{
    asegurarLista();
    if (cual <= J_NINGUNO || cual >= NUM_JUEGOS) {
        cual = proximoJuego;
        proximoJuego = (uint8_t)(proximoJuego % (NUM_JUEGOS - 1) + 1);   // rota
    }
    if (modoHablar) detenerHablar();
    pendA = pendB = false;
    toqueAntesJuego = uBit.logo.isPressed();
    juegoIniciar(juego, (uint8_t)cual, uBit.systemTime(),
                 (uint32_t)uBit.random(0x7FFFFFFF) ^ 0xD1CEu);
}

void vidaTerminarJuego()
{
    if (juegoActivo(juego)) juegoTerminar(juego);
}

void vidaBotonesJuego(bool pulsoA, bool pulsoB)
{
    if (pulsoA) pendA = true;
    if (pulsoB) pendB = true;
}

void vidaBotonB(bool b)
{
    uint32_t t = uBit.systemTime();
    if (b && !bAntes) {
        bDesde = t;
        bLargo = false;
    }
    // Mantener B ~0,7 s = minijuego (solo con la cara en pantalla y callada)
    if (b && !bLargo && t - bDesde >= 700) {
        bLargo = true;
        if (caraVisible() && !modoHablar) vidaJugar(0);
    }
    // Toque corto = picarla
    if (!b && bAntes && !bLargo && caraVisible())
        pendPicar = true;
    bAntes = b;
}

// Vuelca el lienzo: solo escribe los pixeles que cambiaron. Compara contra
// lo que HAY en el display (no contra un cache), asi si otra cosa dibujo
// encima (un loading, un "?"), el frame siguiente lo repara solo.
static void volcar(const Lienzo &l)
{
    if (uBit.display.getBrightness() != brilloGlobal)
        uBit.display.setBrightness(brilloGlobal);
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 5; x++) {
            float f = l.v[y][x];
            if (f != f) {              // NaN: se cuenta y se muestra apagado
                nanVistos++;
                f = 0.0f;
            }
            uint8_t v = lienzoAByte(f);
            if (uBit.display.image.getPixelValue(x, y) != v)
                uBit.display.image.setPixelValue(x, y, v);
        }
}

void vidaDibujarCara(EmocionActual e)
{
    // Una cara temporal, quieta y sin entrada: lo que la transicion necesita
    // es "como se ve esta emocion", no un frame de la cara viva.
    static EstadoCara muestra;
    static Lienzo l;
    uint32_t t = uBit.systemTime();
    caraIniciar(muestra, personalidadDe(e), t, 1u, false);
    muestra.sentidosActivos = false;
    muestra.proxGesto = t + 60000;
    Estimulos nada = { false, 0, 0, 0, false, false, false, false, false, 0, false, 0, false, false };
    caraFrame(muestra, t, nada, l);
    volcar(l);
}

// Los nombres de las reacciones, para avisarle a la PC (EVENTO:<NOMBRE>).
// Mismo orden que Reaccion (Cara.h) desde R_COSQUILLAS.
static const char *const NOMBRE_REACCION[] = {
    "COSQUILLAS", "CARINO", "GRUNIDO", "SOBRESALTO", "DESPERTAR", "MAREO",
    "DORMIR", "TOQUE", "MOLESTA", "BERRINCHE", "OFENDIDA", "ENAMORADA",
    "PERDON", "ESTORNUDO", "TIRITAR", "ACALORADA", "DESLUMBRADA", "CAIDA",
    "VOCES", "APLAUSO",
};
static const int N_NOMBRES = sizeof(NOMBRE_REACCION) / sizeof(NOMBRE_REACCION[0]);
static uint8_t ultimoAvisado = 0;
static uint32_t ultimoAviso = 0;

// La PC usa estos avisos para que Kira HABLE sola ("¡ey, me mareaste!").
// Freno: la misma reaccion no se repite antes de 3 s (una sacudida larga o
// tres toques seguidos son UNA cosa que contar, no tres). Dormirse no se
// avisa: no hay nada que decir.
static void avisarReaccion(uint8_t r)
{
    if (r < R_COSQUILLAS || r == R_DORMIR) return;
    int i = r - R_COSQUILLAS;
    if (i < 0 || i >= N_NOMBRES) return;
    uint32_t ahora = uBit.systemTime();
    if (r == ultimoAvisado && ahora - ultimoAviso < 3000) return;
    ultimoAvisado = r;
    ultimoAviso = ahora;
    char linea[32];
    snprintf(linea, sizeof(linea), "EVENTO:%s\n", NOMBRE_REACCION[i]);
    avisarPC(linea);
}

static void atenderEventos()
{
    uint8_t ev = cara.eventos;
    if (!ev) return;

    // (El sonido de llegada lo toca cambiarEmocion() ANTES de la transicion,
    // para que se oiga cuando empieza el cambio y no cuando termina.)

    if (ev & EV_REACCION) {
        sonidoTocar((uint8_t)(S_COSQUILLAS + cara.eventoReaccion - R_COSQUILLAS));
        avisarReaccion(cara.eventoReaccion);
    }

    if ((ev & EV_GESTO) && cara.eventoGesto >= 0 &&
        cara.eventoGesto < cara.p->nGestos) {
        uint8_t s = cara.p->gestos[cara.eventoGesto].sonido;
        if (s == S_LAGRIMA)
            avisarPC("LAGRIMA\n");   // como antes: aviso por serial
        // Los gestos sueltos solo suenan en silencio: si Kira habla, su voz
        // ya esta sonando en el cel y un bip encima molesta.
        if (s != S_NADA && !modoHablar)
            sonidoGesto(s);
    }
}

static void frameJuego(uint32_t inicio)
{
    bool toque = uBit.logo.isPressed();
    EntradaJuego e;
    e.pulsoA = pendA;
    e.pulsoB = pendB;
    e.toque = toque && !toqueAntesJuego;
    e.sacudida = uBit.accelerometer.getGesture() == ACCELEROMETER_EVT_SHAKE;
    toqueAntesJuego = toque;
    pendA = pendB = false;

    bool sigue = juegoFrame(juego, inicio, e, lienzo);
    volcar(lienzo);
    if (juego.sonido) sonidoTocar(juego.sonido);
    if (juego.hayAviso) {
        char linea[48];
        snprintf(linea, sizeof(linea), "%s\n", juego.aviso);
        avisarPC(linea);
    }
    if (!sigue) {
        // Vuelve la cara, contenta de haber jugado
        caraActividad(cara, uBit.systemTime());
        pendPicar = false;
    }
}

void vidaFrame()
{
    asegurarLista();
    uint32_t inicio = uBit.systemTime();

    if (juegoActivo(juego)) {
        frameJuego(inicio);
        uint32_t gasto = uBit.systemTime() - inicio;
        uBit.sleep(gasto < 16 ? 16 - gasto : 1);
        return;
    }

    Estimulos e;
    e.hayAcel = false;
    e.ax = e.ay = e.az = 0.0f;
    // El acelerometro muestrea a ~50 Hz: leerlo cada 40 ms alcanza
    if (inicio - ultimaAcel >= 40) {
        ultimaAcel = inicio;
        Sample3D s = uBit.accelerometer.getSample();
        e.hayAcel = true;
        e.ax = (float)s.x;
        e.ay = (float)s.y;
        e.az = (float)s.z;
        float tiron = fabsf(e.ax - prevAx) + fabsf(e.ay - prevAy) + fabsf(e.az - prevAz);
        prevAx = e.ax; prevAy = e.ay; prevAz = e.az;
        movimiento += (tiron - movimiento) * 0.15f;
    }
    int gesto = uBit.accelerometer.getGesture();
    e.sacudida = gesto == ACCELEROMETER_EVT_SHAKE;
    e.caida = gesto == ACCELEROMETER_EVT_FREEFALL;
    e.toque = uBit.logo.isPressed();
    e.picar = pendPicar;
    pendPicar = false;

    // El termometro es lento y cambia lento: cada 5 s alcanza
    e.hayTemp = false;
    e.temp = 0;
    if (ultimaTemp == 0 || inicio - ultimaTemp >= 5000) {
        ultimaTemp = inicio;
        e.hayTemp = true;
        e.temp = uBit.thermometer.getTemperature();
    }
    // La luz se mide con la propia matriz de LEDs (CODAL la deja en modo
    // "light sense"); 4 lecturas por segundo sobran para notar una mano
    e.hayLuz = false;
    e.luz = 0;
    if (luzActiva && inicio - ultimaLuz >= 250) {
        ultimaLuz = inicio;
        e.hayLuz = true;
        e.luz = uBit.display.readLightLevel();
    }

    int oidoEv = oir(inicio);
    e.voces = oidoEv == OIDO_VOCES;
    e.aplauso = oidoEv == OIDO_APLAUSO;

    caraFrame(cara, inicio, e, lienzo);
    asegurarPantalla();
    // La flecha necesita la brujula: solo si esta calibrada (calibrar es un
    // juego de ~30 s que bloquea la placa: lo pide la persona, no la flecha)
    if (pantalla.capa == CAPA_FLECHA && inicio - ultimoRumbo >= 100) {
        ultimoRumbo = inicio;
        rumboLeido = uBit.compass.isCalibrated() ? (float)uBit.compass.heading() : -1.0f;
    }
    if (pantallaFrame(pantalla, inicio, rumboLeido, glifoCodal, capa))
        volcar(capa);
    else
        volcar(lienzo);
    if (pantalla.eventos & EVP_TIMER_FIN) {
        sonidoTocar(S_ALARMA);
        avisarPC("EVENTO:TIMER\n");   // para que Kira lo anuncie
    }
    atenderEventos();
    if (oidoActivo) reportarAmbiente(inicio);

    // 60 fps: duerme lo que falte para 16 ms (cede la CPU a BLE, la replica
    // LED y los sonidos)
    uint32_t gasto = uBit.systemTime() - inicio;
    uBit.sleep(gasto < 16 ? 16 - gasto : 1);
}
