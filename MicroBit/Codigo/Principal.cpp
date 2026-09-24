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
#include "Animaciones/Emociones/Emociones.h"

MicroBit uBit;

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

    // Mensaje de bienvenida (se lee con la consola serial de la PC)
    uBit.serial.send("micro:bit emociones listo\n");

    // Bucle principal: escucha serial en ASYNC (no bloquea la animacion)
    while (1) {
        // Si llego un comando de la IA, lo procesa (ACK inmediato)
        revisarSerial();

        // Renderiza la emocion ACTIVA:
        //  - TALK: la boca habla con LA BOCA DE LA EMOCION ACTIVA (los
        //    ojos los mueve su fibra en paralelo)
        //  - LOADING: el patron gira EN BUCLE hasta otra emocion o STOP
        //  - ALEGRIA / TRISTE: animaciones vivas (se abortan solas si llega
        //    un comando serial)
        //  - Las demas: cara estatica ya dibujada, solo espera un poco
        if (modoHablar) {
            if (emocionActual == EM_TRISTE)
                animarBocaTriste();
            else if (emocionActual == EM_ENOJADO)
                animarBocaEnojada();
            else if (emocionActual == EM_SORPRENDIDO)
                animarBocaSorprendida();
            else if (emocionActual == EM_NEUTRAL)
                animarBocaNeutral();
            else if (emocionActual == EM_FASTIDIO)
                animarBocaFastidio();
            else if (emocionActual == EM_MIEDO)
                animarBocaMiedo();
            else if (emocionActual == EM_CANSADO)
                animarBocaCansado();
            else
                animarBocaHablando();
        }
        else if (modoLoading)
            mostrarLoadingBucle();
        // ESCUCHA GPT: el aro reacciona a tu voz (nivel del sink de
        // grabacion) y el VAD corta solo al dejar de hablar. El frame y
        // el VAD van juntos; escucharDetener() los corta a ambos.
        else if (modoEscuchar) {
            escucharFrame();
            escucharVad();
        }
        else if (modoVoz)
            mostrarVozBucle();
        // METRONOMO: pendulo + click (fibra propia marca el pulso; aqui
        // se dibuja el vaiven a 60fps y se atienden los botones A/B)
        else if (modoMetro)
            metroFrame();
        else if (emocionActual == EM_ALEGRIA)
            animarAlegria();
        else if (emocionActual == EM_TRISTE)
            animarTriste();
        else if (emocionActual == EM_ENOJADO)
            animarEnojado();
        else if (emocionActual == EM_SORPRENDIDO)
            animarSorprendido();
        else if (emocionActual == EM_NEUTRAL)
            animarNeutral();
        else if (emocionActual == EM_FASTIDIO)
            animarFastidio();
        else if (emocionActual == EM_MIEDO)
            animarMiedo();
        else if (emocionActual == EM_CANSADO)
            animarCansado();
        else
            uBit.sleep(50);
    }
}
