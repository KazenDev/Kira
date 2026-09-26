/**
 * Grabar.h - GRABADOR DE AUDIO REAL 🎙️📼
 *
 * Captura los samples crudos del microfono (8-bit signed, 11kHz) y los
 * manda por serial a la PC para reconstruir el audio. El mismo pipeline
 * que usa MicFft (splitter de audio de CODAL) pero en vez de FFT,
 * transmite los bytes tal cual.
 *
 * Comandos serial:
 *   RECORD:<ms>   -> graba <ms> milisegundos y manda:
 *                        AUDIO:START\n
 *                        <bytes crudos 8-bit signed a 11kHz>
 *                        AUDIO:END\n
 *   ESCUCHAR/BTN A -> captura manual; el segundo A manda AUDIO:END
 *   B / CANCELAR   -> descarta y manda AUDIO:CANCEL\n
 *                    (el ACK del comando llega ANTES del AUDIO:START)
 *
 * NOTA de calidad: el micro:bit v2 muestrea a 11kHz/8-bit -> suena a
 * telefono viejo. Se entiende la voz, pero es lo-fi por diseño del MEMS.
 * La transcripcion (Whisper local) funciona bien con este audio.
 *
 * Mientras graba, ReplicaLed se calla (replicaLedCallada) para no
 * ensuciar el stream de audio con frames LED:.
 */
#ifndef GRABAR_H
#define GRABAR_H

// ¿estamos transmitiendo samples ahora? (lo consulta el sink)
extern volatile bool grabandoSerial;

// Nivel de audio del chunk actual (desviacion media, 0..~90). Lo calcula
// el sink con los samples que ya recibe: el aro de escucha manual lo usa
// en vez del FFT (que no corre durante la grabacion).
extern volatile float nivelAudio;

// ReplicaLed consulta esto para callar el serial durante la grabacion.
// OJO: esto es SOLO la causa "grabacion". La otra causa es REPLICA:OFF (ReplicaLed.h)
// y las dos se combinan con OR, no se pisan: si la app pidio silencio y
// despues alguien graba, al terminar la grabacion NO se reactiva la replica
// sola. Un unico bool no alcanza, porque cada capa lo bajaria al terminar su
// turno y se llevaria por delante la peticion de la otra.
extern volatile bool replicaLedCallada;

// Arranca el flujo: prende el mic, manda AUDIO:START y transmite crudo.
// No bloquea: los samples salen solos desde el stream de CODAL.
void grabarIniciar();

// Corta el flujo: vacia el buffer, apaga el mic (corriente incluida)
// y manda AUDIO:END para que la PC cierre el archivo.
void grabarDetener();

// Cancela el flujo sin cerrar un archivo usable: apaga el mic y manda
// AUDIO:CANCEL para que el backend descarte la captura.
void grabarCancelar();

#endif // GRABAR_H
