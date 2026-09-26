/**
 * Sistema.cpp - Implementacion del modulo de SISTEMA
 * Receptor serial de emociones, demo automatica y despachador.
 * Las ANIMACIONES DE CARGA viven en su propia carpeta: Loading/Loading.cpp
 */
#include "Sistema.h"
// Ruta relativa: Sistema.cpp esta en Animaciones/Sistema/, las emociones en ../Emociones/
#include "../Emociones/Emociones.h"
#include "../Emociones/Alegria/Alegria.h"
#include "../Emociones/Alegria/Hablar.h"
#include "Loading/Loading.h"   // las animaciones de carga (patrones de luz)
#include "Loading/Luz/Luz.h"      // recalibrarLuz() para el comando CALIB
#include "Transiciones/Transiciones.h"   // efectos entre emociones (60 FPS)
#include "Grabar/Grabar.h"   // RECORD:<ms> -> audio crudo del microfono por serial
#include "Escuchar/Escuchar.h" // ESCUCHAR: escucha manual; A envía y B cancela
#include "Voz/Voz.h"           // VOZ: aro que reacciona al nivel de voz real
#include "Metronomo/Metronomo.h" // METRO:<bpm>:<acento>: metronomo para musicos
#include "BleUart/BleUart.h"   // BLE: los mismos comandos por Bluetooth
#include "ReplicaLed.h"        // REPLICA:OFF/ON -> callar la replica del display
// Funciones de lectura de sensores (tool calling de la IA)
#include "../../Funciones/Funciones.h"

// Emocion activa (la mantiene el bucle principal entre pasadas)
EmocionActual emocionActual = EM_ALEGRIA;

// ---------------------------------------------------------------------------
// Responde por TODOS los canales: USB serial siempre + BLE si hay celular
// conectado (los ACKs y las lecturas de sensor llegan por donde sea que
// vino el comando).
// ---------------------------------------------------------------------------

// Serial::send() es NO BLOQUEANTE y DESCARTA el mensaje si otra fibra esta
// transmitiendo: devuelve DEVICE_SERIAL_IN_USE y el texto se pierde
// (Serial.cpp:376). La fibra de ReplicaLed manda un frame LED cada 50 ms sin
// mirar el resultado, asi que el ACK se perdia justo cuando la cara mas se
// movia - que es cuando la IA esta hablando. Por eso MICROBIT.md decia que
// "la placa deja de responder ACK" y la unica cura era re-flashear: no era
// basura en el RX, era una colision de TX. Grabar.cpp ya reintentaba por su
// cuenta (Grabar.cpp:120); el ACK no. Ahora si.
//
// El mensaje que se manda es corto y el frame LED son 29 bytes a 115200
// (~2,5 ms), asi que 20 intentos de 2 ms alcanzan de sobra. Si aun asi no
// entra, se avisa por BLE (que va por radio y no compite con el UART).
static int enviarSerial(ManagedString texto)
{
    for (int i = 0; i < 20; i++) {
        int r = uBit.serial.send(texto);
        if (r != DEVICE_SERIAL_IN_USE)
            return r;
        uBit.sleep(2);
    }
    return DEVICE_SERIAL_IN_USE;
}

static void responder(ManagedString texto)
{
    if (enviarSerial(texto) == DEVICE_SERIAL_IN_USE) {
        // Perdido el UART: al menos que le llegue al celular por radio.
        bleEnviar(ManagedString("TX:OCUPADO:") + texto);
    } else {
        bleEnviar(texto);
    }
}

