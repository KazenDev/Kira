/**
 * Catalogo.cpp - El catalogo de sonidos y su codificacion (ver Catalogo.h)
 */
#include "Catalogo.h"
#include "SonidoId.h"
#include <stdio.h>
#include <math.h>

// Abreviaturas para que la tabla se lea como partitura
#define SEN 0
#define SIE 1
#define TRI 2
#define CUA 3
#define RUI 4
#define F   0   // frecuencia fija
#define LIN 1   // glissando lineal
#define CUR 2   // glissando que frena al llegar

// Notas (Hz) para las frases melodicas
#define LA4  440
#define DO5  523
#define RE5  587
#define MI5  659
#define FA5  698
#define SOL5 784
#define LA5  880
#define SI5  988
#define DO6 1047
#define MI6 1319
#define SOL6 1568
#define DO7 2093

struct Variante
{
    uint8_t id;
    uint8_t n;
    Nota notas[MAX_NOTAS];
};

// Pausa (silencio) de 'ms'
#define PAUSA(ms) {SEN, 0, 0, 440, 440, ms, F, 0, 0}

//  onda  vol  volFin frec frecFin dur forma vib vibHz
static const Variante CATALOGO[] = {
    // ======================================================================
    // LLEGADA DE CADA EMOCION
    // ======================================================================

    // ALEGRIA: rapida, saltada, siempre para arriba (arpegios mayores)
    // "¡yu-pi!": Do-Mi-Sol saltados y un remate que trina
    { S_ENTRADA_ALEGRIA, 6, {{TRI, 1023, 1000, DO6, DO6,  70, F, 0, 0},
                             PAUSA(20),
                             {TRI, 1023, 1000, MI6, MI6,  70, F, 0, 0},
                             PAUSA(20),
                             {TRI, 1023, 1000, SOL6, SOL6, 70, F, 0, 0},
                             {TRI, 1023,    0, DO7, 2350, 220, CUR, 3, 14}}},
    // trino feliz: tres chirridos que suben cada vez mas alto
    { S_ENTRADA_ALEGRIA, 5, {{SEN, 1023, 900,  700, 1400,  90, CUR, 0, 0},
                             PAUSA(35),
                             {SEN, 1023, 900, 1100, 1800,  90, CUR, 0, 0},
                             PAUSA(30),
                             {SEN, 1023,   0, 1500, 2300, 200, CUR, 4, 12}}},
    // "¡ji-ji!" saltarin
    { S_ENTRADA_ALEGRIA, 5, {{CUA,  800, 700, SOL5, DO6,  60, LIN, 0, 0},
                             {TRI, 1023, 900, DO6, MI6,  70, CUR, 0, 0},
                             PAUSA(40),
                             {TRI, 1023, 900, MI6, SOL6, 70, CUR, 0, 0},
                             {TRI, 1000,   0, SOL6, 1900, 160, CUR, 0, 0}}},

    // TRISTE: lenta, ligada, siempre para abajo (menor), con temblor
    // "aaay...": La que se quiebra y cae a Do con la voz temblando
    { S_ENTRADA_TRISTE, 2,  {{SEN, 1023, 950, LA5, 830, 300, LIN, 2, 6},
                             {SEN,  950,   0, FA5, DO5, 650, CUR, 4, 6}}},
    // "uuuh" que se desinfla
    { S_ENTRADA_TRISTE, 1,  {{TRI, 1023,   0, 720, 400, 950, CUR, 5, 5}}},
    // tres notas menores que bajan (Mi-Do-La)
    { S_ENTRADA_TRISTE, 4,  {{SEN, 1000, 950, MI5, MI5, 230, F, 0, 0},
                             PAUSA(40),
                             {SEN, 1000, 900, DO5, DO5, 230, F, 0, 0},
                             {SEN,  950,   0, LA4, 400, 600, CUR, 4, 5}}},

    // ENOJADO: aspero (sierra/cuadrada), golpes cortos y secos. "Grave" para
    // este parlante es 300-600 Hz: es un buzzer chiquito que resuena cerca
    // de 2,7 kHz y casi no suena por debajo de ~300 Hz (medido con el propio
    // microfono: un rugido de 100-200 Hz no se oia).
    // "¡grr-AH!": dos golpes y un rugido que baja temblando
    { S_ENTRADA_ENOJADO, 4, {{SIE, 1023, 1000, 480, 380, 110, LIN, 0, 0},
                             {RUI,  900,  800, 900, 900,  60, F, 0, 0},
                             PAUSA(30),
                             {SIE, 1023,    0, 560, 300, 320, CUR, 6, 16}}},
    // "¡HMPH!": un resoplido cortante
    { S_ENTRADA_ENOJADO, 3, {{CUA, 1023, 1000, 700, 380, 100, CUR, 0, 0},
                             PAUSA(50),
                             {CUA, 1023,    0, 600, 300, 240, CUR, 0, 0}}},
    // pisotones: tres golpes graves
    { S_ENTRADA_ENOJADO, 5, {{SIE, 1023, 900, 420, 400,  90, LIN, 0, 0},
                             PAUSA(60),
                             {SIE, 1023, 900, 400, 380,  90, LIN, 0, 0},
                             PAUSA(60),
                             {SIE, 1023,   0, 480, 280, 260, CUR, 5, 14}}},

    // SORPRENDIDO: un salto enorme hacia arriba
    // "¡¿OH?!": sube de golpe y queda colgado arriba temblando
    { S_ENTRADA_SORPRENDIDO, 2, {{SEN, 1023, 1000, 400, 1600, 120, CUR, 0, 0},
                                 {TRI, 1000,    0, 1600, 1850, 200, LIN, 3, 10}}},
    // "¿eh?": una nota y la pregunta que sube
    { S_ENTRADA_SORPRENDIDO, 3, {{SEN, 1023, 1000, 600, 600,  90, F, 0, 0},
                                 PAUSA(40),
                                 {SEN, 1023,    0, 600, 1300, 230, CUR, 0, 0}}},
    // "¡guau!": sube, baja y sube mas alto
    { S_ENTRADA_SORPRENDIDO, 3, {{TRI, 1023, 1000, 500, 1400, 100, CUR, 0, 0},
                                 {TRI, 1000,  950, 1400, 900, 100, CUR, 0, 0},
                                 {TRI,  950,    0, 900, 1900, 230, CUR, 0, 0}}},

    // NEUTRAL: plana y pareja, de robot ("bip-bup")
    { S_ENTRADA_NEUTRAL, 3, {{SEN, 1000, 950, LA5, LA5,  90, F, 0, 0},
                             PAUSA(45),
                             {SEN, 1000,   0, MI5, MI5, 120, F, 0, 0}}},
    { S_ENTRADA_NEUTRAL, 3, {{CUA,  750, 700, 620, 620,  70, F, 0, 0},
                             PAUSA(50),
                             {CUA,  750,   0, 620, 620,  80, F, 0, 0}}},
    { S_ENTRADA_NEUTRAL, 1, {{TRI, 1000,   0, 700, 740, 220, LIN, 0, 0}}},

    // FASTIDIO: "meh", nasal (cuadrada), lenta, se cae sin ganas
    { S_ENTRADA_FASTIDIO, 2, {{CUA,  950, 900, 587, 587, 150, F, 0, 0},
                              {CUA,  950,   0, 554, 440, 320, LIN, 0, 0}}},
    // "ugh" arrastrado
    { S_ENTRADA_FASTIDIO, 1, {{SIE, 1000,   0, 520, 360, 380, CUR, 0, 0}}},
    // "tsk... meh"
    { S_ENTRADA_FASTIDIO, 3, {{RUI,  800,   0, 2500, 2500,  35, F, 0, 0},
                              PAUSA(90),
                              {CUA,  950,   0, 500, 420, 300, LIN, 0, 0}}},

    // MIEDO: aguda y temblorosa (vibrato rapido), entrecortada
    // "¡iiih!" que sube y se quiebra
    { S_ENTRADA_MIEDO, 2, {{TRI, 1000, 1000, 1200, 1550, 180, LIN, 5, 13},
                           {TRI, 1000,    0, 1450,  850, 320, CUR, 6, 13}}},
    // "i-i-iih" tartamudeado
    { S_ENTRADA_MIEDO, 5, {{TRI, 1000, 900, 1300, 1300,  55, F, 0, 0},
                           PAUSA(35),
                           {TRI, 1000, 900, 1380, 1380,  55, F, 0, 0},
                           PAUSA(35),
                           {TRI, 1000,   0, 1250,  800, 280, CUR, 6, 14}}},
    // sirena chiquita: sube temblando y baja
    { S_ENTRADA_MIEDO, 2, {{SEN, 1023, 1000, 900, 1800, 220, CUR, 5, 12},
                           {SEN, 1000,    0, 1800, 1100, 180, CUR, 0, 0}}},

    // CANSADO: lenta, suave, bostezo que se apaga
    // "aaa-uuuh": inspira para arriba y bosteza para abajo
    { S_ENTRADA_CANSADO, 2, {{SEN,  900, 1023, 500, 820, 420, CUR, 0, 0},
                             {SEN, 1023,    0, 820, 380, 850, CUR, 2, 4}}},
    // dos suspiros cada vez mas bajos
    { S_ENTRADA_CANSADO, 3, {{TRI, 1000,  800, 700, 500, 550, CUR, 0, 0},
                             PAUSA(160),
                             {TRI,  900,    0, 520, 380, 650, CUR, 0, 0}}},
    // "mmm..." apagandose
    { S_ENTRADA_CANSADO, 1, {{SEN, 1000,    0, 520, 400, 900, LIN, 2, 3}}},

    // ======================================================================
    // REACCIONES
    // ======================================================================
    // Cosquillas: "ji-ji-ji-jiii"
    { S_COSQUILLAS, 7, {{TRI, 1023, 900, 1100, 1400,  70, CUR, 0, 0},
                        PAUSA(25),
                        {TRI, 1023, 900,  950, 1250,  70, CUR, 0, 0},
                        PAUSA(25),
                        {TRI, 1023, 900, 1150, 1450,  70, CUR, 0, 0},
                        PAUSA(25),
                        {TRI, 1000,   0, 1000, 1500, 140, CUR, 3, 12}}},
    { S_COSQUILLAS, 5, {{SEN, 1023, 900, 1300, 1600,  60, CUR, 0, 0},
                        PAUSA(30),
                        {SEN, 1023, 900, 1250, 1550,  60, CUR, 0, 0},
                        PAUSA(30),
                        {SEN, 1000,   0, 1200, 1750, 130, CUR, 0, 0}}},
    // Carino: "awww" ascendente y dulce
    { S_CARINO, 3, {{SEN, 1000, 1000, DO5, DO5, 140, F, 0, 0},
                    {SEN, 1000, 1000, MI5, MI5, 140, F, 0, 0},
                    {SEN, 1023,    0, SOL5, LA5, 380, CUR, 2, 6}}},
    { S_CARINO, 2, {{SEN, 1000, 1023, MI5, SOL5, 200, CUR, 0, 0},
                    {SEN, 1023,    0, SI5, DO6, 420, CUR, 2, 5}}},
    // Grunido: "grrr" grave y temblado
    { S_GRUNIDO, 1, {{SIE, 1023, 0, 380, 280, 380, LIN, 8, 18}}},
    { S_GRUNIDO, 2, {{SIE, 1023, 1000, 420, 400, 160, F, 8, 20},
                     {SIE, 1000,    0, 400, 260, 240, CUR, 8, 20}}},
    // Sobresalto: "¡ip!" agudisimo de golpe
    { S_SOBRESALTO, 2, {{CUA, 1023, 1000, 400, 1900,  70, CUR, 0, 0},
                        {SEN, 1000,    0, 1900, 1300, 120, CUR, 0, 0}}},
    { S_SOBRESALTO, 2, {{TRI, 1023, 1000, 600, 2100,  60, CUR, 0, 0},
                        {TRI, 1000,    0, 2000, 1500, 150, LIN, 5, 14}}},
    // Despertar: sube despacio y despues de golpe
    { S_DESPERTAR, 2, {{SEN,  900, 1000, 300, 700, 260, CUR, 0, 0},
                       {SEN, 1023,    0, 700, 1100, 120, CUR, 0, 0}}},
    // Mareo: "uuuuh" que baja y ondula lento
    { S_MAREO, 1, {{SEN, 1000, 0, 850, 300, 1000, LIN, 8, 4}}},
    { S_MAREO, 2, {{TRI, 1000, 1000, 600, 950, 450, CUR, 7, 3},
                   {TRI, 1000,    0, 950, 300, 650, CUR, 7, 3}}},
    // Dormir: dos notitas que bajan, bajito
    { S_DORMIR, 2, {{SEN, 600, 450, 500, 450, 280, LIN, 0, 0},
                    {SEN, 500,   0, 520, 420, 450, CUR, 0, 0}}},

    // --- Animo ---------------------------------------------------------------
    // Toque (la picaron): "¿mm?" que sube como pregunta
    { S_TOQUE, 1, {{SEN, 1000, 0, 700, 1050, 170, CUR, 0, 0}}},
    { S_TOQUE, 3, {{TRI, 1000, 900, 650, 650,  60, F, 0, 0},
                   PAUSA(30),
                   {TRI, 1000,   0, 800, 1150, 130, CUR, 0, 0}}},
    // Molesta: "¡ey!" cortante
    { S_MOLESTA, 2, {{CUA, 1023, 1000, 950, 650, 120, CUR, 0, 0},
                     {CUA, 1000,    0, 650, 600,  90, F, 0, 0}}},
    { S_MOLESTA, 1, {{SIE, 1023,    0, 750, 450, 220, CUR, 4, 14}}},
    // Berrinche: rugido, chispazo y gritos temblados
    { S_BERRINCHE, 5, {{SIE, 1023, 1000, 600, 420, 160, LIN, 5, 16},
                       {RUI, 1000,  900, 900, 900,  80, F, 0, 0},
                       {SIE, 1023, 1000, 720, 360, 300, CUR, 7, 14},
                       PAUSA(40),
                       {SIE, 1023,    0, 560, 300, 380, CUR, 8, 12}}},
    // Ofendida: "hmph" por la nariz
    { S_OFENDIDA, 2, {{RUI,  800,   0, 700, 700, 100, F, 0, 0},
                      {CUA,  950,   0, 520, 400, 170, CUR, 0, 0}}},
    // Enamorada: arpegio Do-Mi-Sol-Do y un trino arriba
    { S_ENAMORADA, 4, {{SEN, 1000, 1000, DO5, DO5, 110, F, 0, 0},
                       {SEN, 1000, 1000, MI5, MI5, 110, F, 0, 0},
                       {SEN, 1000, 1000, SOL5, SOL5, 110, F, 0, 0},
                       {SEN, 1023,    0, DO6, 1175, 450, CUR, 3, 6}}},
    // Perdon: dos notitas felices que suben
    { S_PERDON, 2, {{SEN, 1000, 1000, SOL5, SI5, 130, CUR, 0, 0},
                    {SEN, 1023,    0, 1175, MI6, 220, CUR, 0, 0}}},

    // --- Clima y entorno ------------------------------------------------------
    // Estornudo: "a... a... aaa... ¡CHIS!"
    { S_ESTORNUDO, 6, {{TRI,  900, 900, 500, 500, 120, F, 0, 0},
                       PAUSA(70),
                       {TRI,  950, 950, 560, 560, 120, F, 0, 0},
                       PAUSA(70),
                       {TRI, 1000, 1000, 620, 720, 220, CUR, 0, 0},
                       {RUI, 1023,    0, 2000, 2000, 190, F, 0, 0}}},
    // Tiritar: "brrrr" (cuadrada con un temblor rapidisimo)
    { S_TIRITAR, 1, {{CUA, 950, 0, 620, 560, 650, LIN, 6, 20}}},
    { S_TIRITAR, 1, {{TRI, 1000, 0, 800, 700, 600, LIN, 8, 18}}},
    // Acalorada: "fuuu... uff"
    { S_ACALORADA, 3, {{RUI,  700,   0, 900, 900, 260, F, 0, 0},
                       PAUSA(80),
                       {SEN, 1000,   0, 720, 480, 320, CUR, 0, 0}}},
    // Deslumbrada: "¡ay!" y se acostumbra
    { S_DESLUMBRADA, 2, {{TRI, 1023, 1000, 1500, 900, 200, CUR, 0, 0},
                         {TRI, 1000,    0,  900, 1200, 220, CUR, 0, 0}}},
    // Caida: "¡aaaaah!" que cae... y "uf" al final
    { S_CAIDA, 3, {{SEN, 1023, 1000, 1800, 550, 850, LIN, 3, 8},
                   PAUSA(60),
                   {SEN, 1000,    0, 600, 850, 160, CUR, 0, 0}}},

    // ======================================================================
    // GESTOS SUELTOS (solo en reposo, y espaciados)
    // ======================================================================
    { S_BOSTEZO, 2, {{SEN,  800, 900, 480, 720, 380, CUR, 0, 0},
                     {SEN,  900,   0, 720, 380, 700, CUR, 2, 4}}},
    { S_SUSPIRO, 1, {{RUI,  450,   0, 900, 400, 520, CUR, 0, 0}}},
    { S_SILBIDO, 3, {{SEN,  800, 800, 1200, 1800, 180, CUR, 0, 0},
                     PAUSA(90),
                     {SEN,  800,   0, 1800, 1100, 260, CUR, 0, 0}}},
    { S_UGH,     1, {{SIE,  900,   0, 500, 350, 260, CUR, 0, 0}}},
    { S_LAGRIMA, 1, {{SEN,  700,   0, 1300, 800,  70, CUR, 0, 0}}},
    { S_OH,      1, {{SEN,  900,   0, 450, 750, 200, CUR, 0, 0}}},
    { S_HMPH,    2, {{RUI,  800,   0, 600, 600, 110, F, 0, 0},
                     {SIE,  900,   0, 420, 300, 130, LIN, 0, 0}}},
    { S_TITIRITA, 5, {{CUA, 600, 550,  900,  900,  40, F, 0, 0},
                      PAUSA(30),
                      {CUA, 600, 550,  950,  950,  40, F, 0, 0},
                      PAUSA(30),
                      {CUA, 600,   0,  900,  900,  40, F, 0, 0}}},
    { S_RISITA,  3, {{TRI,  800, 700, 1200, 1500,  60, CUR, 0, 0},
                     PAUSA(30),
                     {TRI,  750,   0, 1300, 1650,  90, CUR, 0, 0}}},
    // --- Minijuegos -----------------------------------------------------------
    // "¡a jugar!": escalita rapida que sube
    { S_JUEGO, 4, {{CUA, 800, 800, DO6, DO6, 60, F, 0, 0},
                   {CUA, 800, 800, MI6, MI6, 60, F, 0, 0},
                   {CUA, 800, 800, SOL6, SOL6, 60, F, 0, 0},
                   {TRI, 1000,  0, DO7, DO7, 160, F, 0, 0}}},
    { S_TIC,   1, {{CUA, 600,   0, 1800, 1800, 18, F, 0, 0}}},
    // Gana: fanfarria "ta-ta-taaa"
    { S_GANA,  4, {{TRI, 1000, 1000, SOL5, SOL5, 100, F, 0, 0},
                   PAUSA(30),
                   {TRI, 1000, 1000, SOL5, SOL5, 100, F, 0, 0},
                   {TRI, 1023,    0, DO6, 1100, 420, CUR, 3, 7}}},
    // Pierde: "wa-wa-waaa" (trombon triste)
    { S_PIERDE, 4, {{CUA, 900, 900, 470, 450, 220, LIN, 0, 0},
                    {CUA, 900, 900, 440, 420, 220, LIN, 0, 0},
                    {CUA, 900, 900, 415, 395, 220, LIN, 0, 0},
                    {CUA, 900,   0, 390, 330, 600, LIN, 4, 6}}},
    { S_DING,  1, {{SEN, 1000, 0, 1568, 1568, 320, F, 0, 0}}},
    { S_CUENTA, 6, {{CUA, 800, 0, 700, 700, 60, F, 0, 0},
                    PAUSA(240),
                    {CUA, 800, 0, 700, 700, 60, F, 0, 0},
                    PAUSA(240),
                    {CUA, 800, 0, 700, 700, 60, F, 0, 0},
                    PAUSA(240)}},
    { S_YA,    1, {{CUA, 1023, 0, 1400, 1400, 140, F, 0, 0}}},

    // Hola: el arranque (Do-Sol-Do, con trino al final)
    { S_HOLA,    3, {{SEN, 1000, 1000, DO5, DO5,  90, F, 0, 0},
                     {SEN, 1000, 1000, SOL5, SOL5, 90, F, 0, 0},
                     {SEN, 1023,    0, DO6, 1175, 240, CUR, 3, 10}}},
};
static const int NCATALOGO = sizeof(CATALOGO) / sizeof(CATALOGO[0]);

