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
// Elige UNA transicion al azar (nunca carga igual dos veces).
// ---------------------------------------------------------------------------
void hacerTransicion(EmocionActual destino)
{
    mostrarTransicion(uBit.random(NUM_TRANSICIONES), destino);
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
