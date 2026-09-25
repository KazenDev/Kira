# Bench del micro:bit — medir antes de optimizar

Este bench corre el **código real de la firmware** en el host, contra un shim
que imita el micro:bit. No toca la placa y no adivina: cada coste por
operación está deducido del fuente de CODAL que ya está en
`MicroBit/Referencias/`.

## Correrlo

```sh
cd MicroBit/Bench
sh run.sh            # ciclo de 6148 ms (el de la alegría actual)
sh run.sh 4000       # con otro ciclo, para comparar duraciones
```

## Qué mide

| Métrica | Por qué importa |
|---|---|
| **ciclo completo** | ms de una pasada de la animación. Es lo que la persona espera antes de que se repita. |
| **CPU awake** | ciclos estimados del trabajo real. Si es diminuto, optimizar CPU es inútil: el problema es el tiempo, no el cálculo. |
| **cambios visibles / fps** | un cambio de framebuffer o de brillo global = un frame perceptible. |
| **gap máximo sin cambio** | el rato más largo que la cara se queda quieta. Un gap de 1,5 s se ve como "se colgó". |
| **puntos de interrupción** | cuántas veces por ciclo la animación puede notar un comando serial. Es la **reactividad**. |
| **PEOR latencia** | ms entre que la IA manda un comando y que la cara reacciona. El número que más se siente. |
| **heap allocs** | `ManagedString` es un tipo gestionado por refcount: construirlo desde un literal **es un `malloc()`**. En una placa donde el stack BLE se come la RAM, eso importa. |

## El modelo de coste y de dónde sale

Todo está en `mock/MicroBit.h`, con el archivo y la línea del fuente real:

| Operación | Coste | Fuente |
|---|---|---|
| `Image::setPixelValue` | 4 comparaciones + **1 store de byte**. No toca ningún peripheral: el refresco del LED lo hace la PPI+TIMER+GPIOTE por hardware leyendo el framebuffer. | `codal-core/source/types/Image.cpp:400` |
| `Image::clear` | `memclr()` del framebuffer. M0+ no tiene `memset`, así que es un bucle. | `Image.cpp:377` |
| display `setBrightness` | clamp + **una división entera**. El M0+ no divide por hardware → `__aeabi_idiv` por software. | `NRF52LedMatrix.cpp:341` |
| `uBit.sleep` | `fiber_sleep()`: deschedulea la fibra y entra al scheduler. Si no queda fibra runnable, el idle task hace `__WFE()`. **Dormir es lo más barato, no lo más caro.** | `CodalFiber.cpp:322, 775` |
| `readUntil` | recibe el delimitador **por valor**; `ManagedString(const char*)` hace `malloc()` real, el copy ctor solo `incr()`. | `Serial.cpp:721`, `ManagedString.cpp:78, 266` |
| framebuffer | **10×5 = 50 bytes**, no 5×5: el display se crea como `image(map.width*2, map.height)` y las 5 columnas extra son para la rotación. | `MicroBitDisplay.cpp` ctor |
| scheduler | **no preemptivo**, round-robin. | `CodalFiber.cpp:18` |

Los ciclos son **estimaciones conservadoras**, no mediciones con un profiler.
Sirven para comparar código contra código con el mismo modelo, que es lo que
importa. Para medir de verdad hay que flashear y leer el TICKS de la placa.

## Agregar otra emoción

1. El shim ya trae lo que usan las animaciones (`sleep`, `display` —incluido
   `getBrightness()`, porque `Display::brightness` es **protected**—, `serial`).
2. `bench_alegria.cpp` tiene los stubs que la firmware real espera del runtime:
   `revisarSerial()`, `procesarComando()`, `demoAutomatica()`, `bleColaSacar()`,
   `emocionActual`.
3. Agregá la versión nueva a `tabla()` y la vieja al lado.

## Qué compara hoy

`AlegriaVieja.cpp` es la versión anterior, sacada con `git show` del commit
previo al patrón de frame (con las funciones renombradas para poder linkearse
junto a la nueva). **Las dos son el código real**: no hay ninguna reescritura
para el bench. La nueva es la que se flashea.

Resultado de la migración de Alegría:

```
metrica                    VIEJA   NUEVA (frame)
ciclo completo           6148 ms       6160 ms
fps visual                  25.2         49.4
gap maximo sin cambio     390 ms      240 ms
puntos de interrupcion         6         385
PEOR latencia de comando  1290 ms         0 ms
```

La CPU awake sube de 0,011% a 0,043% del ciclo (0,67 ms → 2,62 ms) y los
wakeups de 170 a 385 por ciclo. Sigue siendo ruido: **el problema nunca fue
CPU, fue tiempo**.

En la placa real, `MicroBit/prueba_latencia.py` mide lo mismo end-to-end: con un
comando que no cambia la cara, **24/24 ACKs y 18,2 ms de peor latencia**, o sea
un frame.

## La idea central

El estado de la animación vive en el reloj, no en los `sleep()` encadenados. El
`sleep(16)` es solo "esperar al próximo frame", no "avanzar". Por eso
interrumpir es gratis y las curvas son continuas. Es el mismo patrón que ya
usaba `metroFrame()` del metrónomo.
