/**
 * BleUart.cpp - Implementacion del puente Bluetooth (UART service)
 *
 * Tres piezas:
 *   1. EL SERVICIO: new MicroBitUARTService(*uBit.ble, 32, 32) levanta el
 *      "serial inalambrico" (Nordic UART Service, UUID 6e400001-...).
 *      Ese UUID es el estandar de facto: nRF Connect, Web Bluetooth de
 *      Chrome y las apps BLE lo reconocen sin configurar nada.
 *   2. LA FIBRA (fibraBle): lee lineas del UART BLE (hasta \n) y las
 *      manda al MISMO despachador que el USB (procesarComando). Da igual
 *      por donde llegue el comando: la placa obedece igual.
 *   3. LOS EVENTOS: el message bus avisa MICROBIT_BLE_EVT_CONNECTED /
 *      EVT_DISCONNECTED; ahi cantamos el saludo.
 *
 * --- 17-sep (tarde): el flag de los eventos MENTIA (arreglado) ---
 * El sintoma: la placa recibia los comandos pero no contestaba NADA (28
 * comandos mandados, 1 sola respuesta) y habia que resetearla. La doc
 * oficial del runtime describe EXACTAMENTE el caso:
 *
 *   "If the connected application loses its connection and then reconnects,
 *    the onConnected method will not execute and therefore the 'connected'
 *    variable which tracks the Bluetooth connection state will not update.
 *    The micro:bit application will now behave as though it is not in a
 *    connection" (microbit-docs, UART Service - Known Issue)
 *
 * O sea: tras una reconexion el flag podia quedar en false PARA SIEMPRE y
 * `fibraBle` no leia nada (placa SORDA) y `bleEnviar` no mandaba nada (placa
 * MUDA). Ahora el estado se le pregunta al chip (enlaceVivo()), que es lo
 * que hace el propio MicroBitUARTService por dentro. Bonus: al detectar un
 * enlace nuevo la placa manda un HELLO por aire, que ademas prueba que el
 * camino de VUELTA funciona.
 *
 * NOTA de memoria: el stack BLE (SoftDevice de Nordic) es "memory
 * hungry" (dixit Lancaster): por eso el codal.json apaga DFU y EVENT
 * services y deja solo Device Info + este UART.
 */
#include "BleUart.h"

bool bleConectado = false;

#if CONFIG_ENABLED(DEVICE_BLE)

#include <stdint.h>

static MicroBitUARTService *bleUart = NULL;

// Para avisar UNA vez por enlace si el celular no habilito las notificaciones
static bool avisadoSinSuscriptor = false;

/** ¿Hay un celular conectado AHORA? Se lo preguntamos al SoftDevice, no al
 * flag de los eventos (que puede quedar mintiendo tras una reconexion: ver
 * la nota de arriba). El propio MicroBitUARTService usa esto internamente:
 * getConnected() === ble_conn_state_peripheral_conn_count() > 0. */
static bool enlaceVivo()
{
    return bleUart != NULL && bleUart->getConnected();
}

// ---------------------------------------------------------------------------
// COLA DE COMANDOS BLE: la fibra lectora SOLO encola lineas COMPLETAS
// (terminadas en \n); el bucle principal las EJECUTA en orden via
// revisarSerial(). UN solo procesador de comandos para USB y BLE: la
// carrera entre fibras (transiciones dibujando encima de animaciones)
// desaparece para siempre.
//
// POLITICA DE DESCARTE (cambiada; ver bleColaPoner). Lo que viaja por BLE es
// el ESTADO ACTUAL del personaje, no una lista de pendientes: cuando la app
// reconecta puede mandar una rafaga (estado + TALK + sensores). Si al llenarse
// se descartara la linea NUEVA, se perderian justo las ordenes mas recientes
// -la emocion que el usuario acaba de pedir- y la cara se quedaria con la
// ANTERIOR. Verificado con Bench/sim_ble.cpp: 8 sensores seguidos de SAD y
// ANGRY; con la politica vieja se procesaba SAD y se peredia ANGRY. Con la de
// ahora sobrevive la ultima intencion.
//
// TAMANO: 8 de HOLGA son 128 ms (el principal drena 1 cada 16 ms), asi que
// hace falta mandar mas de 8 comandos en menos de 128 ms para que desborde. La
// simulacion confirma que con trafico normal (1 emocion cada 2 s) y con una
// reconexion de 9 comandos en 20 ms NO se descarta nada.
//
// NOTA SOBRE CARRERAS: bleColaPoner NO tiene ningun punto de yield adentro
// (ni fiber_sleep ni llamada bloqueante; el ManagedString es un refcount), y
// el scheduler de CODAL es cooperativo de un solo core, asi que el
// check-then-act es atomico con respecto al principal. No hace falta nada mas.
// ---------------------------------------------------------------------------
#define COLA_BLE_N 8
static ManagedString colaBle[COLA_BLE_N];
static volatile int colaBleIni = 0;   // indice a sacar (bucle principal)
static volatile int colaBleFin = 0;   // indice a poner (fibra lectora)
static volatile int colaBleNum = 0;   // cuantas hay pendientes