// ---------------------------------------------------------------------------
// Demo automatica (prueba del codigo sin necesitar la IA)
// ---------------------------------------------------------------------------
void demoAutomatica()
{
    for (int i = 0; i < 2; i++) {
        // La demo tambien usa las TRANSICIONES entre emociones (60 FPS)
        //
        // Alegria ahora es una FUNCION DE FRAME (una llamada = un frame), asi
        // que para que la demo la deje respirar hay que correrla un rato, no
        // llamarla una sola vez. Las otras 7 emociones siguen siendo
        // secuencias con sleeps y conservan la llamada simple.
        hacerTransicion(EM_ALEGRIA); emocionActual = EM_ALEGRIA;
        {
            uint32_t hasta = uBit.systemTime() + 1300;
            while (uBit.systemTime() < hasta) animarAlegria();
        }
        uBit.sleep(300);
        hacerTransicion(EM_TRISTE); emocionActual = EM_TRISTE; animarTriste();
        hacerTransicion(EM_ALEGRIA); emocionActual = EM_ALEGRIA; animarAlegria(); uBit.sleep(300);
        hacerTransicion(EM_ENOJADO); emocionActual = EM_ENOJADO; animarEnojado();
        hacerTransicion(EM_ALEGRIA); emocionActual = EM_ALEGRIA; animarAlegria(); uBit.sleep(300);
        hacerTransicion(EM_SORPRENDIDO); emocionActual = EM_SORPRENDIDO; animarSorprendido();
        hacerTransicion(EM_ALEGRIA); emocionActual = EM_ALEGRIA; animarAlegria(); uBit.sleep(300);
        hacerTransicion(EM_NEUTRAL); emocionActual = EM_NEUTRAL; animarNeutral();
        hacerTransicion(EM_ALEGRIA); emocionActual = EM_ALEGRIA; animarAlegria(); uBit.sleep(300);
        hacerTransicion(EM_FASTIDIO); emocionActual = EM_FASTIDIO; animarFastidio();
        hacerTransicion(EM_ALEGRIA); emocionActual = EM_ALEGRIA; animarAlegria(); uBit.sleep(300);
        hacerTransicion(EM_MIEDO); emocionActual = EM_MIEDO; animarMiedo();
        hacerTransicion(EM_ALEGRIA); emocionActual = EM_ALEGRIA; animarAlegria(); uBit.sleep(300);
        hacerTransicion(EM_CANSADO); emocionActual = EM_CANSADO; animarCansado();
        hacerTransicion(EM_ALEGRIA); emocionActual = EM_ALEGRIA; animarAlegria(); uBit.sleep(300);
        mostrarLoadingRandom(2);   // carga con patron aleatorio
    }
    // Al terminar la demo, vuelve a la alegria en reposo
    hacerTransicion(EM_ALEGRIA);
    emocionActual = EM_ALEGRIA;
}

// ---------------------------------------------------------------------------
// RX SANO: basura en el buffer del UART
//
// POR QUE hace falta. Serial::readUntil() en modo ASYNC (Serial.cpp:786)
// devuelve una cadena vacia y NO avanza rxBuffTail cuando todavia no
// encuentra el delimitador. Traducido: los bytes que llegan sin "\n" se
// quedan en el buffer PARA SIEMPRE y se pegan al comando siguiente. Un solo
// byte basura -un reset a mitad de un comando, un host que se cerro sin
// mandar el \n- contaminaba el ACK y la placa contestaba:
//
//     ACK:^\\\xf7\xf7\x94\xffSTOP        (deberia ser  ACK:STOP)
//
// El backend nunca matcheaba ese ACK, se caia el timeout, y el unico
// "arreglo" documentado era re-flashear la placa (limpia el UART de y por
// todo). El backend tambien se defendia por su lado con un parche equivalente
// ("descartar la basura del boot", serial_transport.py:278). Los dos lados
// parcheando la misma causa: el arreglo va ACA, en la fuente.
//
// QUE HACE. Dos cosas, y la segunda es la que de verdad importa:
//   1) rxLimpiar(): una linea con bytes de control no es un comando. Se
//      queda solo lo imprimible, y si no queda nada no se ejecuta nada.
//   2) rxDescartarSiVencio(): un parcial que lleva mucho tiempo esperando su
//      "\n" ya no va a llegar. Se tira entero.
//
// ASIAMBOS CANALES SE PARECEN. La fibra BLE ya hacia exactamente esto, con
// el mismo corte de 2 s (BleUart.cpp:175). La ruta USB no tenia nada. Ahora
// las dos se comportan igual, que es la misma logica que ya aplicaron para
// que un solo despachador atienda los dos canales.
// ---------------------------------------------------------------------------

