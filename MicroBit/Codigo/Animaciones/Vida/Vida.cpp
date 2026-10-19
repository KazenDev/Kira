/**
 * Vida.cpp - Pegamento del motor de vida con CODAL (ver Vida.h)
 */
#include "Vida.h"
#include <stdio.h>
#include "Nucleo/Cara.h"
#include "Nucleo/SonidoId.h"
#include "../Emociones/Emociones.h"
#include "Nucleo/Juegos.h"
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
    Estimulos nada = { false, 0, 0, 0, false, false, false, false, false, 0, false, 0 };
    caraFrame(muestra, t, nada, l);
    volcar(l);
}

static void atenderEventos()
{
    uint8_t ev = cara.eventos;
    if (!ev) return;

    // (El sonido de llegada lo toca cambiarEmocion() ANTES de la transicion,
    // para que se oiga cuando empieza el cambio y no cuando termina.)

    if (ev & EV_REACCION)
        sonidoTocar((uint8_t)(S_COSQUILLAS + cara.eventoReaccion - R_COSQUILLAS));

    if ((ev & EV_GESTO) && cara.eventoGesto >= 0 &&
        cara.eventoGesto < cara.p->nGestos) {
        uint8_t s = cara.p->gestos[cara.eventoGesto].sonido;
        if (s == S_LAGRIMA)
            uBit.serial.send("LAGRIMA\n");   // como antes: aviso por serial
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
        uBit.serial.send(juego.aviso);
        uBit.serial.send("\n");
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

    caraFrame(cara, inicio, e, lienzo);
    volcar(lienzo);
    atenderEventos();

    // 60 fps: duerme lo que falte para 16 ms (cede la CPU a BLE, la replica
    // LED y los sonidos)
    uint32_t gasto = uBit.systemTime() - inicio;
    uBit.sleep(gasto < 16 ? 16 - gasto : 1);
}