// Contadores para que un descarte no sea INVISIBLE. Antes el llamador
// ignoraba el return de bleColaPoner y nadie se enteraba nunca.
static int  descartadosBle = 0;
static bool avisadoDescarte = false;

static bool bleColaPoner(ManagedString linea)
{
    if (colaBleNum >= COLA_BLE_N) {
        // Cola llena: sale la MAS VIEJA y entra la nueva. Al estar llena,
        // ini == fin, asi que avanzar ini libera el slot donde se guarda
        // esta: la cola queda con las ultimas COLA_BLE_N lineas.
        colaBleIni = (colaBleIni + 1) % COLA_BLE_N;
        // OJO: colaBleNum NO se toca. Entra una linea y sale otra, asi que la
        // cuenta sigue igual. Bajarla sin su ++ hacia que la cuenta bajara en
        // cada desborde y que el siguiente store sobreescribiera un slot sin
        // leer: eso lo agarro Bench/sim_ble.cpp.
        descartadosBle++;
        if (!avisadoDescarte) {
            avisadoDescarte = true;
            uBit.serial.send("BLE: cola llena, se descarto lo MAS VIEJO\n");
        }
    } else {
        colaBleNum++;
    }
    colaBle[colaBleFin] = linea;
    colaBleFin = (colaBleFin + 1) % COLA_BLE_N;
    return true;               // ahora siempre se acepta: la nueva gana
}

bool bleColaSacar(ManagedString &linea)
{
    if (colaBleNum <= 0)
        return false;
    linea = colaBle[colaBleIni];
    colaBleIni = (colaBleIni + 1) % COLA_BLE_N;
    colaBleNum--;
    return true;
}

// ---------------------------------------------------------------------------
// Eventos de conexion (message bus)
// ---------------------------------------------------------------------------
static void alConectar(MicroBitEvent)
{
    // OJO: esto es solo INFORMATIVO (para el log). Nada del camino de datos
    // depende de este flag: el estado real lo da enlaceVivo().
    bleConectado = true;

    // ⚠ SIN SONIDO AQUI: el sintetizador hace asignaciones gordas y, con
    // el heap al limite (~2KB), un OOM aqui congela el chip y TUMBA la
    // conexion BLE recien nacida (el telefono la ve caer en el discovery).
    // El saludo sonoro queda para cuando haya RAM holgada; el vinculo se
    // avisa por USB y el propio chat confirma que llega.

    uBit.serial.send("BLE conectado\n");
}

static void alDesconectar(MicroBitEvent)
{
    bleConectado = false;
    // Resumen de lo que se perdio durante este enlace. Con la cola de 8
    // deberia ser 0 siempre; si aparece, la app esta mandando rafagas y
    // conviene agrandarla (COLA_BLE_N) en vez de adivinar.
    if (descartadosBle > 0) {
        uBit.serial.send("BLE: se descartaron ");
        uBit.serial.send(ManagedString(descartadosBle));
        uBit.serial.send(" lineas por cola llena\n");
    }
    uBit.serial.send("BLE desconectado\n");
}