static const Variante *buscar(uint8_t id, int v)
{
    int k = 0;
    for (int i = 0; i < NCATALOGO; i++) {
        if (CATALOGO[i].id != id) continue;
        if (k == v) return &CATALOGO[i];
        k++;
    }
    return 0;
}

int catalogoVariantes(uint8_t id)
{
    int k = 0;
    for (int i = 0; i < NCATALOGO; i++)
        if (CATALOGO[i].id == id) k++;
    return k;
}

int catalogoNotas(uint8_t id, int v, const Nota **notas)
{
    const Variante *x = buscar(id, v);
    if (!x) return 0;
    *notas = x->notas;
    return x->n;
}

int catalogoDuracion(uint8_t id, int v)
{
    const Variante *x = buscar(id, v);
    if (!x) return 0;
    int d = 0;
    for (int i = 0; i < x->n; i++) d += x->notas[i].dur;
    return d;
}

static int recortar(int v, int lo, int hi)
{
    return v < lo ? lo : (v > hi ? hi : v);
}

// Un segmento ya listo para el sintetizador
struct Segmento
{
    int onda, vol, volFin, f0, f1, dur, forma;
};

// Frecuencia de la nota en la fraccion q (0..1) de su duracion
static float frecEn(const Nota &a, float q, float tono)
{
    float f0 = a.frec * tono, f1 = a.frecFin * tono;
    if (a.forma == CUR) q = sinf(q * 1.5707963f);   // la misma curva de CODAL
    else if (a.forma == F) q = 0.0f;
    return f0 + (f1 - f0) * q;
}

