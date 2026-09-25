/**
 * MicroBit.h - SHIM del micro:bit para el bench de HOST
 *
 * NO es CODAL. Es un modelo que imita lo UNICO que las animaciones de Kira
 * tocan del chip, con un coste por operacion deducido del fuente real de CODAL
 * (MicroBit/Referencias/codal-microbit-v2-samples/libraries/).
 *
 * Hechos del fuente real que fijan el modelo (ver README.md del bench):
 *
 *  1) Image::setPixelValue()  -> 4 comparaciones de rango + 1 store de byte.
 *     (Image.cpp:400) OJO: no toca ningun peripheral. El refresco del LED lo
 *     hace la PPI+TIMER+GPIOTE por hardware leyendo este framebuffer.
 *  2) NRF52LEDMatrix::setBrightness() -> clamp + UNA division entera
 *     (NRF52LedMatrix.cpp:341). El M0+ no tiene division por hardware, asi que
 *     eso es __aeabi_idiv por software.
 *  3) Image::clear() -> memclr() del framebuffer completo (Image.cpp:377).
 *     El framebuffer del display es de 10x5 = 50 bytes (MicroBitDisplay.cpp
 *     lo crea como image(map.width*2, map.height)), no 5x5: las 5 columnas
 *     extra son para la rotacion.
 *  4) uBit.sleep(ms) -> fiber_sleep(): deschedulea la fibra, la pone en la
 *     cola de sueño y entra al scheduler. Si no queda ninguna fibra
 *     runnable, el idle task hace __WFE() (CodalFiber.cpp:775-795): el CPU se
 *     duerme en bajo consumo. Dormir es lo MAS BARATO, no lo mas caro.
 *  5) Serial::readUntil(ManagedString delimeters, ...) recibe el delimitador
 *     POR VALOR (Serial.cpp:721) -> construir "ManagedString('\n')" es una
 *     ASIGNACION DE HEAP (es un tipo gestionado por refcount). Cada
 *     revisarSerial() paga ese malloc+free.
 *  6) El scheduler es NO PREEMPTIVO y round-robin (CodalFiber.cpp:18).
 *
 * El clock virtual solo avanza con sleep() y con las operaciones medidas: asi
 * el bench cuenta TIEMPO DE PAREDA (lo que ve la persona) y no solo CPU.
 */
#ifndef BENCH_MOCK_MICROBIT_H
#define BENCH_MOCK_MICROBIT_H

#include <stdint.h>
#include <string.h>
#include <string>

// ---------------------------------------------------------------------------
// Constantes del modelo de coste (Cortex-M0+ @ 64 MHz, nRF52833)
//
// Son ESTIMACIONES conservadoras, no ciclos medidos con un profiler. Lo que
// importa es el ORDEN DE GRANDEZA y que sean proporcionales entre operaciones,
// para poder comparar el codigo de antes contra el de despues.
// ---------------------------------------------------------------------------
#define BENCH_CPU_HZ            64000000.0

// Image::setPixelValue: dispatch virtual (~4) + 4 comparaaciones de rango
// (~8) + un store de byte (~2) + retorno.
#define BENCH_CYCLES_SETPIXEL       18

// NRF52LEDMatrix::setBrightness: dispatch virtual + clamp + division entera
// por software (__aeabi_idiv en M0+: ~45-60 ciclos) + store.
#define BENCH_CYCLES_SETBRIGHTNESS  60

// memclr() en M0+ no hay instruction: bucle de ~5 ciclos por byte.
#define BENCH_CYCLES_MEMCLR_BYTE     5

// fiber_sleep: sacar de la run queue, poner en la de sueño, schedule() con
// cambio de contexto; despues el CPU entra en WFE (0 ciclos, corriente minima).
#define BENCH_CYCLES_SLEEP         200

// Serial::readUntil vacio: rxInUse(), recorrer el buffer vacio y (en el
// firmware actual) un malloc+free del ManagedString delimitador.
#define BENCH_CYCLES_READUNTIL     140

