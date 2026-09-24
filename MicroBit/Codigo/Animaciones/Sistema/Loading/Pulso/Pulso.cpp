/**
 * Pulso.cpp - Patron de carga 2: PULSO DEL CORAZON REAL 💓 (v2)
 *
 * v2 (remodelado): adios al ramp sube-baja metronomico. Ahora es un
 * LATIDO REAL con:
 *   - LUB-DUB: golpe fuerte (sistole) + golpe corto y suave justo despues,
 *     como la contraccion real de un corazon
 *   - FLUIDO: ataque tipo curva RC (sube acercandose al pico) + caida
 *     exponencial (decay 78%) -> la luz respira, no salta de a 15
 *   - HRV natural: el descanso entre latidos varia al azar (+-70ms).
 *     El corazon humano NUNCA late con metronomo
 *   - IMPREDECIBLE: cada 6-10 latidos uno es ECTOPICO: llega antes de
 *     tiempo (descanso corto ~220ms) y le sigue una pausa compensatoria
 *     larga (~700ms), como un latido prematuro real (PVC)
 *   - LENTO: ~63 BPM (reposo real). Un ciclo de 9 latidos = ~10s
 *   - BUCLE CONTINUO: brillo static entre pasadas + sin clear al final
 *     (el brillo de reposo queda y el siguiente ciclo engancha)
 */
#include "Pulso.h"
#include "../LoadingBase.h"

static int brillo = 22;   // brillo actual del aro (continuo entre pasadas)

// Baja al brillo de reposo (22) y espera ms con chequeo serial.
// Devuelve true si llego un comando (hay que abortar).
static bool descanso(int ms)
{
    for (int f = 0; f < 4 && brillo > 22; f++) {
        brillo = brillo * 78 / 100;
        if (brillo < 22) brillo = 22;
        setAnillo(2, brillo);
        uBit.sleep(20);
        if (frameSerial()) return true;
    }
    for (int f = 0; f < ms / 20; f++) {
        // respiracion sutil durante la pausa (22->25->22): el corazon sigue vivo
        int respira = 22 + (f % 6 < 3 ? f % 6 : 6 - f % 6);
        setAnillo(2, respira);
        uBit.sleep(20);
        if (frameSerial()) return true;
    }
    return false;
}

// Un golpe del latido: ataque suave (RC: se acerca al pico) + caida
// exponencial fluida. Devuelve true si llego un comando.
static bool golpe(int pico, int ataque, int caida)
{
    for (int f = 0; f < ataque; f++) {
        brillo = brillo + (pico - brillo) / 2;   // subida natural (RC)
        setAnillo(2, brillo);
        uBit.sleep(20);
        if (frameSerial()) return true;
    }
    for (int f = 0; f < caida; f++) {
        brillo = brillo * 78 / 100;              // caida exponencial
        if (brillo < 22) brillo = 22;
        setAnillo(2, brillo);
        uBit.sleep(20);
        if (frameSerial()) return true;
    }
    return false;
}

// Lub-dub completo + descanso con variabilidad natural
static bool latido(int restMs)
{
    if (golpe(255, 6, 8)) return true;   // LUB (sistole, fuerte)
    if (golpe(170, 5, 7)) return true;   // DUB (suave, justo despues)
    return descanso(restMs);
}

void animarPulso(int ciclos)
{
    static int cuenta = 0;
    static int proximoEctopico = 6 + uBit.random(5);   // el 1ro a los 6-10 latidos

    for (int c = 0; c < ciclos; c++) {
        for (int k = 0; k < 9; k++) {                  // 9 latidos por ciclo (~10s)
            cuenta++;
            bool ectopico = (cuenta >= proximoEctopico);
            if (ectopico)
                proximoEctopico = cuenta + 6 + uBit.random(5);  // programar el proximo

            int rest = 430 + uBit.random(140);         // HRV natural (+-70ms)
            if (ectopico) rest = 220;                  // ectopico: llega ANTES de tiempo

            if (latido(rest)) return;

            if (ectopico) {                            // pausa compensatoria larga
                if (descanso(650 + uBit.random(150))) return;
            }
        }
    }
    // NO limpia al final: el brillo de reposo queda y el ciclo siguiente engancha
}
