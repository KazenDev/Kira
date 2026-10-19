/**
 * Principal.cpp - PUNTO DE ENTRADA del programa
 *
 * Este modulo solo ORQUESTA: inicializa la micro:bit, y en el bucle
 * principal escucha comandos serial (de la IA) y muestra la animacion
 * por defecto. Toda la logica esta repartida en los modulos:
 *
 *   Animaciones/Emociones/  -> las caras y sus animaciones
 *   Animaciones/Sistema/    -> aro de carga, demo, receptor serial
 */
#include "MicroBit.h"
#include "Animaciones/Sistema/Sistema.h"
#include "Animaciones/Sistema/Loading/Loading.h"
#include "Animaciones/Sistema/ReplicaLed.h"
#include "Animaciones/Sistema/MicFft/MicFft.h"
#include "Animaciones/Sistema/Voz/Voz.h"
#include "Animaciones/Sistema/Escuchar/Escuchar.h"
#include "Animaciones/Sistema/Metronomo/Metronomo.h"
#include "Animaciones/Sistema/BleUart/BleUart.h"
#include "Animaciones/Vida/Vida.h"
#include "Animaciones/Vida/Sonidos/Sonidos.h"

MicroBit uBit;

// Botones de la escucha manual. Se detectan flancos, no niveles, para que
// mantener A no termine la captura inmediatamente despues de iniciarla.
static bool botonAAnterior = false;
static bool botonBAnterior = false;

// NO es static: los golpes dramaticos de un disparo que se exentan del lote
// (ver LOTE_EXENTO en LoadingBase.h) tienen que atender A/B el mismo, o el
// costo de exentarse serian botones muertos. La llama el bucle principal
// una vez por frame Y la llaman los golpes largos frame por frame.
void atenderBotonesEscucha()
{
    bool a = uBit.buttonA.isPressed();
    bool b = uBit.buttonB.isPressed();
    bool pulsoA = a && !botonAAnterior;
    bool pulsoB = b && !botonBAnterior;
    botonAAnterior = a;
    botonBAnterior = b;

    // En metronomo A/B ya tienen su propio control de tempo.
    if (modoMetro) return;

    // En un minijuego A y B son del juego (no abren el microfono).
    if (vidaJugando()) {
        vidaBotonesJuego(pulsoA, pulsoB);
        return;
    }

    if (modoEscuchar) {
        if (pulsoB) {
            // B manda siempre: cancela y descarta, incluso si A+B aparecen.
            escucharCancelar();
        } else if (pulsoA) {
            if (escucharMicroActivo()) {
                // Segundo A: cerrar el stream y enviarlo al backend.
                escucharDetener();
            } else {
                // Primer A después de ESCUCHAR: abrir el micrófono.
                sonidoCallar();
                vidaOidoPausar();
                escucharIniciar();
            }
        }
    } else if (pulsoA) {
        // Primer A: abrir el microfono, apagar otros modos y mostrar el aro.
        detenerHablar();
        detenerLoading();
        detenerVoz();
        sonidoCallar();   // que el microfono no grabe a Kira
        vidaOidoPausar();
        escucharIniciar();
    }

    // B fuera de la escucha es de Kira: toque corto = picarla, mantenerlo =
    // minijuego (la escucha manda: si A abrio el microfono, B cancela).
    if (!modoEscuchar) vidaBotonB(b);
}

// ---------------------------------------------------------------------------
// MAIN: solo carga y despacha
// ---------------------------------------------------------------------------
int main()
{
    uBit.init();

    uBit.display.setBrightness(90);  // luz tenue desde el inicio

    // Fibra paralela: retransmite el frame LED real por serial (~12fps)
    // para que la web pinte una REPLICA FIEL de lo que se ve en el micro:bit
    iniciarReplicaLed();

    // BLUETOOTH: anuncia BLE y acepta los mismos comandos por aire
    // (MicroBitUARTService). Si el build no tiene BLE, no hace nada.
    iniciarBleUart();

    // CODAL enciende el microfono al boot: lo apagamos ya (stream + corriente).
    // Solo la animacion Barra lo vuelve a prender con micFftIniciar().
    micFftDetener();

    // La cara arranca viva (y saluda por el parlante)
    vidaIniciar();

    // Mensaje de bienvenida (se lee con la consola serial de la PC)
    uBit.serial.send("micro:bit emociones listo\n");

    // Bucle principal: escucha serial en ASYNC (no bloquea la animacion)
    while (1) {
        // Si llego un comando de la IA, lo procesa (ACK inmediato)
        revisarSerial();
        atenderBotonesEscucha();

        // Lo que se ve, por prioridad: LOADING, escucha manual, aro de voz y
        // metronomo son modos que toman la pantalla entera. Si no hay
        // ninguno, la CARA VIVE: el motor de vida corre la personalidad de
        // la emocion activa (y su boca, si la IA esta hablando).
        if (modoLoading)
            mostrarLoadingBucle();
        // ESCUCHA MANUAL: el aro reacciona al nivel real del microfono.
        // No se llama al VAD: A envia y B cancela mediante el handler de
        // botones de arriba.
        else if (modoEscuchar)
            escucharFrame();
        else if (modoVoz)
            mostrarVozBucle();
        // METRONOMO: pendulo + click (fibra propia marca el pulso; aqui
        // se dibuja el vaiven a 60fps y se atienden los botones A/B)
        else if (modoMetro)
            metroFrame();
        else
            vidaFrame();
    }
}