// malloc/free del heap gestionado: primer free de la cadena, arena, etc.
#define BENCH_CYCLES_MALLOC        220

// ---------------------------------------------------------------------------
// Instrumentacion
// ---------------------------------------------------------------------------
struct BenchCounters
{
    unsigned long setPixel;        // llamadas a setPixelValue
    unsigned long setBrightness;   // llamadas a setBrightness
    unsigned long bytesCleared;    // bytes passados por clear()
    unsigned long sleeps;          // llamadas a sleep()
    unsigned long sleptMs;         // ms de pared acumulados en sleep()
    unsigned long readUntil;       // llamadas a readUntil() (revisarSerial)
    unsigned long heapAllocs;      // ManagedString construidos (= mallocs)
    unsigned long cycles;          // ciclos simulados
    unsigned long frames;          // cambios visibles del framebuffer
    unsigned long nowMs;           // reloj virtual
    unsigned long ultimoFrameMs;   // cuando fue el ultimo cambio visible
    unsigned long peorGapMs;       // el hueco mas largo entre cambios
    int  randomValor;              // lo que devuelve uBit.random(max)
    unsigned long lagrimas;        // veces que Triste pidió una lágrima
};

extern BenchCounters bench;

void bench_reset();
void bench_tick_cycles(double cycles);
// Registra un cambio visible del framebuffer (para medir fluidez real).
void bench_notificar_frame();

// --- Inyección de un comando serial en un instante exacto -------------------
// Es lo que permite medir REACTIVIDAD: el comando "llega" por serial a los
// X ms y vemos cuánto tarda el bucle de la emocion en enterarse.
extern unsigned long bench_inyectarEnMs;   // 0 = no inyectar nada
extern bool         bench_inyectado;
extern unsigned long bench_inyectadoEnMs;
extern unsigned long bench_latenciaMs;     // del ultimo comando detectado
extern unsigned long bench_peorLatenciaMs; // el peor caso del scenario
extern unsigned long bench_ultimaRevisionMs;
extern unsigned long bench_revisiones;

// Lo llama el sleep() del shim en el instante exacto de la inyeccion.
void bench_inyectar_si_toca();

// ---------------------------------------------------------------------------
// Tipos gestionados (mock). Solo lo que usan las animaciones.
//
// El reparto de costes sale del fuente real de CODAL:
//   ManagedString.cpp:78  initString() -> malloc(sizeof(StringData)+len+1)
//   ManagedString.cpp:266 copy ctor    -> ptr->incr()   (SIN malloc)
//   initEmpty()                         -> buffer vacio compartido (SIN malloc)
//
// O sea: ManagedString("literal") ES una asignacion de heap; copiar uno
// existente es solo un refcount. Por eso hoisteear el delimitador "\n" a un
// static de file scope ahorra un malloc+free por llamada a revisarSerial(),
// mientras que pasarlo por valor (que es lo que hace readUntil) no cuesta
// nada de heap.
// ---------------------------------------------------------------------------
class ManagedString
{
    public:
        std::string s;
        ManagedString() {}                                   // vacio compartido
        ManagedString(const char *c) : s(c)                  // malloc de verdad
        {
            bench.heapAllocs++;
            bench_tick_cycles(BENCH_CYCLES_MALLOC);
        }
        ManagedString(const ManagedString &o) : s(o.s) {}    // solo incr()
        int length() const { return (int)s.size(); }
        char charAt(int i) const { return s[i]; }
};

// ---------------------------------------------------------------------------
// Image: el framebuffer. 10x5 = 50 bytes, igual que el display real.
// ---------------------------------------------------------------------------
class Image
{
    public:
        uint8_t buf[50];
        Image() { memset(buf, 0, sizeof(buf)); }

