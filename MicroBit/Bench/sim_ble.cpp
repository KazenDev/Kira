/**
 * sim_ble.cpp - SIMULACION del puente BLE: la fibra lectora vs. el bucle principal.
 *
 * Modela la interaccion REAL de Codigo/Animaciones/Sistema/BleUart/:
 *
 *   - la FIBRA (fibraBle) consulta el UART cada 10 ms (fiber_sleep(10) al final
 *     de cada vuelta) y encola lineas completas con bleColaPoner()
 *   - el BUCLE PRINCIPAL saca una linea por frame (cada 16 ms) via
 *     revisarSerial() -> bleColaSacar()
 *
 * Las dos colas de abajo son COPIAS LITERALES de BleUart.cpp: la version
 * nueva (la que esta en el firmware ahora) y la vieja (descartar lo NUEVO),
 * para poder comparar cual de las dos hace lo que uno quiere.
 *
 * Lo que mide:
 *   1) cuantos comandos LLEGAN, cuantos se PROCESAN y cuantos se PIERDEN
 *   2) cual es el ULTIMO procesado, que es lo que la cara muestra al final
 *   3) si la politica vieja deja la cara en un estado VIEJO
 *
 * Uso:  ./sim_ble
 */
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#define COLA_BLE_N 8

// ---------------------------------------------------------------------------
// La cola NUEVA, palabra por palabra de BleUart.cpp:
//   if (num >= N) { ini = (ini+1)%N; num--; descartados++; }  else num++;
//   buf[fin] = linea;  fin = (fin+1)%N;  return true;
// ---------------------------------------------------------------------------
struct ColaNueva
{
    std::string buf[COLA_BLE_N];
    int ini = 0, fin = 0, num = 0;
    int perdidos = 0;

    bool poner(const std::string &linea)
    {
        if (num >= COLA_BLE_N) {
            ini = (ini + 1) % COLA_BLE_N;
            // num NO cambia: sale una por la cola y entra una. (Un num-- sin
            // su ++ hacia que la cuenta bajara y el siguiente store
            // sobreescribiera un slot sin leer: eso lo agarro la simulacion.)
            perdidos++;
        } else {
            num++;
        }
        buf[fin] = linea;
        fin = (fin + 1) % COLA_BLE_N;
        return true;                  // siempre se acepta: la nueva gana
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
// La cola VIEJA: al llenarse descarta lo NUEVO. Es la que deja la cara
// mostrando la emocion anterior.
// ---------------------------------------------------------------------------
struct ColaVieja
{
    std::string buf[COLA_BLE_N];
    int ini = 0, fin = 0, num = 0;
    int perdidos = 0;
    std::string perdido;

    bool poner(const std::string &linea)
    {
        if (num >= COLA_BLE_N) {          // se descarta lo NUEVO
            perdidos++;
            perdido = linea;
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
// Corre los dos actores con el reloj virtual. El bucle principal saca UNA
// linea cada 16 ms, igual que la placa.
// ---------------------------------------------------------------------------
template <class Cola>
static void correr(Cola &c,
                   const std::vector<std::pair<int, std::string>> &trafico,
                   int duracion,
                   std::vector<std::string> &procesadas)
{
    size_t cursor = 0;
    for (int t = 0; t < duracion; t++) {
        while (cursor < trafico.size() && trafico[cursor].first <= t)
            c.poner(trafico[cursor++].second);
        if (t % 16 == 0) {
            std::string l;
            if (c.sacar(l)) procesadas.push_back(l);
        }
    }
}

static void escenario(const char *nombre,
                     const std::vector<std::pair<int, std::string>> &trafico,
                     int duracion, const char *ultimoEsperado)
{
    printf("\n=== %s ===\n", nombre);
    printf("  llegan %zu comandos en %d ms\n", trafico.size(), duracion);

    ColaVieja  v;
    ColaNueva  n;
    std::vector<std::string> pv, pn;
    correr(v, trafico, duracion, pv);
    correr(n, trafico, duracion, pn);

    printf("  %-26s %10s %10s\n", "", "VIEJA", "NUEVA");
    printf("  %-26s %10zu %10zu\n", "procesados", pv.size(), pn.size());
    printf("  %-26s %10d %10d\n", "descartados", v.perdidos, n.perdidos);
    printf("  %-26s %10s %10s\n", "ultimo procesado",
           pv.empty() ? "-" : pv.back().c_str(),
           pn.empty() ? "-" : pn.back().c_str());
    printf("  %-26s %10s %10s\n", "la cara termina en",
           pv.empty() ? "-" : pv.back().c_str(),
           pn.empty() ? "-" : pn.back().c_str());
    printf("  %-26s %10s %10s\n", "deberia terminar en", ultimoEsperado, ultimoEsperado);
    bool okV = !pv.empty() && pv.back() == ultimoEsperado;
    bool okN = !pn.empty() && pn.back() == ultimoEsperado;
    printf("  %-26s %10s %10s\n", "acierta?",
           okV ? "si" : "NO", okN ? "si" : "NO");
}

int main()
{
    printf("simulacion del puente BLE (colas = copias literales de BleUart.cpp)\n");
    printf("la fibra encola cada 10 ms; el principal drena 1 cada 16 ms\n");
    printf("holgura de la cola: 8 comandos = 128 ms\n");

    const char *em[] = {"HAPPY","SAD","ANGRY","SURPRISED","NEUTRAL",
                        "ANNOYED","SCARED","TIRED"};

    // A) conversacion normal
    {
        std::vector<std::pair<int, std::string>> v;
        for (int i = 0; i < 8; i++) v.push_back({i * 2000, em[i]});
        v.push_back({16000, "TALK"});
        escenario("A) conversacion normal (1 emocion cada 2 s)", v, 18000, "TALK");
    }

    // B) la app reconecta y manda su estado
    {
        std::vector<std::pair<int, std::string>> v;
        for (int i = 0; i < 8; i++) v.push_back({1000 + i * 2, em[i]});
        v.push_back({1002, "TALK"});
        escenario("B) reconexion: 9 comandos en 20 ms", v, 6000, "TALK");
    }

    // C) el caso que importa: la cola se llena de sensores y LUEGO llegan
    //    las emociones. Lo que llega ultimo es lo que el usuario quiere ver.
    {
        std::vector<std::pair<int, std::string>> v;
        for (int i = 0; i < 8; i++) v.push_back({1000 + i, "SENSOR:TEMP"});
        v.push_back({1010, "SAD"});
        v.push_back({1020, "ANGRY"});
        escenario("C) 8 sensores y DESPUES la emocion nueva", v, 4000, "ANGRY");
    }

    return 0;
}
