/**
 * Transiciones.cpp - El DIRECTOR de transiciones
 *
 * Elige una transicion al azar (nunca la misma) y la ejecuta.
 * Tambien sabe dibujar el primer frame (cara base) de cada emocion
 * (las emociones exponen mostrarCaraX() para esto).
 */
#include "Transiciones.h"
#include "Morfosis/Morfosis.h"
#include "Cortina/Cortina.h"
#include "Fundido/Fundido.h"
#include "../../Emociones/Emociones.h"   // las mostrarCaraX de todas

// ---------------------------------------------------------------------------
// Dibuja el primer frame (cara base) de la emocion destino.
// Lo usan las transiciones para saber A DONDE van los pixeles / que dibujar.
// ---------------------------------------------------------------------------
void dibujarCaraDestino(EmocionActual destino)
{
    switch (destino) {
        case EM_ALEGRIA:     mostrarCaraAlegria(); break;
        case EM_TRISTE:      mostrarCaraTriste(); break;
        case EM_ENOJADO:     mostrarCaraEnojado(); break;
        case EM_SORPRENDIDO: mostrarCaraSorprendido(); break;
        case EM_NEUTRAL:     mostrarCaraNeutral(); break;
        case EM_FASTIDIO:    mostrarCaraFastidio(); break;
        case EM_MIEDO:       mostrarCaraMiedo(); break;
        case EM_CANSADO:     mostrarCaraCansado(); break;
        default:             mostrarCaraAlegria(); break;
    }
}

// ---------------------------------------------------------------------------
// Reproduce la transicion idx (0..2) hacia la emocion destino.
// ---------------------------------------------------------------------------
void mostrarTransicion(int idx, EmocionActual destino)
{
    uBit.serial.printf("TRANS:%d\n", idx);   // debug: cual salio
    switch (idx) {
        case 0: transicionMorfosis(destino); break;
        case 1: transicionCortina(destino); break;
        case 2: transicionFundido(destino); break;
        default: dibujarCaraDestino(destino); break;
    }
}

// ---------------------------------------------------------------------------
// NO ANIDAR TRANSICIONES (obligatorio, no es una optimizacion)
//
// Las transiciones ahora chequean el serial en cada frame (para no quedarse
// ~2 s sorda, que era el problema). Eso trae un riesgo que hay que cerrar: si
// un comando llega en medio de una transicion, revisarSerial() lo PROCESA
// desde adentro del frame, y si ese comando es otro cambio de emocion llama
// a hacerTransicion()... otra vez. Y otra. Y otra.
//
// El codigo de la transicion se ejecuta ENTERO en la pila del proceso
// principal, no en una fibra. Medido en el .obj: transicionMorfosis reserva
// 580 bytes de pila, y la pila util de la placa es ~2 KB. Tres niveles
// anidados son 1,86 KB: eso desborda la pila. Sin este guard, arreglar la
// latencia rompia la placa.
//
// Que pasa en vez de anidar: se anota el destino y se devuelve. La
// transicion que ya esta corriendo sigue (o se aborta, si el comando
// aparecio a mitad), y al terminar el director pinta el destino pendiente.
// La cara final es la correcta; lo que se pierde es el efecto visual de la
// transicion anidada.
// ---------------------------------------------------------------------------
static bool transicionEnCurso = false;
static bool hayDestinoPendiente = false;
static EmocionActual destinoPendiente = EM_ALEGRIA;

// ---------------------------------------------------------------------------
// Elige UNA transicion al azar (nunca carga igual dos veces).
// ---------------------------------------------------------------------------
void hacerTransicion(EmocionActual destino)
{
    if (transicionEnCurso) {
        destinoPendiente = destino;
        hayDestinoPendiente = true;
        return;
    }

    transicionEnCurso = true;
    mostrarTransicion(uBit.random(NUM_TRANSICIONES), destino);
    transicionEnCurso = false;

    if (hayDestinoPendiente) {
        hayDestinoPendiente = false;
        // clear() explicito: si la transicion se aborto a mitad, Morfosis
        // puede haber dejado pixeles sueltos en el camino. Hay que empezar
        // de una cara limpia.
        uBit.display.image.clear();
        dibujarCaraDestino(destinoPendiente);
    }
}

// ---------------------------------------------------------------------------
// Preview: muestra las 3 en secuencia (comando TRANSALL).
// Termina en alegria y la deja como emocion activa, para que el bucle
// principal NO re-renderice otra cara al instante y corte el final.
// ---------------------------------------------------------------------------
void mostrarTransicionesTodos()
{
    for (int i = 0; i < NUM_TRANSICIONES; i++)
        mostrarTransicion(i, EM_ALEGRIA);
    emocionActual = EM_ALEGRIA;
}
