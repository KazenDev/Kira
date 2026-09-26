/**
 * ReplicaLed.cpp - Retransmision del display LED real por serial.
 *
 * Corre en una FIBRA paralela: no toca el bucle principal ni las
 * animaciones. Lee los 25 pixeles del framebuffer actual y los manda
 * como "LED:<25 chars>\n".
 *
 * ADAPTATIVO (para no molestar):
 *  - Si la imagen CAMBIO desde el ultimo frame -> transmite a ~20fps
 *    (50ms): fluido para animaciones, como se ve en el micro:bit.
 *  - Si la imagen NO cambio -> duerme 400ms sin transmitir: en reposo
 *    el micro:bit no manda nada, no satura el serial y la luz amarilla
 *    de actividad deja de parpadear (solo parpadea cuando hay motion).
 */
#include "ReplicaLed.h"
#include "Grabar/Grabar.h"   // replicaLedCallada: callarse durante la grabacion
#include <string.h>

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// REPLICA:OFF / ON (ver ReplicaLed.h). Son DOS causas de silencio y se combinan
// con OR: la de la grabacion (replicaLedCallada) y la de la app (este flag).
// Con un solo bool se pisarian: el que termina su turno lo baja y se lleva
// por delante la peticion del otro. Por ejemplo: la app pide REPLICA:OFF para
// medir la latencia del puerto, y en medio alguien graba; al terminar la
// grabacion, Grabar baja replicaLedCallada y, con un solo flag, la replica
// volveria a hablar sola.
//
// No es un flag propio del "esta grabando": es un motivo mas de callarse.
// ---------------------------------------------------------------------------
static volatile bool replicaLedPedida = false;

void replicaLedSilenciar(bool silenciar)
{
    replicaLedPedida = silenciar;
}

// La fibra: lee el framebuffer y lo transmite SOLO cuando cambia
// ---------------------------------------------------------------------------
static void fibraReplica()
{
    char anterior[25];
    bool primera = true;

    while (1) {
        // Silencio por CUALQUIERA de las dos causas: el grabador esta
        // mandando audio crudo por el mismo UART, o la app pidio callarse
        // (REPLICA:OFF). En los dos casos mandar frames LED arruinaria algo.
        if (replicaLedCallada || replicaLedPedida) {
            uBit.sleep(50);
            continue;
        }
        char buf[32];
        int n = 0;
        buf[n++] = 'L';
        buf[n++] = 'E';
        buf[n++] = 'D';
        buf[n++] = ':';
        for (int y = 0; y < 5; y++) {
            for (int x = 0; x < 5; x++) {
                // '#' si el pixel esta encendido (brillo > 0), '.' si apagado
                buf[n++] = uBit.display.image.getPixelValue(x, y) > 0 ? '#' : '.';
            }
        }
        buf[n++] = '\n';

        // ¿cambio la imagen? (comparar solo los 25 pixeles)
        bool cambio = primera || memcmp(anterior, buf + 4, 25) != 0;

        if (cambio) {
            // El frame LED es TELEMETRIA: si el UART esta ocupado (lo esta el
            // ACK de un comando, que es prioridad), se pierde y esta bien. No
            // se reintenta porque pelear por el UART hacia perder ACKs, que
            // si son protocolo. Ademas la replica es adaptativa: como la
            // imagen no cambio, el proximo frame reintenta solo al proximo
            // cambio de imagen.
            if (uBit.serial.send((uint8_t *)buf, (int)n) != DEVICE_SERIAL_IN_USE) {
                memcpy(anterior, buf + 4, 25);
                primera = false;
            }
            uBit.sleep(50);   // ~20fps mientras hay movimiento
        } else {
            uBit.sleep(400);  // reposo: silencio, sin parpadeo de la luz
        }
    }
}

// ---------------------------------------------------------------------------
// Inicio: crea la fibra (una sola vez)
// ---------------------------------------------------------------------------
void iniciarReplicaLed()
{
    create_fiber(fibraReplica);
}