        int setPixelValue(int x, int y, uint8_t v)
        {
            // Bounds check identico al del CODAL real.
            if (x >= getWidth() || y >= getHeight() || x < 0 || y < 0)
                return -1;
            if (buf[y * getWidth() + x] != v) bench_notificar_frame();
            buf[y * getWidth() + x] = v;
            bench.setPixel++;
            bench_tick_cycles(BENCH_CYCLES_SETPIXEL);
            return 0;
        }

        int getPixelValue(int x, int y)
        {
            if (x >= getWidth() || y >= getHeight() || x < 0 || y < 0)
                return -1;
            bench_tick_cycles(BENCH_CYCLES_SETPIXEL / 2);
            return buf[y * getWidth() + x];
        }

        void clear()
        {
            if (memcmp(buf, "\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 25) != 0)
                bench_notificar_frame();
            memset(buf, 0, sizeof(buf));
            bench.bytesCleared += sizeof(buf);
            bench_tick_cycles(BENCH_CYCLES_MEMCLR_BYTE * sizeof(buf));
        }

        int getWidth() const { return 10; }
        int getHeight() const { return 5; }
};

// ---------------------------------------------------------------------------
// Display
// ---------------------------------------------------------------------------
class Display
{
    public:
        Image image;
        int brightness = 90;
        void setBrightness(int b)
        {
            if (b < 0) b = 0;
            if (b > 255) b = 255;
            // El brillo global sale por el PWM (quantum), asi que un cambio de
            // setBrightness ES un cambio visible: cuenta como frame.
            if (brightness != b) bench_notificar_frame();
            brightness = b;
            bench.setBrightness++;
            bench_tick_cycles(BENCH_CYCLES_SETBRIGHTNESS);
        }
        // En CODAL, Display::brightness es protected (Display.h:41): se lee
        // con el getter, nunca directo.
        int getBrightness() const { return brightness; }
};

// ---------------------------------------------------------------------------
// Serial: solo readUntil, que es lo que usa revisarSerial().
// ---------------------------------------------------------------------------
enum SerialMode { ASYNC, SYNC_SPINWAIT };

class Serial
{
    public:
        // El bench inyecta comandos aqui para medir la REACTIVIDAD.
        std::string pendiente;
        bool hayComando = false;

        ManagedString readUntil(ManagedString delimeters, SerialMode mode)
        {
            (void)mode;
            bench.readUntil++;
            bench_tick_cycles(BENCH_CYCLES_READUNTIL);
            if (hayComando) { hayComando = false; return ManagedString(pendiente.c_str()); }
            return ManagedString();
        }

        int send(const char *s)
        {
            // Cuenta las lagrimas de Triste: es la unica forma de verificar
            // que el calendario de la emocion sigue siendo "cada 4-7 ciclos"
            // y no "una por frame".
            if (s && s[0] == 'L' && s[1] == 'A') bench.lagrimas++;
            return 0;
        }
        int send(uint8_t *d, int n) { (void)d; (void)n; return 0; }
        // CODAL lo habilita con CONFIG_ENABLED(CODAL_PROVIDE_PRINTF). El
        // director de transiciones lo usa para el "TRANS:n" de debug.
        int printf(const char *fmt, ...) { (void)fmt; return 0; }
};

// ---------------------------------------------------------------------------
// MicroBit
// ---------------------------------------------------------------------------
class MicroBit
{
    public:
        Display display;
        Serial serial;
        uint32_t sleepCalls = 0;

        void sleep(uint32_t ms)
        {
            bench.sleeps++;
            bench.sleptMs += ms;
            bench.nowMs += ms;
            bench_tick_cycles(BENCH_CYCLES_SLEEP);
            // El comando puede haber LLEGADO durante este dormir.
            bench_inyectar_si_toca();
        }

        uint32_t systemTime() { return bench.nowMs; }

        // CODAL devuelve un entero en [0, max]. El bench lo hace
        // determinista (siempre 0) para que la comparacion sea reproducible;
        // la version real usa el RNG del sistema.
        int random(int max) { (void)max; return bench.randomValor; }
};

extern MicroBit uBit;

#endif // BENCH_MOCK_MICROBIT_H
