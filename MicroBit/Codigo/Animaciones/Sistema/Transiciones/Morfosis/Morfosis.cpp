/**
 * Morfosis.cpp - Transicion MORFOSIS 🧬 (60 FPS, 126 frames = ~2.0s)
 *
 * IDEA DEL USUARIO: los pixeles de la cara ACTUAL no se apagan y ya.
 * Cada pixel VIAJA hasta su destino en la cara NUEVA (el primer
 * frame): se mueve por pasos de posicion y brilla de camino, como un
 * cometa diminuto. Si la cara nueva tiene MAS pixeles, los nuevos
 * "nacen" (aparecen con fade); si tiene MENOS, los sobrantes se
 * apagan con fade. Resultado: la vieja cara SE DERRITE y se
 * reconvierte en la nueva.
 *
 * ALGORITMO:
 *   1. Leo la pantalla actual (getPixelValue) -> lista de pixeles A
 *   2. Dibujo la cara destino (primer frame) y la leo -> lista B
 *   3. Restauro la pantalla actual
 *   4. Emparejo cada pixel A con el pixel B MAS CERCANO (greedy)
 *   5. Animo 126 frames: cada pareja interpola posicion con retardo
 *      escalonado (cascada); los sin pareja se apagan; los B que
 *      sobran nacen con fade en la ultima mitad
 *   6. Dibujo la cara destino completa (final limpio)
 */
#include "Morfosis.h"

// Un pixel con posicion
struct Px { int x, y; };

#define MAX_PX 25
#define FRAMES 125     // ~2s a 16ms/frame (60 FPS)

// ---------------------------------------------------------------------------
// Lee la pantalla actual: devuelve los pixeles encendidos (>100 de brillo)
// ---------------------------------------------------------------------------
static int leerPantalla(Px out[MAX_PX])
{
    int n = 0;
    for (int y = 0; y < 5 && n < MAX_PX; y++)
        for (int x = 0; x < 5 && n < MAX_PX; x++)
            if (uBit.display.image.getPixelValue(x, y) > 100) {
                out[n].x = x;
                out[n].y = y;
                n++;
            }
    return n;
}

// ---------------------------------------------------------------------------
// Distancia al cuadrado (evita sqrt, solo comparar)
// ---------------------------------------------------------------------------
static int dist2(const Px& a, const Px& b)
{
    int dx = a.x - b.x;
    int dy = a.y - b.y;
    return dx * dx + dy * dy;
}