// Expande el vibrato: la nota se parte en medios ciclos que suben y bajan
// 'vib' % alrededor de su glissando. Los extremos quedan en la nota justa.
static int expandir(const Nota &a, float tono, float durF, Segmento *s, int libres)
{
    int dur = recortar((int)(a.dur * durF + 0.5f), 1, 9999);
    if (a.vib == 0 || a.vol == 0 || libres < 3) {
        if (libres < 1) return 0;
        s[0].onda = a.onda;
        s[0].vol = a.vol;
        s[0].volFin = a.volFin;
        s[0].f0 = (int)(a.frec * tono + 0.5f);
        s[0].f1 = (int)(a.frecFin * tono + 0.5f);
        s[0].dur = dur;
        s[0].forma = a.forma;
        return 1;
    }
    float hz = a.vibHz ? (float)a.vibHz : 9.0f;
    int medios = (int)(dur * hz * 2.0f / 1000.0f + 0.5f);
    if (medios < 2) medios = 2;
    if (medios > libres) medios = libres;
    float d = a.vib * 0.01f;
    float fPrev = frecEn(a, 0.0f, tono);
    int hecho = 0;
    for (int i = 0; i < medios; i++) {
        float q1 = (float)(i + 1) / (float)medios;
        float signo = (i % 2 == 0) ? 1.0f : -1.0f;
        float f = frecEn(a, q1, tono) * (i == medios - 1 ? 1.0f : 1.0f + signo * d);
        s[i].onda = a.onda;
        s[i].vol = (int)(a.vol + (a.volFin - a.vol) * ((float)i / medios));
        s[i].volFin = (int)(a.vol + (a.volFin - a.vol) * q1);
        s[i].f0 = (int)(fPrev + 0.5f);
        s[i].f1 = (int)(f + 0.5f);
        int fin = (int)(dur * q1 + 0.5f);
        s[i].dur = fin - hecho > 0 ? fin - hecho : 1;
        hecho = fin;
        s[i].forma = LIN;
        fPrev = f;
    }
    return medios;
}