// ---------------------------------------------------------------------------
// La fibra lectora: lineas BLE completas -> COLA (no ejecuta nada).
// Solo se procesan lineas TERMINADAS en \n. Un trozo parcial espera su
// resto; si pasan 2s sin completarse (trozo perdido por interferencia),
// se DESCARTA: jamas se ejecuta un comando a medias. Asi "METRO:1" +
// retraso + "20:4" no genera ni un metronomo fantasma ni un "?" en
// pantalla, a diferencia del flush anterior (que los ejecutaba).
// ---------------------------------------------------------------------------
static void fibraBle()
{
    uint32_t parcialDesde = 0;   // cuando aparecio el primer byte pendiente
    uint32_t ultimoSaludo = 0;   // para reintentar el HELLO sin apurar la fibra
    bool enlaceAntes = false;    // para detectar la TRANSICION de enlace
    bool saludado = false;       // ¿ya probamos el camino de vuelta?

    while (1) {
        // ESTADO REAL (no el flag de los eventos: ver la nota del encabezado).
        if (!enlaceVivo()) {
            if (enlaceAntes) {
                // Se cayo el enlace: la proxima conexion vuelve a saludar y
                // no arrastramos medio comando del enlace anterior.
                enlaceAntes = false;
                saludado = false;
                parcialDesde = 0;
            }
            uBit.sleep(100);
            continue;
        }

        if (!enlaceAntes) {
            // ENLACE NUEVO: aunque los eventos no hayan avisado nada (el bug),
            // limpiamos la basura del enlace anterior y lo dejamos en el log.
            enlaceAntes = true;
            saludado = false;
            parcialDesde = 0;
            avisadoSinSuscriptor = false;
            // El aviso de descarte es POR ENLACE, igual que el de "el celu
            // no escucha": con una rafaga se avisa una vez, no 8.
            avisadoDescarte = false;
            descartadosBle = 0;
            if (bleUart->rxBufferedSize() > 0)
                bleUart->read(bleUart->rxBufferedSize(), ASYNC);
            uBit.serial.send("BLE: enlace vivo (estado real)\n");
        }

        // SALUDO: sale SOLO si el celular ya se suscribio a las notificaciones
        // (send() devuelve MICROBIT_NOT_SUPPORTED si no), asi que cuando sale
        // tenemos la PRUEBA de que el camino de vuelta funciona. Se reintenta
        // cada 500ms hasta lograrlo (el celu puede tardar en suscribirse).
        if (!saludado && (int32_t)(uBit.systemTime() - ultimoSaludo) >= 500) {
            ultimoSaludo = uBit.systemTime();
            if (bleUart->send(ManagedString("HELLO\n"), ASYNC) > 0)
                saludado = true;
        }

        ManagedString linea = bleUart->readUntil(ManagedString("\n"), ASYNC);
        if (linea.length() > 0) {
            bleColaPoner(linea);
            parcialDesde = 0;
        }
        else if (bleUart->rxBufferedSize() > 0) {
            if (parcialDesde == 0)
                parcialDesde = uBit.systemTime();
            else if ((int32_t)(uBit.systemTime() - parcialDesde) >= 2000) {
                bleUart->read(bleUart->rxBufferedSize(), ASYNC);   // tirar
                parcialDesde = 0;
            }
        }
        else {
            parcialDesde = 0;
        }

        uBit.sleep(10);
    }
}

// ---------------------------------------------------------------------------
// Inicio: servicio + eventos + fibra (una sola vez, desde main)
// ---------------------------------------------------------------------------
void iniciarBleUart()
{
    // rx/tx de 32 bytes: sobra para comandos como "METRO:120:4\n" (12 bytes)
    bleUart = new MicroBitUARTService(*uBit.ble, 32, 32);

    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_CONNECTED,    alConectar);
    uBit.messageBus.listen(MICROBIT_ID_BLE, MICROBIT_BLE_EVT_DISCONNECTED, alDesconectar);

    create_fiber(fibraBle);
}

// ---------------------------------------------------------------------------
// bleEnviar: texto por aire (lo usa responder() de Sistema.cpp)
// ---------------------------------------------------------------------------
void bleEnviar(ManagedString texto)
{
    // El estado REAL, no el flag (si el flag mintio, la placa quedaba muda).
    if (!enlaceVivo())
        return;

    int escrito = bleUart->send(texto, ASYNC);

    // Conectado pero SIN suscripcion del celular (CCCD): la respuesta se
    // perderia en el silencio total. Lo dejamos anotado UNA vez por enlace
    // en el log USB (el arreglo del lado del celu es re-suscribirse).
    if (escrito == MICROBIT_NOT_SUPPORTED && !avisadoSinSuscriptor) {
        avisadoSinSuscriptor = true;
        uBit.serial.send("BLE: el celu no escucha (falta suscripcion)\n");
    }
}

#else
// ---------------------------------------------------------------------------
// Build SIN bluetooth: todo no-op, el resto del codigo no se entera
// ---------------------------------------------------------------------------
void iniciarBleUart() {}
void bleEnviar(ManagedString) { (void)0; }

#endif // DEVICE_BLE