// El mismo corte que usa la fibra BLE: si en 2 s no llego el "\n", fue un
// comando partido y no uno lento.
static const unsigned long RX_VENCIDO_MS = 2000;
static bool rxHayParcial = false;
static unsigned long rxParcialDesde = 0;

// El delimitador se construye UNA vez. readUntil() lo recibe por valor, pero
// el copy ctor de ManagedString solo hace ptr->incr() (ManagedString.cpp:266):
// no hay malloc. Antes se construia ManagedString("\n") en cada llamada, y ese
// constructor SI hace malloc (ManagedString.cpp:78) - un malloc+free por
// llamada, en un buffer de RAM que comparte con el stack de BLE.
static ManagedString &delimitadorRx()
{
    static ManagedString d("\n");
    return d;
}

// Tira un parcial que lleva demasiado tiempo esperando su delimitador.
static void rxDescartarSiVencio()
{
    if (uBit.serial.rxBufferedSize() <= 0) {
        rxHayParcial = false;
        return;
    }

    unsigned long ahora = uBit.systemTime();
    if (!rxHayParcial) {
        rxHayParcial = true;
        rxParcialDesde = ahora;
        return;
    }

    if ((int32_t)(ahora - rxParcialDesde) >= (int32_t)RX_VENCIDO_MS) {
        int bytes = uBit.serial.rxBufferedSize();
        // clearRxBuffer() hace rxBuffTail = rxBuffHead (Serial.cpp:1070):
        // descarta TODO lo pendiente, parcial incluido.
        uBit.serial.clearRxBuffer();
        rxHayParcial = false;
        enviarSerial(ManagedString("RX:PARCIAL:Descartados ") + ManagedString(bytes)
                     + ManagedString("\n"));
    }
}

// Deja solo lo imprimible (0x20..0x7E). Esto tambien se come el "\r" de
// Windows que antes se quitaba a mano, y descarta los bytes de control que
// CODAL no genera (0x00..0x1F y 0x7F) mas los >= 0x80, que en un char con
// signo llegan negativos. Devuelve la linea limpia y avisa si hubo basura.
static ManagedString rxLimpiar(ManagedString cmd, bool &habiaBasura)
{
    char buf[64];
    int n = 0;
    habiaBasura = false;

    for (int i = 0; i < cmd.length(); i++) {
        char c = cmd.charAt(i);
        if (c >= 0x20 && c <= 0x7E) {
            if (n < (int)sizeof(buf) - 1)
                buf[n++] = c;
        } else {
            habiaBasura = true;
        }
    }
    buf[n] = 0;
    return ManagedString(buf, n);
}

// ---------------------------------------------------------------------------
// Lee el serial: si llego un comando completo, lo procesa y devuelve true.
// ---------------------------------------------------------------------------
bool revisarSerial()
{
    // BLE PRIMERO: la fibra lectora dejo lineas completas en la cola.
    // El bucle principal es el UNICO procesador de comandos (USB y BLE):
    // asi las animaciones abortan al instante igual que con USB (los
    // `return` tempranos funcionan para los dos canales) y dos
    // transiciones jamas se dibujan encima.
    ManagedString pendienteBle;
    if (bleColaSacar(pendienteBle)) {
        procesarComando(pendienteBle);
        return true;
    }
    ManagedString line = uBit.serial.readUntil(delimitadorRx(), ASYNC);
    if (line.length() > 0) {
        rxHayParcial = false;   // una linea completa: no queda parcial suelto
        procesarComando(line);
        return true;
    }

    // No hay linea completa. Si quedo data suelta y ya es vieja, se tira:
    // un parcial sin "\n" se quedaria pegado al proximo comando para siempre.
    rxDescartarSiVencio();
    return false;
}

