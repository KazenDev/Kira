/**
 * MicFft.cpp - ANALIZADOR DE ESPECTRO REAL 🎚️⚡
 *
 * Como recibe datos (modelo del stream de CODAL):
 *   NRF52ADCChannel (mic, 11kHz) -> DataStream (evento diferido)
 *     -> uBit.audio.splitter -> canal nuevo -> MicFft::pullRequest()
 *         -> upstream.pull() (ManagedBuffer con samples 8-bit signed)
 *
 * El FFT corre cada 128 samples (~11.6ms) en el contexto del fiber del
 * stream (~100us con FPU, despreciable). Inspirado en el proyecto
 * microbit-spectrum (FFT + 5 bandas en la matriz 5x5).
 *
 * BANDAS (FFT-128 @ 11kHz, bin k = k*86Hz, Nyquist 5.5kHz):
 *   B0 bajos      bins  1- 8   (~86-690 Hz)   voces graves / graves
 *   B1 medios-b   bins  9-17   (~774-1463 Hz)
 *   B2 medios     bins 18-30   (~1548-2581 Hz)
 *   B3 medios-a   bins 31-47   (~2666-4044 Hz)
 *   B4 agudos     bins 48-63   (~4129-5419 Hz)
 *
 * AUTO-GANANCIA: la referencia de volumen sube al instante y decae lento
 * (0.985/frame), con piso minimo - asi los susurros se ven y los gritos
 * llenan la pantalla sin saturar. El silencio (max < umbral) pone todo 0.
 */
#include "MicFft.h"
#include "MicroBit.h"
#include "StreamSplitter.h"
#include "DataStream.h"
#include <math.h>

#define FFT_N      128     // tamanio del FFT (potencia de 2)
#define N_BANDAS   5

// Limites de cada banda [binIni, binFin) (el bin 0 es DC, se salta)
static const int BANDAS_BINS[N_BANDAS][2] = {
    { 1,  9}, { 9, 18}, {18, 31}, {31, 48}, {48, 64}
};

static float win[FFT_N];    // ventana Hann
static float re[FFT_N];     // muestras de entrada (parte real)
static float im[FFT_N];     // parte imaginaria (0)
static int   cola = 0;      // cuantas muestras llevamos acumuladas

static float bandas[N_BANDAS];  // resultado: 0..4.5
static float ganancia = 25.0f;  // auto-ganancia (referencia de volumen)
static bool  lista = false;     // ventana calculada al iniciar
static int   ventanas = 0;      // FFTs procesados (para debug del stream)