int codificarNotas(const Nota *notas, int n, float tono, float dur,
                   char *salida, int capacidad)
{
    if (n <= 0 || n > MAX_NOTAS) return -1;

    Segmento segs[MAX_SEGMENTOS];
    int ns = 0;
    for (int i = 0; i < n; i++) {
        // Cada nota que queda necesita al menos un segmento
        int reservados = n - i - 1;
        int libres = MAX_SEGMENTOS - ns - reservados;
        ns += expandir(notas[i], tono, dur, &segs[ns], libres);
    }

    int largo = ns * (CHARS_NOTA + 1) - 1;
    if (ns == 0 || capacidad < largo + 1) return -1;

    char *p = salida;
    for (int i = 0; i < ns; i++) {
        const Segmento &a = segs[i];
        int f0 = recortar(a.f0, 20, 9999);
        int f1 = recortar(a.f1, 20, 9999);
        int forma = a.forma;
        // Pasos de la interpolacion: la curva de CODAL es sin(paso en
        // grados), con 90 llega justo al final. Sin cambio, 1 paso.
        int pasos = (forma == F || f0 == f1) ? 1 : (forma == CUR ? 90 : 128);
        if (f0 == f1) forma = F;
        // Formato de SoundExpressions.cpp (parseSoundExpression): campos de
        // ancho fijo. [15-17] y [22-25] no se usan. [34-43] = sin efecto
        // (el vibrato de CODAL no sirve, ver Catalogo.h). [44-71] = sin azar
        // (el azar lo pone el motor, asi el bench lo controla).
        int w = snprintf(p, (size_t)(capacidad - (p - salida)),
            "%01d%04d%04d%04d%02d000%04d0000%04d%04d0000000000"
            "0000000000000000000000000000",
            recortar(a.onda, 0, 4), recortar(a.vol, 0, 1023), f0,
            recortar(a.dur, 1, 9999), forma, f1,
            recortar(a.volFin, 0, 1023), pasos);
        if (w != CHARS_NOTA) return -1;
        p += w;
        if (i < ns - 1) *p++ = ',';
    }
    *p = '\0';
    return (int)(p - salida);
}

int catalogoCodificar(uint8_t id, float azarVariante, float azarTono,
                      float azarDur, char *salida, int capacidad)
{
    int nv = catalogoVariantes(id);
    if (nv <= 0) return -1;
    int v = (int)(azarVariante * nv);
    if (v >= nv) v = nv - 1;
    const Nota *notas = 0;
    int n = catalogoNotas(id, v, &notas);
    if (n <= 0) return -1;
    float tono = 0.92f + 0.16f * azarTono;
    float dur  = 0.9f + 0.2f * azarDur;
    return codificarNotas(notas, n, tono, dur, salida, capacidad);
}