void procesarComando(ManagedString cmd)
{
    // RX SANO: una linea con bytes de control no es un comando. Se queda solo
    // lo imprimible (esto tambien se come el "\r" de Windows). Si la linea
    // era basura, NO se ejecuta nada y NO se manda un ACK mentiroso: el
    // comando que la IA mando nunca llego entero, asi que su ACK seria falso.
    bool habiaBasura = false;
    ManagedString limpio = rxLimpiar(cmd, habiaBasura);

    if (limpio.length() == 0) {
        enviarSerial(ManagedString("RX:VACIO:SinComando\n"));
        return;
    }

    if (habiaBasura) {
        // Diagnostico por USB (la fibra BLE ya entrega lineas limpias, asi
        // que si esto salta venia del serial de la PC).
        enviarSerial(ManagedString("RX:BASURA:Limpio=") + limpio + ManagedString("\n"));
    }

    cmd = limpio;

    // ACK INMEDIATO a la IA: confirma que el comando llego, ANTES de
    // correr la animacion (que puede tardar 2-6s). Asi la IA siempre
    // recibe respuesta al instante y nada se siente "desincronizado".
    responder(ManagedString("ACK:" + cmd + "\n"));

    if (cmd == "TALK") {
        // EL METRÓNOMO MANDA 🎵: si el péndulo corre, la confirmación de
        // voz de la IA suena en el cel pero la placa SIGUE de metrónomo
        // (antes la boca mataba el metro al nacer: la IA lo encendía y su
        // propia confirmación HAPPY+TALK lo apagaba al instante).
        if (modoMetro)
            return;
        // El habla es lo UNICO que deja la fibra de ojos viva.
        // Cualquier otra cosa (incluido el loading) se detiene.
        // CADA emocion tiene SU hablar: tristeza habla triste, enojo
        // gruñe, sorpresa exclama "OH", neutral parpadea los 3 LEDs;
        // el resto usa la alegria.
        detenerLoading();
        detenerVoz();   // si estaba el aro de voz, lo apaga (y el mic)
        metroDetener(); // si estaba el metronomo, lo corta (TALK manda)
        if (emocionActual == EM_TRISTE)
            iniciarHablarTriste();
        else if (emocionActual == EM_ENOJADO)
            iniciarHablarEnojado();
        else if (emocionActual == EM_SORPRENDIDO)
            iniciarHablarSorprendido();
        else if (emocionActual == EM_NEUTRAL)
            iniciarHablarNeutral();
        else if (emocionActual == EM_FASTIDIO)
            iniciarHablarFastidio();
        else if (emocionActual == EM_MIEDO)
            iniciarHablarMiedo();
        else if (emocionActual == EM_CANSADO)
            iniciarHablarCansado();
        else
            iniciarHablar();
    }
    else if (cmd == "CALIB") {
        // Re-calibra el sensor de luz del patron 9 SIN cortar el
        // loading: la proxima pasada de animarLuz pedira tapar/iluminar.
        recalibrarLuz();
    }
    // CALIBRAR:BRUJULA -> ejecuta la UX oficial de CODAL. Es un comando
    // manual, no una tool: puede tardar hasta ~32 s y por eso no se dispara
    // desde una consulta puntual de la IA.
    else if (cmd == "CALIBRAR:BRUJULA") {
        detenerHablar();
        detenerLoading();
        detenerVoz();
        escucharDetener();
        metroDetener();
        int resultado = uBit.compass.calibrate();
        if (resultado == DEVICE_OK) {
            responder(ManagedString("BRUJULA:CALIBRADA\n"));
        } else {
            responder(ManagedString("BRUJULA:CALIBRACION:ERROR\n"));
        }
    }
    // ESCUCHAR -> ESCUCHA MANUAL: deja la placa armada y muestra el aro.
    // El primer A abre el micro; el segundo A envía; B cancela.
    else if (cmd == "ESCUCHAR") {
        metroDetener();
        escucharArmar();
    }
    // RECORD:<ms> -> GRABADOR: manda el audio crudo del microfono por
    // serial (AUDIO:START, bytes 8-bit a 11kHz, AUDIO:END). Bloquea el
    // bucle principal esos ms (nada mas que hacer); el stream del mic
    // corre en su propia fibra y la replica LED se calla sola.
    else if (cmd.substring(0, 7) == "RECORD:") {
        int ms = 0;
        for (int i = 7; i < cmd.length(); i++) {
            char c = cmd.charAt(i);
            if (c < '0' || c > '9') break;
            ms = ms * 10 + (c - '0');
        }
        if (ms > 0 && ms <= 20000) {
            grabarIniciar();
            uBit.sleep(ms);
            grabarDetener();
        }
    }
    // CANCELAR: el cancelado manual del frontend (o un comando remoto)
    // descarta la captura actual sin cerrar un AUDIO:END. La cancelación
    // física por B llama directamente a escucharCancelar().
    else if (cmd == "CANCELAR" || cmd == "CANCEL") {
        if (modoEscuchar)
            escucharCancelar();
        detenerHablar();
        detenerLoading();
        detenerVoz();
        metroDetener();
    }
    // SENSOR:* -> herramientas de la IA: leen un sensor REAL y responden
    // por serial con el valor (TEMP:24, LUZ:120, BOTON:1:0, ACCEL:...,
    // MIC:..., BAT:..., GESTO:..., BRUJULA:... o TOUCH:...). No cambian
    // la emocion; la luz puede usar la matriz un instante y el microfono solo
    // se enciende durante su muestra.
    else if (cmd.substring(0, 7) == "SENSOR:") {
        ManagedString sensor = cmd.substring(7, cmd.length());
        if (sensor == "TEMP") {
            responder(ManagedString("TEMP:") + leerTemperatura() + "\n");
        }
        else if (sensor == "LUZ") {
            responder(ManagedString("LUZ:") + leerLuz() + "\n");
        }
        else if (sensor == "BOTON") {
            int b = leerBotones();
            responder(ManagedString("BOTON:") + (b & 1 ? "1" : "0") + ":" + (b & 2 ? "1" : "0") + "\n");
        }
        else if (sensor == "ACCEL") {
            LecturaAcelerometro l = leerAcelerometro();
            responder(ManagedString("ACCEL:") + l.x + ":" + l.y + ":" + l.z
                             + ":" + l.pitch + ":" + l.roll + "\n");
        }
        else if (sensor == "MIC" || sensor == "SONIDO") {
            LecturaMicrofono l = leerMicrofono();
            if (l.estado == 0) {
                responder(ManagedString("MIC:BUSY\n"));
            }
            else if (l.estado < 0) {
                responder(ManagedString("MIC:ERR\n"));
            }
            else {
                ManagedString salida = ManagedString("MIC:");
                salida = salida + l.nivel;
                for (int i = 0; i < 5; i++) {
                    salida = salida + ManagedString(":");
                    salida = salida + l.bandas[i];
                }
                salida = salida + ManagedString(":") + l.ventanas + ManagedString("\n");
                responder(salida);
            }
        }
        else if (sensor == "BAT" || sensor == "BATERIA") {
            LecturaBateria l = leerBateria();
            ManagedString salida = ManagedString("BAT:");
            salida = salida + l.bateria_mv;
            salida = salida + ManagedString(":") + l.vin_mv;
            salida = salida + ManagedString(":") + l.fuente + ManagedString("\n");
            responder(salida);
        }
        else if (sensor == "GESTO" || sensor == "GESTOS") {
            LecturaGesto l = leerGesto();
            ManagedString salida = ManagedString("GESTO:");
            salida = salida + l.codigo;
            salida = salida + ManagedString(":") + l.magnitud_mg + ManagedString("\n");
            responder(salida);
        }
        else if (sensor == "BRUJULA" || sensor == "COMPASS" ||
                 sensor == "MAG" || sensor == "MAGNETO") {
            LecturaBrujula l = leerBrujula();
            ManagedString salida = ManagedString("BRUJULA:");
            salida = salida + l.rumbo;
            salida = salida + ManagedString(":") + l.campo;
            salida = salida + ManagedString(":") + l.calibrada + ManagedString("\n");
            responder(salida);
        }
        else if (sensor == "TOQUE" || sensor == "TACTIL" || sensor == "LOGO") {
            LecturaTactil l = leerToque();
            ManagedString salida = ManagedString("TOUCH:");
            salida = salida + l.presionado;
            salida = salida + ManagedString(":") + l.lectura + ManagedString("\n");
            responder(salida);
        }
        else {
            responder(ManagedString("SENSOR:?\n"));
        }
    }
    else {
        // Cualquier OTRO comando cancela el habla Y el loading (y el aro
        // de voz, la escucha en curso y el metronomo): la fibra muere en
        // su siguiente ciclo y la boca/loading/pendulo paran. El microfono
        // se apaga de verdad.
        detenerHablar();
        detenerLoading();
        detenerVoz();
        escucharDetener();
        metroDetener();

        // Las emociones CAMBIAN la emocion activa (persiste en el bucle).
        // ANTES de cambiar se reproduce una TRANSICION aleatoria (60 FPS)
        // que deja el primer frame de la cara nueva dibujado; el bucle
        // principal (Principal.cpp) sigue animando desde ahi.
        if (cmd == "HAPPY") {
            hacerTransicion(EM_ALEGRIA);
            emocionActual = EM_ALEGRIA;
        }
        else if (cmd == "SAD") {
            hacerTransicion(EM_TRISTE);
            emocionActual = EM_TRISTE;
        }
        else if (cmd == "ANGRY") {
            hacerTransicion(EM_ENOJADO);
            emocionActual = EM_ENOJADO;
        }
        else if (cmd == "SURPRISED") {
            hacerTransicion(EM_SORPRENDIDO);
            emocionActual = EM_SORPRENDIDO;
        }
        else if (cmd == "NEUTRAL") {
            hacerTransicion(EM_NEUTRAL);
            emocionActual = EM_NEUTRAL;
        }
        else if (cmd == "FASTIDIO" || cmd == "ANNOYED") {
            hacerTransicion(EM_FASTIDIO);
            emocionActual = EM_FASTIDIO;
        }
        else if (cmd == "MIEDO" || cmd == "SCARED") {
            hacerTransicion(EM_MIEDO);
            emocionActual = EM_MIEDO;
        }
        else if (cmd == "CANSADO" || cmd == "TIRED") {
            hacerTransicion(EM_CANSADO);
            emocionActual = EM_CANSADO;
        }
        // LOADING: BUCLE INFINITO (repite un patron al azar) hasta que
        // llegue otra emocion o STOP. Transitorio: no cambia la emocion.
        // VOZ: aro centrado que CRECE con la fuerza de tu voz y respira
        // en silencio (mic real, 60fps). Bucle hasta otra emocion o STOP.
        // "VOZ" elige una variante al azar; "VOZ0".."VOZ4" una especifica.
        else if (cmd == "VOZ") iniciarVoz();
        else if (cmd.length() == 4 && cmd.substring(0, 3) == "VOZ") {
            int idx = cmd.charAt(3) - '0';
            if (idx >= 0 && idx < 5)
                iniciarVoz(idx);
            else {
                uBit.display.print("?");
                uBit.sleep(500);
            }
        }
        else if (cmd == "LOADING") iniciarLoading();
        else if (cmd == "LOADALL") mostrarLoadingTodos(1);  // preview de todos
        // LOAD0..LOAD9: BUCLE CONTINUO con ese patron especifico
        // (gira sin reinicios hasta que llegue otra emocion o STOP)
        else if (cmd.length() == 5 && cmd.substring(0, 4) == "LOAD") {
            int idx = cmd.charAt(4) - '0';
            if (idx >= 0 && idx < 10)
                iniciarLoading(idx);
            else {
                uBit.display.print("?");
                uBit.sleep(500);
            }
        }
        // METRO:<bpm>:<acento> -> METRONOMO para musicos: pendulo visual +
        // click por el parlante (agudo en el acento). "METRO:STOP" lo corta
        // (vuelve a la alegria, como STOP). Los detener de arriba ya
        // pararon cualquier otro modo; el metronomo arranca limpio.
        else if (cmd.substring(0, 6) == "METRO:") {
            ManagedString resto = cmd.substring(6, cmd.length());
            if (resto == "STOP") {
                hacerTransicion(EM_ALEGRIA);
                emocionActual = EM_ALEGRIA;
            }
            else {
                // Parsear "<bpm>[:<acento>]" (lo mismo que hace RECORD)
                int bpm = 0, acento = 4;
                int i = 0;
                while (i < resto.length() && resto.charAt(i) >= '0' && resto.charAt(i) <= '9')
                    bpm = bpm * 10 + (resto.charAt(i++) - '0');
                if (i < resto.length() && resto.charAt(i) == ':') {
                    i++;
                    acento = 0;
                    while (i < resto.length() && resto.charAt(i) >= '0' && resto.charAt(i) <= '9')
                        acento = acento * 10 + (resto.charAt(i++) - '0');
                }
                if (bpm > 0)
                    metroIniciar(bpm, acento);
                else {
                    uBit.display.print("?");
                    uBit.sleep(500);
                }
            }
        }
        else if (cmd == "REPLICA:OFF") replicaLedSilenciar(true);
        else if (cmd == "REPLICA:ON")  replicaLedSilenciar(false);
        else if (cmd == "TEST")  demoAutomatica();
        // STOP: vuelve a la alegria (con transicion, como cualquier cambio)
        else if (cmd == "STOP")  {
            hacerTransicion(EM_ALEGRIA);
            emocionActual = EM_ALEGRIA;
        }
        // CALLA: la boca para AL INSTANTE y la cara queda quieta en su
        // emocion actual, SIN transicion. Lo manda el frontend cuando el
        // audio TTS termina: antes la boca quedaba en bucle infinito
        // hasta el proximo comando (la cara hablaba sola en silencio).
        else if (cmd == "CALLA") {
            detenerHablar();
            dibujarCaraDestino(emocionActual);
        }
        // TRANS: una transicion aleatoria de prueba (sin cambiar de emocion)
        else if (cmd == "TRANS") hacerTransicion(emocionActual);
        // TRANSALL: preview de las 3 transiciones en secuencia
        else if (cmd == "TRANSALL") mostrarTransicionesTodos();
        // TRANS0/1/2: fuerza una transicion especifica (para probar cada una)
        else if (cmd == "TRANS0") mostrarTransicion(0, emocionActual);
        else if (cmd == "TRANS1") mostrarTransicion(1, emocionActual);
        else if (cmd == "TRANS2") mostrarTransicion(2, emocionActual);
        else if (cmd == "BLINK") {
            // Parpadeo simple: apaga y enciende los ojos
            uBit.display.image.setPixelValue(1, 1, 0);
            uBit.display.image.setPixelValue(3, 1, 0);
            uBit.sleep(150);
            uBit.display.image.setPixelValue(1, 1, 255);
            uBit.display.image.setPixelValue(3, 1, 255);
        }
        else {
            // Comando desconocido: parpadeo de error
            uBit.display.print("?");
            uBit.sleep(500);
        }
    }
}
