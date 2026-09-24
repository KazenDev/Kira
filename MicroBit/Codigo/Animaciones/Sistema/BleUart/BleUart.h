/**
 * BleUart.h - PUENTE BLUETOOTH: los mismos comandos, por aire 📡💙
 *
 * Con MICROBIT_BLE_ENABLED=1 (codal.json), la placa ANUNCIA Bluetooth
 * BLE y este modulo levanta el UART SERVICE (serial inalambrico de
 * Nordic, el estandar que hablan nRF Connect, Web Bluetooth y casi
 * cualquier app BLE). Un celular puede conectar y mandar LOS MISMOS
 * comandos que por USB: HAPPY, METRO:90:4, SENSOR:TEMP, STOP...
 * El despachador (procesarComando) NO cambia: solo es otro canal.
 *
 * Config del build (codal.json):
 *   MICROBIT_BLE_OPEN = 1  -> sin emparejamiento: se conecta y listo
 *                             (anuncia para siempre, ideal para la feria)
 *   DFU/EVENT services = 0 -> ahorradores de RAM (no los usamos)
 *
 * Extras:
 *   - Al CONECTAR un celular suena un saludo (hello de CODAL)
 *   - Los ACKs y respuestas de sensor salen por BLE ademas del USB
 *     (helper responder() en Sistema.cpp)
 */
#ifndef BLEUART_H
#define BLEUART_H

#include "MicroBit.h"

// La instancia global del micro:bit se define en Principal.cpp
extern MicroBit uBit;

// Flag INFORMATIVO que mantienen los eventos CONNECTED/DISCONNECTED (para el
// log). ⚠ NO usar para decidir si hay enlace: tras una reconexion puede
// quedar en false mintiendo (known issue del runtime) y dejaria la placa
// sorda y muda. El camino de datos pregunta el estado real al chip
// (enlaceVivo() en BleUart.cpp).
extern bool bleConectado;

// Levanta el UART service + la fibra lectora (lo llama main UNA vez).
// Si el build no tiene BLE, no hace nada (compila igual).
void iniciarBleUart();

// Manda texto por BLE si hay enlace REAL (ACKs, respuestas de sensor).
// Si no hay celular, o el celular no se suscribio a las notificaciones,
// no manda nada (y lo deja anotado en el log USB una vez por enlace).
void bleEnviar(ManagedString texto);

// Toma UNA linea BLE completa pendiente (la dejo la fibra lectora).
// Devuelve true si habia una. La llama revisarSerial() del bucle
// principal: UN solo procesador de comandos para USB y BLE.
bool bleColaSacar(ManagedString &linea);

#endif // BLEUART_H
