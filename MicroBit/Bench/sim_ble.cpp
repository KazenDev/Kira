/**
 * sim_ble.cpp - SIMULACION del puente BLE completo: las DOS capas.
 *
 * Lo que la placa tiene en el medio de un comando del celular:
 *
 *   CAPA 1  el buffer RX del BLE (MicroBitUARTService). onDataWritten mete los
 *           bytes de a uno y, si esta lleno, hace SOLO
 *               else MicroBitEvent(MICROBIT_UART_S_EVT_RX_FULL);
 *           o sea PIERDE EL BYTE, en silencio. Con 32+1 = 33 bytes entran
 *           3-4 comandos, y el celu manda 20 bytes por paquete ATT (MTU 23),
 *           asi que un comando largo ocupa dos paquetes.
 *
 *   CAPA 2  la cola de 8 lineas (BleUart.cpp). La fibra lee del buffer cada
 *           10 ms y encola; el principal saca 1 por frame (16 ms).
 *
 * Simula las dos, con la cola en version NUEVA y VIEJA, para ver donde esta
 * de verdad el cuello de botella.
 *
 * Uso:  ./sim_ble
 */
#include <stdio.h>
#include <string>
#include <vector>

// ---------------------------------------------------------------------------
// CAPA 1: el buffer RX del BLE (MicroBitUARTService.cpp:158-200)
// ---------------------------------------------------------------------------
struct BufferRxBle
{
    int tam;                       // el codigo real suma 1
    std::string b;
    int cabeza = 0, cola = 0;      // circular
    int perdidos = 0;              // bytes perdidos SIN avisar (el bug)

    BufferRxBle(int util) : tam(util + 1), b(util + 1, '\0') {}

    bool escribir(char c)
    {
        int nuevo = (cabeza + 1) % tam;
        if (nuevo != cola) {        // hay lugar
            b[cabeza] = c;
            cabeza = nuevo;
            return true;
        }
        perdidos++;                // aqui se pierde, y antes nadie lo oia
        return false;
    }

    // readUntil('\n', ASYNC): saca la linea completa si esta.
    bool leerLinea(std::string &out)
    {
        int i = cola, encontrada = -1;
        while (i != cabeza && encontrada < 0) {
            if (b[i] == '\n') encontrada = i;
            i = (i + 1) % tam;
        }
        if (encontrada < 0) return false;
        out.clear();
        for (int k = cola; k != encontrada; k = (k + 1) % tam) out += b[k];
        cola = (encontrada + 1) % tam;
        return true;
    }
};

// ---------------------------------------------------------------------------
// CAPA 2: la cola de lineas, copias literales de BleUart.cpp
// ---------------------------------------------------------------------------
#define COLA_BLE_N 8

struct ColaNueva
{
    std::string buf[COLA_BLE_N];
    int ini = 0, fin = 0, num = 0;
    int perdidos = 0;

    bool poner(const std::string &linea)
    {
        if (num >= COLA_BLE_N) {
            ini = (ini + 1) % COLA_BLE_N;    // sale la MAS VIEJA
            // num NO cambia: entra una y sale una
            perdidos++;
        } else {
            num++;
        }
        buf[fin] = linea;
        fin = (fin + 1) % COLA_BLE_N;
        return true;
    }

    bool sacar(std::string &linea)
    {
        if (num <= 0) return false;
        linea = buf[ini];
        ini = (ini + 1) % COLA_BLE_N;
        num--;
        return true;
    }
};

struct ColaVieja
{
    std::string buf[COLA_BLE_N];
    int ini = 0, fin = 0, num = 0;
    int perdidos = 0;

    bool poner(const std::string &linea)
    {
        if (num >= COLA_BLE_N) {      // se descarta lo NUEVO
            perdidos++;
            return false;
        }
        buf[fin] = linea;
        fin = (fin + 1) % COLA_BLE_N;
        num++;
        return true;
    }

    bool sacar(std::string &linea)
    {
        if (num <= 0) return false;
        linea = buf[ini];
        ini = (ini + 1) % COLA_BLE_N;
        num--;
        return true;
    }
};

// ---------------------------------------------------------------------------
// Corre la placa: la fibra lee del buffer cada 10 ms, el principal saca 1 por
// frame. El celu escribe sus comandos en los instantes indicados, en trozos de
// hasta 20 bytes (el limite del paquete ATT).
// ---------------------------------------------------------------------------
struct Resultado {
    std::vector<std::string> procesadas;
    int bytesPerdidos = 0;
    int lineasPerdidas = 0;
};

template <class Cola>
static Resultado correr(int rxBytes,
                        const std::vector<std::pair<int, std::string>> &trafico,
                        int duracion)
{
    BufferRxBle rx(rxBytes);
    Cola cola;
    Resultado r;
    size_t cursor = 0;

    for (int t = 0; t < duracion; t++) {
        // el celu escribe (en trozos de 20 bytes, como el ATT)
        while (cursor < trafico.size() && trafico[cursor].first <= t) {
            const std::string &cmd = trafico[cursor].second;
            for (size_t o = 0; o < cmd.size(); o += 20) {
                for (size_t k = o; k < cmd.size() && k < o + 20; k++)
                    rx.escribir(cmd[k]);
            }
            cursor++;
        }
        // la fibra lee el buffer cada 10 ms y encola lineas completas
        if (t % 10 == 0) {
            std::string linea;
            while (rx.leerLinea(linea))
                cola.poner(linea);
        }
        // el principal saca una por frame
        if (t % 16 == 0) {
            std::string l;
            if (cola.sacar(l)) r.procesadas.push_back(l);
        }
    }
    r.bytesPerdidos = rx.perdidos;
    r.lineasPerdidas = cola.perdidos;
    return r;
}

