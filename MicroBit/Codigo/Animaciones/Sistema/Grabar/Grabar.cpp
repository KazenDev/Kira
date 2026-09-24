/**
 * Grabar.cpp - GRABADOR DE AUDIO REAL 🎙️📼
 *
 * Modelo del stream (igual que MicFft):
 *   NRF52ADCChannel (mic, 11kHz) -> DataStream
 *     -> uBit.audio.splitter -> canal nuevo -> GrabarSink::pullRequest()
 *         -> upstream.pull() (ManagedBuffer con samples 8-bit signed)
 *
 * Cada pullRequest acumula los samples en un chunk de 128 bytes y lo
 * manda por serial crudo (uBit.serial.send(uint8_t*, int), binario OK).
 * A 11kHz el micro produce ~11KB/s y el serial a 115200 baud aguanta
 * ~11.5KB/s: los chunks salen a tiempo real con poco margen.
 */
#include "Grabar.h"
#include "MicroBit.h"
#include "StreamSplitter.h"
#include "DataStream.h"

extern MicroBit uBit;

volatile bool grabandoSerial = false;
volatile bool replicaLedCallada = false;
volatile float nivelAudio = 0.0f;   // nivel del chunk (para el aro de escucha)

#define GRABAR_CHUNK 128   // bytes por envio serial (1.1ms a 115200)

// ---------------------------------------------------------------------------
// El sink: CODAL llama pullRequest() cuando hay samples nuevos del mic
// ---------------------------------------------------------------------------
class GrabarSink : public DataSink
{
    public:
    DataSource &upstream;
    uint8_t chunk[GRABAR_CHUNK];
    int n = 0;

    GrabarSink(DataSource &source) : upstream(source)
    {
        source.connect(*this);   // engancharnos al canal del splitter
    }

    virtual int pullRequest() override
    {
        if (!grabandoSerial) return DEVICE_OK;   // no grabando: no mandar nada
        ManagedBuffer b = upstream.pull();
        int len = b.length();
        if (len <= 0) return DEVICE_OK;
        uint8_t *data = &b[0];

        // Nivel del chunk: desviacion media respecto a la media (cancela
        // el offset DC). Samples 8-bit signed centrados ~0: en silencio el
        // nivel queda bajo (~2-8), con voz sube (~15-60). El aro de
        // escucha manual usa esto (el FFT no corre durante la grabacion).
        {
            long media = 0;
            for (int i = 0; i < len; i++) media += (int8_t)data[i];
            media /= len;
            long total = 0;
            for (int i = 0; i < len; i++) {
                long d = (int8_t)data[i] - media;
                if (d < 0) d = -d;
                total += d;
            }
            nivelAudio = (float)total / len;
        }

        for (int i = 0; i < len; i++) {
            chunk[n++] = data[i];
            if (n >= GRABAR_CHUNK) {
                uBit.serial.send((uint8_t *)chunk, n);
                n = 0;
            }
        }
        return DEVICE_OK;
    }
};

static GrabarSink *grabador = NULL;
static SplitterChannel *grabCanal = NULL;

// ---------------------------------------------------------------------------
// Inicio / fin de la grabacion
// ---------------------------------------------------------------------------
void grabarIniciar()
{
    if (grabandoSerial) return;    // ya grabando (idempotente)

    // Canal nuevo en el splitter + sink (igual que MicFft)
    if (grabador == NULL) {
        SplitterChannel *chan = uBit.audio.splitter->createChannel();
        if (chan == NULL) return;
        grabCanal = chan;
        grabador = new GrabarSink(*chan);
    }

    // Callar la replica LED (no queremos frames LED: en medio del audio)
    replicaLedCallada = true;

    // Avisar a la PC que empieza el flujo de audio crudo
    uBit.serial.send("AUDIO:START\n");

    // Prender el microfono fisico (runmic ON + canal ADC)
    uBit.audio.activateMic();
    grabCanal->dataWanted(DATASTREAM_WANTED);
    uBit.audio.mic->connect(*grabador);

    grabandoSerial = true;
}

static void grabarCerrar(const char *marcador)
{
    // 1) Cortar el flujo: el sink deja de mandar samples
    grabandoSerial = false;
    uBit.audio.mic->dataWanted(DATASTREAM_NOT_WANTED);
    uBit.audio.deactivateMic();   // corta la corriente del mic fisico

    // 2) Vaciar lo que quede en el chunk (con retry: mismo bug de txInUse)
    if (grabador && grabador->n > 0) {
        for (int i = 0; i < 10; i++) {
            int r = uBit.serial.send((uint8_t *)grabador->chunk, grabador->n);
            if (r != DEVICE_SERIAL_IN_USE)
                break;
            uBit.sleep(15);
        }
        grabador->n = 0;
    }

    // 3) Cerrar o cancelar el flujo para la PC. Serial::send() puede estar
    // ocupado por la fibra del mic; reintentamos hasta que quede libre.
    for (int i = 0; i < 20; i++) {
        int r = uBit.serial.send(marcador);
        if (r != DEVICE_SERIAL_IN_USE)
            break;
        uBit.sleep(25);
    }

    // 4) ReplicaLed ya puede volver a transmitir
    replicaLedCallada = false;
}

void grabarDetener()
{
    if (!grabandoSerial) return;
    grabarCerrar("AUDIO:END\n");
}

void grabarCancelar()
{
    // Si se cancela justo después de iniciar, el backend todavía espera un
    // marcador: lo enviamos aunque el sink todavía no haya producido samples.
    if (!grabandoSerial) {
        replicaLedCallada = true;
        for (int i = 0; i < 20; i++) {
            int r = uBit.serial.send("AUDIO:CANCEL\n");
            if (r != DEVICE_SERIAL_IN_USE)
                break;
            uBit.sleep(25);
        }
        replicaLedCallada = false;
        return;
    }
    grabarCerrar("AUDIO:CANCEL\n");
}
