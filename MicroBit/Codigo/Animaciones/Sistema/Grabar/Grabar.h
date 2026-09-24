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
// el sink con los samples que ya recibe: el VAD de la escucha GPT lo usa
// en vez del FFT (que no corre durante la grabacion).
extern volatile float nivelAudio;

// ReplicaLed consulta esto para callar el serial durante la grabacion
extern volatile bool replicaLedCallada;

// Arranca el flujo: prende el mic, manda AUDIO:START y transmite crudo.
// No bloquea: los samples salen solos desde el stream de CODAL.
void grabarIniciar();

// Corta el flujo: vacia el buffer, apaga el mic (corriente incluida)
// y manda AUDIO:END para que la PC cierre el archivo.
void grabarDetener();

#endif // GRABAR_H