// ---------------------------------------------------------------------------
// FFT radix-2 iterativo (decimacion en tiempo), n potencia de 2
// ---------------------------------------------------------------------------
static void fftRadix2(float *r, float *i, int n)
{
    const float KPI = 3.14159265f;   // (PI es macro de CODAL: no usar ese nombre)

    // Permutacion bit-reversal
    for (int x = 1, j = 0; x < n; x++) {
        int bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (x < j) {
            float t = r[x]; r[x] = r[j]; r[j] = t;
            t = i[x]; i[x] = i[j]; i[j] = t;
        }
    }
    // Mariposas
    for (int len = 2; len <= n; len <<= 1) {
        float ang = 2.0f * KPI / (float)len;
        float wRe = cosf(ang), wIm = -sinf(ang);
        for (int base = 0; base < n; base += len) {
            float cRe = 1.0f, cIm = 0.0f;
            for (int k = 0; k < len / 2; k++) {
                float uRe = r[base + k], uIm = i[base + k];
                float vRe = r[base + k + len/2] * cRe - i[base + k + len/2] * cIm;
                float vIm = r[base + k + len/2] * cIm + i[base + k + len/2] * cRe;
                r[base + k]         = uRe + vRe;
                i[base + k]         = uIm + vIm;
                r[base + k + len/2] = uRe - vRe;
                i[base + k + len/2] = uIm - vIm;
                float nRe = cRe * wRe - cIm * wIm;
                cIm = cRe * wIm + cIm * wRe;
                cRe = nRe;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Procesa una ventana completa de FFT_N muestras -> bandas[5]
// ---------------------------------------------------------------------------
static void procesarFFT()
{
    // 1) Quitar DC (media) + ventana Hann
    float media = 0.0f;
    for (int i = 0; i < FFT_N; i++) media += re[i];
    media /= FFT_N;
    for (int i = 0; i < FFT_N; i++) {
        re[i] = (re[i] - media) * win[i];
        im[i] = 0.0f;
    }

    // 2) FFT
    fftRadix2(re, im, FFT_N);

    // 3) Magnitud por banda (maximo en el rango, sin el bin DC)
    float magBand[N_BANDAS] = {0, 0, 0, 0, 0};
    float maxAll = 0.0f;
    for (int b = 0; b < N_BANDAS; b++) {
        for (int k = BANDAS_BINS[b][0]; k < BANDAS_BINS[b][1] && k < FFT_N / 2; k++) {
            float m = sqrtf(re[k] * re[k] + im[k] * im[k]);
            if (m > magBand[b]) magBand[b] = m;
            if (m > maxAll) maxAll = m;
        }
    }

    // 4) Auto-ganancia: referencia que sube al instante y decae lento
    if (maxAll > ganancia) ganancia = maxAll;
    else ganancia *= 0.985f;
    if (ganancia < 25.0f) ganancia = 25.0f;

    // 5) Alturas (0..4.5). Silencio real -> todo 0
    // NOTA (race benigno y a proposito): esto corre en el fiber del stream
    // mientras Barra lee bandas[] en el main loop. Peor caso: un frame ve
    // una mezcla vieja/nueva - irrelevante para visualizacion a 60fps.
    if (maxAll < 4.0f) {
        for (int b = 0; b < N_BANDAS; b++) bandas[b] = 0.0f;
    } else {
        for (int b = 0; b < N_BANDAS; b++) {
            float h = magBand[b] / ganancia * 4.5f;
            bandas[b] = (h > 4.5f) ? 4.5f : h;
        }
    }
    ventanas++;
}

// FFTs procesados desde que inicio (para confirmar que el stream fluye)
int micFftConteo()
{
    return ventanas;
}

// ---------------------------------------------------------------------------
// El sink: CODAL llama pullRequest() cuando hay datos nuevos
// ---------------------------------------------------------------------------
class MicFft : public DataSink
{
    public:
    DataSource &upstream;

    MicFft(DataSource &source) : upstream(source)
    {
        source.connect(*this);          // engancharnos al canal del splitter
    }

    virtual int pullRequest() override
    {
        ManagedBuffer b = upstream.pull();
        int len = b.length();
        if (len <= 0) return DEVICE_OK;        // guard defensivo: buffer vacio
        uint8_t *data = &b[0];
        for (int i = 0; i < len; i++) {
            re[cola] = (float)(int8_t)data[i];   // 8-bit signed
            im[cola] = 0.0f;
            cola++;
            if (cola >= FFT_N) {
                cola = 0;
                procesarFFT();
            }
        }
        return DEVICE_OK;
    }
};

static MicFft *fft = NULL;
static SplitterChannel *micCanal = NULL;   // canal del splitter (para reiniciar)
static bool activo = false;                // el mic+FFT estan corriendo ahora?

void micFftIniciar()
{
    if (fft != NULL && activo) return;     // ya esta corriendo (idempotente)

    // Ventana Hann: win[n] = sin^2(pi n / (N-1))
    if (!lista) {
        const float KPI = 3.14159265f;
        for (int n = 0; n < FFT_N; n++) {
            float s = sinf(KPI * n / (FFT_N - 1));
            win[n] = s * s;
        }
        lista = true;
    }

    // Crear un canal nuevo en el splitter de audio normalizado y
    // construir el sink con ese canal como upstream (se conecta solo)
    if (fft == NULL) {
        SplitterChannel *chan = uBit.audio.splitter->createChannel();
        if (chan == NULL) return;
        micCanal = chan;
        fft = new MicFft(*chan);
    }

    // REACTIVAR el microfono por si lo apagamos con micFftDetener():
    // runmic ON + activar el canal ADC (el watchdog de MicroBitAudio lo
    // deja consistente: si isEnabled()==false y micEnabled==false, no
    // interviene).
    uBit.audio.activateMic();

    // ARRANCAR EL FLUJO del microfono (mecanismo de CODAL):
    //  1) dataWanted se propaga hacia arriba por el pipeline hasta el
    //     canal ADC, que activa el muestreo (NRF52ADCChannel::dataWanted)
    micCanal->dataWanted(DATASTREAM_WANTED);
    //  2) El DMA del ADC SOLO emite para canales con status CONNECTED
    //     (NRF52_ADC_CHANNEL_STATUS_CONNECTED). En este build nadie llama
    //     a mic->connect() (los samples se conectan a mic->output, que es
    //     un DataStream y no setea ese flag) -> lo marcamos nosotros.
    // WORKAROUND VERIFICADO EMPIRICAMENTE (W sube ~86/s): si CODAL cambia
    // el mecanismo de status CONNECTED esto se rompe en silencio. NO tocar.
    uBit.audio.mic->connect(*fft);

    activo = true;
}

// Apaga el microfono DE VERDAD (stream + corriente):
//  1) dataWanted(NOT_WANTED) -> NRF52ADCChannel se desactiva y se libera
//     (adc.releaseChannel + disable, isEnabled()=false). Asi el watchdog
//     de MicroBitAudio ya no lo vuelve a prender solo.
//  2) deactivateMic() -> corta la corriente del microfono fisico
//     (runmic=0, micEnabled=false): el MEMS deja de gastar ~200uA.
// Seguro de llamar SIEMPRE (boot, cambio de patron, STOP): si el FFT
// nunca se inicio o ya esta apagado, es un no-op. Al volver a llamar
// micFftIniciar() se reactiva todo.
void micFftDetener()
{
    uBit.audio.mic->dataWanted(DATASTREAM_NOT_WANTED);
    uBit.audio.deactivateMic();
    activo = false;
}

const float *micFftBandas()
{
    return bandas;
}