static void comparar(const char *nombre,
                     const std::vector<std::pair<int, std::string>> &trafico,
                     int duracion, const char *ultimoEsperado)
{
    printf("\n=== %s ===\n", nombre);
    printf("  llegan %zu comandos en %d ms\n\n", trafico.size(), duracion);
    printf("  %-30s %14s %14s %14s\n", "", "rx=32 (hoy)", "rx=32 nueva", "rx=96 nueva");
    printf("  %-30s %14s %14s %14s\n", "", "cola VIEJA", "cola NUEVA", "cola NUEVA");

    Resultado a = correr<ColaVieja>(32, trafico, duracion);
    Resultado b = correr<ColaNueva>(32, trafico, duracion);
    Resultado c = correr<ColaNueva>(96, trafico, duracion);

    auto fin = [](const Resultado &r) {
        return r.procesadas.empty() ? std::string("-") : r.procesadas.back();
    };
    printf("  %-30s %14zu %14zu %14zu\n", "procesados",
           a.procesadas.size(), b.procesadas.size(), c.procesadas.size());
    printf("  %-30s %14d %14d %14d\n", "bytes perdidos en el buffer",
           a.bytesPerdidos, b.bytesPerdidos, c.bytesPerdidos);
    printf("  %-30s %14d %14d %14d\n", "lineas descartadas en la cola",
           a.lineasPerdidas, b.lineasPerdidas, c.lineasPerdidas);
    printf("  %-30s %14s %14s %14s\n", "la cara termina en",
           fin(a).c_str(), fin(b).c_str(), fin(c).c_str());
    printf("  %-30s %14s %14s %14s\n", "deberia ser",
           ultimoEsperado, ultimoEsperado, ultimoEsperado);
    // cada columna se juzga por SEPARADO: elrx=32 termina en
    // "SENSOR:TANGRY", que no es ANGRY ni ningun comando valido.
    auto veredicto = [&](const Resultado &r) {
        std::string f = fin(r);
        if (f == ultimoEsperado) return std::string("OK");
        bool corrupto = f.find("SENSOR:") == 0;
        return corrupto ? std::string("MAL (corrupto)") : std::string("MAL");
    };
    printf("  %-30s %14s %14s %14s\n", "veredicto",
           veredicto(a).c_str(), veredicto(b).c_str(), veredicto(c).c_str());
}

int main()
{
    printf("simulacion del puente BLE completo (buffer RX + cola de lineas)\n");
    printf("el celu manda hasta 20 bytes por paquete ATT (MTU 23)\n");
    printf("la fibra lee el buffer cada 10 ms; el principal saca 1 linea cada 16 ms\n\n");

    // OJO: los comandos llevan su "\n". El firmware los arma con
    // readUntil("\n"), asi que sin el salto de linea NUNCA se completa
    // uno: es la diferencia entre la capa de bytes (ATT) y la de lineas.
    const char *em[] = {"HAPPY\n","SAD\n","ANGRY\n","SURPRISED\n",
                        "NEUTRAL\n","ANNOYED\n","SCARED\n","TIRED\n"};

    // A) conversacion normal
    {
        std::vector<std::pair<int, std::string>> v;
        for (int i = 0; i < 8; i++) v.push_back({i * 2000, em[i]});
        v.push_back({16000, "TALK\n"});
        comparar("A) conversacion normal (1 emocion cada 2 s)", v, 18000, "TALK");
    }

    // B) reconexion de la app: 9 comandos en 20 ms
    {
        std::vector<std::pair<int, std::string>> v;
        for (int i = 0; i < 8; i++) v.push_back({1000 + i * 2, em[i]});
        v.push_back({1002, "TALK\n"});
        comparar("B) reconexion: 9 comandos en 20 ms", v, 6000, "TALK");
    }

    // C) sensores que llenan, y DESPUES la emocion nueva
    {
        std::vector<std::pair<int, std::string>> v;
        for (int i = 0; i < 8; i++) v.push_back({1000 + i, "SENSOR:TEMP\n"});
        v.push_back({1010, "SAD\n"});
        v.push_back({1020, "ANGRY\n"});
        comparar("C) 8 sensores y DESPUES la emocion", v, 4000, "ANGRY");
    }

    // D) el caso que si mata bytes: respuesta de sensor larga, en rafaga
    {
        std::vector<std::pair<int, std::string>> v;
        for (int i = 0; i < 6; i++)
            v.push_back({1000 + i, "SENSOR:ACCEL\n"});
        v.push_back({1003, "ANGRY\n"});
        comparar("D) sensores largos (2 paquetes ATT) + emocion", v, 4000, "ANGRY");
    }

    return 0;
}