// ---------------------------------------------------------------------------
// La MORFOSIS
// ---------------------------------------------------------------------------
void transicionMorfosis(EmocionActual destino)
{
    // 1) Leo la cara actual
    Px A[MAX_PX];
    int nA = leerPantalla(A);

    // 2) Dibujo la cara destino y la leo
    uBit.display.image.clear();
    dibujarCaraDestino(destino);
    Px B[MAX_PX];
    int nB = leerPantalla(B);

    // 3) Restauro la cara actual (la morfosis arranca de la vieja)
    uBit.display.image.clear();
    for (int i = 0; i < nA; i++)
        uBit.display.image.setPixelValue(A[i].x, A[i].y, 255);

    // 4) Emparejo cada pixel A con el B mas cercano (greedy, sin repetir)
    int parejaA[MAX_PX];   // parejaA[i] = indice de B, o -1 si no tiene
    bool usadoB[MAX_PX] = { false };
    for (int i = 0; i < nA; i++) {
        int mejor = -1;
        int mejorD = 1000;
        for (int j = 0; j < nB; j++) {
            if (usadoB[j]) continue;
            int d = dist2(A[i], B[j]);
            if (d < mejorD) { mejorD = d; mejor = j; }
        }
        if (mejor >= 0) {
            parejaA[i] = mejor;
            usadoB[mejor] = true;
        } else {
            parejaA[i] = -1;
        }
    }

    // Contar cuantas parejas hay (para escalonar la cascada)
    int nParejas = 0;
    for (int i = 0; i < nA; i++)
        if (parejaA[i] >= 0) nParejas++;

    const int dur = FRAMES / 2;      // 62 frames de viaje
    const int medio = dur / 2;       // 31

    // Los retardos se calculan UNA VEZ, aca afuera del bucle de frames.
    // El retardo de una pareja depende solo de su ORDEN y de cuantas parejas
    // hay: ninguno de los dos cambia entre frames, asi que recalcularlo
    // adentro era trabajo repetido 126 veces por pareja.
    //
    // OJO, lo que NO se gana aqui: la division. Se midio el .obj compilado
    // de esta transicion y tiene CERO llamadas a __aeabi_idiv: con -O2, GCC
    // prueba el rango del dividendo (t viene de f-retardo, acotado) y
    // convierte la division en multiplicacion+shift. Ademas toda la
    // firmware compila con cero __aeabi_idiv. Esto queda como codigo mas
    // limpio, no como una aceleracion.
    // `static` a proposito, NO por descuido. Con 25 ints son 100 bytes, y
    // medido en el .obj el frame de esta funcion pasaba de 580 a 676 bytes de
    // PILA. La pila util de la placa es ~2 KB y la transicion corre en el
    // hilo principal, asi que la pila es el recurso escaso; la RAM sobra
    // (10,8 KB usados de 128 KB).
    //
    // Es seguro porque el guard de Transiciones.cpp garantiza que esta
    // funcion NUNCA se re-entra: si llega un comando durante una transicion,
    // el destino se guarda y se aplica al terminar, no se anida. Si alguna
    // vez se saca ese guard, esto hay que volver a ponerlo en la pila.
    static int retardo[MAX_PX];
    {
        int orden = 0;
        for (int i = 0; i < nA; i++) {
            if (parejaA[i] < 0) { retardo[i] = 0; continue; }
            retardo[i] = nParejas > 0 ? (dur * orden) / nParejas : 0;
            orden++;
        }
    }

    // 5) Animo los FRAMES a 60 FPS
    for (int f = 0; f <= FRAMES; f++) {
        // ── LA VENTANA MUERTA ──────────────────────────────────────────
        // Esta transicion duraba 2,0 s COMPLETAMENTE sorda: no miraba el
        // serial ni una vez, asi que si la IA mandaba un comando en medio, la
        // cara recien se enteraba cuando la transicion terminaba (peor caso
        // medido: 1984 ms). Con un chequeo por frame se entera en 16 ms y
        // esta transicion se aborta sola.
        //
        // Costo del chequeo: un readUntil() por frame, ~140 ciclos = 0,03 ms.
        // Son 0,27 ms en toda la transicion: 0,014% de los 2 s. Barato.
        //
        // NO se llama a hacerTransicion() desde aca (eso anidaria
        // transiciones y desborda la pila): ver el guard de Transiciones.cpp.
        if (revisarSerial()) return;

        uBit.display.image.clear();

        // 5a) Las parejas VIAJAN (cascada: cada una sale un poquito
        //     mas tarde; asi se ve un flujo, no un cambio de golpe)
        int orden = 0;
        for (int i = 0; i < nA; i++) {
            if (parejaA[i] < 0) continue;
            int j = parejaA[i];

            // Retardo escalonado: precalculado arriba, no se recalcula.
            int t = f - retardo[i];        // frames desde que salio
            if (t < 0) {
                // todavia no salio: sigue en su lugar original
                uBit.display.image.setPixelValue(A[i].x, A[i].y, 255);
            } else if (t >= dur) {
                // ya llego: brilla en el destino (fade in final)
                uBit.display.image.setPixelValue(B[j].x, B[j].y, 255);
            } else {
                // en camino: interpolacion de posicion + brillo
                int px = A[i].x + ((B[j].x - A[i].x) * t) / dur;
                int py = A[i].y + ((B[j].y - A[i].y) * t) / dur;
                // brillo: sube al partir y baja al llegar (cometa).
                // `medio` esta declarado arriba del bucle de frames: el
                // original lo recalculaba aqui adentro, en cada pixel de cada
                // frame, y ademas tapaba al de afuera (variable sombreada).
                int br = (t <= medio)
                    ? 60 + (195 * t) / medio        // despega
                    : 255 - (150 * (t - medio)) / (dur - medio);
                uBit.display.image.setPixelValue(px, py, br);
            }
            orden++;
        }

        // 5b) Los A sin pareja (sobran): se apagan con fade en la
        //     primera mitad
        for (int i = 0; i < nA; i++) {
            if (parejaA[i] >= 0) continue;
            int br = 255 - (255 * f) / dur;
            if (br > 0)
                uBit.display.image.setPixelValue(A[i].x, A[i].y, br);
        }

        // 5c) Los B sin pareja (faltan): NACEN con fade en la
        //     segunda mitad (aparecen en su posicion final)
        for (int j = 0; j < nB; j++) {
            if (usadoB[j]) continue;
            int t = f - dur;
            if (t < 0) continue;
            int br = (255 * t) / dur;
            uBit.display.image.setPixelValue(B[j].x, B[j].y, br);
        }

        uBit.sleep(16);   // 60 FPS
    }

    // 6) Final limpio: la cara destino completa
    dibujarCaraDestino(destino);
}
