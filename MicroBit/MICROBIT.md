# 🎭 MICRO:BIT — EXPRESIONES EMOCIONALES CON IA

> Proyecto para la **Feria de Ciencias** — el micro:bit es la **cara física**
> de Kira y Kiro (los personajes IA). Muestra emociones, habla con la boca,
> carga mientras la IA piensa y hasta lee sensores reales.

---

## 🧠 ¿DE QUÉ TRATA?

Una IA (DeepSeek V4 Flash) conversa con el público. Cada respuesta trae una
**emoción** (`happy`, `sad`, `angry`, `surprised`, `neutral`, `fastidio`,
`miedo`, `cansado`) y el micro:bit **pone la cara**: dibuja la emoción en su
matriz LED 5×5, la anima con movimiento, sincroniza la **boca** con la voz
real del TTS (Fish Audio) y muestra un **loading** mientras la IA "piensa".

Es una **pieza de demostración viva**: la gente le habla al chat, y la
plaquita reacciona como si tuviera sentimientos.

---

## ⚙️ ¿CÓMO LO ESTAMOS HACIENDO?

| Decisión | Por qué |
|---|---|
| **CODAL C++** (no MicroPython ni MakeCode) | 60 FPS reales por hardware (PPI + GPIOTE), animaciones fluidas; las caras viven en FLASH (RAM casi intacta: 128KB). El programa usa ~28% del flash (512KB) |
| **Compilación local** con `build.py` del samples oficial | `python3 build.py` → genera `MICROBIT.hex` → se copia al drive DAPLink (se flashea solo) |
| **Comunicación por serial USB** (115200 baudios) | La PC manda comandos (`HAPPY\n`, `LOADING\n`...), el micro:bit responde `ACK:<COMANDO>` — verificación REAL de que está vivo |
| **Código modular** (una carpeta por cosa) | Cada emoción, cada loading y cada transición vive en su propia carpeta con su `.cpp`/`.h`. Se puede agregar una emoción nueva sin tocar el resto |

---

## 📁 ESTRUCTURA DEL CÓDIGO (`Codigo/`)

```
MicroBit/
├── info_microbit.txt          ← info técnica del dispositivo (serial, chip, firmware)
├── MICROBIT.md                ← ESTE ARCHIVO
├── Codigo/                    ← NUESTRO CÓDIGO FUENTE (el src real)
│   ├── Principal.cpp          ← PUNTO DE ENTRADA: init + bucle principal (solo orquesta)
│   ├── Animaciones/
│   │   ├── Emociones/         ← LAS CARAS (8 emociones)
│   │   │   ├── Alegria/  Triste/  Enojado/  Sorprendido/
│   │   │   ├── Neutral/  Fastidio/  Miedo/  Cansado/
│   │   │   └── Emociones.h/cpp  ← registro de emociones
│   │   └── Sistema/
│   │       ├── Sistema.cpp/h   ← receptor serial + despachador de comandos
│   │       ├── ReplicaLed.cpp/h← fibra que retransmite el frame LED real (~12-20fps)
│   │       ├── MicFft/         ← analizador de espectro REAL del micrófono (FFT, 5 bandas)
│   │       ├── Loading/        ← 10 animaciones de carga (una carpeta por patrón)
│   │       └── Transiciones/   ← 3 efectos de transición entre emociones (60 FPS)
│   └── Funciones/             ← LECTURA DE SENSORES para tool calling de la IA
│       ├── Temperatura/  Luz/  Botones/  Acelerometro/
│       ├── Microfono/  Bateria/  Gestos/  Brujula/  Tactil/
│       └── Funciones.h         ← API única de temperatura, luz, botones, movimiento, sonido, batería, gestos, brújula y toque
└── Referencias/               ← repos CODAL completos (consulta local, sin GitHub)
    ├── codal-microbit-v2-samples/  ← EL PROYECTO: se sincroniza source/ y se compila
    ├── codal-core/                 ← las APIs: Image, Serial, AnimatedDisplay...
    ├── codal-nrf52/                ← driver del chip nRF52
    └── codal-microbit-nrf5sdk/     ← SDK de Nordic
```

### El flujo (Principal.cpp)
1. `uBit.init()` → brillo tenue (90) → arranca la **réplica LED** (fibra paralela
   que manda el frame real a la PC) → **apaga el micrófono** al boot (CODAL lo
   prende solo; solo la animación Barra o una consulta puntual lo reactivan).
2. Bucle infinito: `revisarSerial()` (escucha comandos **sin bloquear** la
   animación), atiende los flancos de A/B para la escucha manual y renderiza
   la emoción activa (o boca hablando, o loading).

---

## 📡 PROTOCOLO SERIAL (comandos que entiende)

Todo comando termina en `\n`. El micro:bit responde **siempre** `ACK:<comando>`
al instante, y después lo que corresponda.

| Comando | Qué hace |
|---|---|
| `HAPPY` / `SAD` / `ANGRY` / `SURPRISED` / `NEUTRAL` / `FASTIDIO` / `MIEDO` / `CANSADO` | Cambia a esa emoción (con **transición aleatoria** de 60 FPS primero) |
| `TALK` | La boca habla con LA BOCA de la emoción activa (ojos siguen en fibra paralela) |
| `LOADING` | Loading en **bucle infinito** con patrón al azar (nunca carga igual 2 veces) |
| `LOAD0` … `LOAD9` | Bucle continuo con UN patrón específico (para probar/preview) |
| `LOADALL` | Preview de todos los loadings en secuencia |
| `STOP` | Vuelve a la alegría (con transición) |
| `CANCELAR` | Descarta la escucha manual sin transcribir (`AUDIO:CANCEL`) |
| `CALIB` | Recalibra el sensor de luz del patrón 9 |
| `TEST` | Demo automática (recorre emociones + loadings) |
| `BLINK` | Parpadeo simple de ojos |
| `TRANS` / `TRANS0/1/2` / `TRANSALL` | Probar transiciones |
| `SENSOR:TEMP` / `SENSOR:LUZ` / `SENSOR:BOTON` / `SENSOR:ACCEL` / `SENSOR:MIC` / `SENSOR:BAT` | Lee un sensor REAL y responde `TEMP:24`, `LUZ:120`, `BOTON:1:0`, `ACCEL:x:y:z:pitch:roll`, `MIC:nivel:b0:b1:b2:b3:b4:ventanas` o `BAT:bateria_mv:vin_mv:fuente` |
| `SENSOR:GESTO` | Devuelve el último gesto estable de CODAL y la magnitud actual: `GESTO:codigo:magnitud_mg` |
| `SENSOR:BRUJULA` | Devuelve rumbo, intensidad magnética y estado de calibración: `BRUJULA:rumbo:campo:calibrada` |
| `SENSOR:TOQUE` | Devuelve estado y lectura capacitiva del logo: `TOUCH:presionado:lectura` |
| `CALIBRAR:BRUJULA` | Ejecuta la calibración UX oficial de CODAL; es manual y puede tardar unos 32 s |
| `ESCUCHAR` | Arma la escucha manual; A abre, A envía y B cancela |

---

## 🎙️ ESCUCHA MANUAL CON A/B

- **Primer A**: abre el micrófono, activa el stream de audio y muestra el aro
  que reacciona al nivel real de la voz.
- **Segundo A**: termina el stream y envía `AUDIO:END`; el backend convierte,
  transcribe y entrega el texto a Kira.
- **B**: cancela la captura y envía `AUDIO:CANCEL`; el audio se descarta.
- No se usa VAD ni se corta por silencio: la persona decide exactamente cuándo
  termina el turno. El comando remoto `ESCUCHAR` comparte este mismo modo.

---

## 😊 EMOCIONES (8)

Cada una tiene su carpeta con **animación propia** (movimiento, no caras fijas):

1. **Alegria** — ojos que parpadean con guiño impredecible, lagrima de felicidad, sonrisa viva
2. **Triste** — ceño caído, lagrima que cae, boca triste
3. **Enojado** — cejas inclinadas, cara seria que respira fuerte
4. **Sorprendido** — ojos bien abiertos que parpadean rápido, boca "OH"
5. **Neutral** — ojos cerrados tipo "bruh", 3 LEDs que parpadean al hablar
6. **Fastidio** — ojos hacia el costado, gesto de "qué paja"
7. **Miedo** — ojos en las esquinas, boca temblorosa
8. **Cansado** — ojos pesados, parpadeo lento, bostezo

Cada emoción tiene su propio **hablar** (la boca se mueve distinto según la emoción).

---

## ⏳ LOADINGS (10 patrones de carga)

Cada patrón vive en **su propia carpeta** con su código dedicado (rastro,
60 FPS, efectos propios):

| # | Nombre | Qué es |
|---|---|---|
| 0 | ☄️ Cometa | Luz orbitando con rastro mejorado, bucle continuo |
| 1 | 🌀 Espiral | Espiral que gira adentro/afuera, con transición de lado |
| 2 | 💓 Pulso | Borde que late, impredecible y fluido |
| 3 | 🌧️ Lluvia | Gotas cayendo con tormenta, 60 FPS, direcciones variadas |
| 4 | 📊 Barra | **Ecualizador REAL del micrófono** (FFT, 5 bandas de frecuencia) |
| 5 | 🏃 Carrera | Dos luces en carrera + persecución (deciden por cuál van) |
| 6 | 🔄 Flechas | Spinner de 4 flechas girando |
| 7 | 🏜️ Arena | **Sandbox interactivo**: la arena cae con la rotación del micro:bit (acelerómetro) |
| 8 | 🌊 Onda | Onda **senoidal real e interactiva**: va más rápido si movés el micro:bit |
| 9 | 🕯️ Luz | Círculo de carga que reacciona al **sensor de luz** (linterna) |

---

## 🎬 TRANSICIONES (3)

Entre emoción y emoción hay un efecto de 60 FPS (el bucle nunca se ve
"cortado"):

1. **Morfosis** — la cara nueva se forma encima de la vieja
2. **Cortina** — cortina que baja/abre revelando la cara nueva
3. **Fundido** — fundido cruzado suave

Se eligen **al azar** en cada cambio (la micro:bit nunca transiciona igual 2 veces).

---

## 🎤 MICRÓFONO (Barra / FFT)

- `MicFft/` conecta un FFT al pipeline de audio de CODAL (`uBit.audio.splitter`)
  y saca **5 bandas de frecuencia reales** → el ecualizador de la barra es de verdad.
- **Apagado inteligente**: `micFftDetener()` (parar stream ADC + cortar corriente
  física con `deactivateMic()`) se llama al boot, al salir de Barra y al cambiar
  de patrón. Solo Barra lo reactiva (`micFftIniciar()`). Verificado en vivo:
  boot = apagado, LOAD4 = encendido, STOP = apagado.

---

## 📺 RÉPLICA LED (el micro:bit virtual de la web)

`ReplicaLed.cpp` corre en una **fibra paralela**: retransmite por serial el
frame REAL de la matriz (`LED:<25 chars>`) a ~12-20fps. El backend lo lee y
`/api/status` lo expone → la web pinta **exactamente** lo que muestra el
micro:bit físico (no una cara simulada). Es **adaptativa**: en reposo no
transmite (deja de parpadear el LED amarillo de actividad).

---

## 😄 PATRON DE FRAME (emociones)

Las emociones se animan con el **patrón de frame**: `animarX()` muestra **un
frame (~16 ms)** y vuelve. El bucle de `Principal.cpp` la llama ~60 veces por
segundo y **el estado de la animación vive en `systemTime()`**, no en una cadena
de `sleep()`.

```cpp
// Alegria.cpp, esquema
if (revisarSerial()) return;                 // chequeo en CADA frame
unsigned short t = (uBit.systemTime() - faseBase) % CICLO_MS;
// ... derivar ojos y brillo de t, escribir solo lo que cambio ...
uBit.sleep(16);
```

El ciclo de cada emoción es una **tabla de segmentos** (`CICLO[]` con `desde`,
`hasta`, `curva`) en vez de una secuencia de funciones: agregar una fase es
agregar una fila. La tabla vive en FLASH, no en RAM (~42 bytes por emoción).

**Por qué importa** (medido, ver abajo):

- La versión anterior era una secuencia de ~170 `sleep()` con el progreso
  *dentro* de los `sleep`. La fase "respirar" eran **1,29 s sin un solo
  `revisarSerial()`**: si la IA mandaba un comando ahí, la cara no se enteraba
  hasta 1,3 s después.
- Con el estado en el reloj la animación es una **función pura del tiempo**, así
  que **interrumpir es gratis**: el frame siguiente ya calculó el estado nuevo.
- Las curvas son continuas (un seno evaluado por frame) en vez de 13 escalones
  de brillo sostenidos 22 ms cada uno.
- El parpadeo dejó de ser en serie: el original cerraba el ojo izquierdo y
  240 ms después el derecho (1,08 s de "parpadeo"). Ahora los dos salen del
  mismo número.

Mismo patrón que ya usaba el metrónomo (`metroFrame()`), por eso queda
consistente con el resto de la firmware y no es una excepción.

**Las 8 de 8 migradas.** Ranking medido (`sh Bench/run_emociones.sh`):

| emoción | ciclo | fps | checkpoints | escrituras | **ventana muerta** |
|---|---|---|---|---|---|
| Alegría | 6160 ms | 49,5 | 385 | 140 | **0 ms** (era 1290) |
| Triste | 6768 ms | 44,3 | 423 | 323 | **0 ms** (era 1415) |
| Cansado | 4336 ms | **77,5** | 271 | 291 | **0 ms** (era 995) |
| Miedo | 2224 ms | 54,9 | 139 | 96 | **0 ms** (era 670) |
| Enojado | 4576 ms | 33,2 | 286 | **44** | **0 ms** (era 948) |
| Sorprendido | 2640 ms | 27,3 | 165 | 81 | **0 ms** (era 725) |
| Neutral | 3888 ms | 46,0 | 243 | 127 | **0 ms** (era 810) |
| Fastidio | 1632 ms | 40,4 | 102 | 29 | **0 ms** (era 650) |

### Fastidio: la asimetría está en los OJOS

Sin azar, y de nuevo **sin cambiar ni un tiempo**, porque los gestos ya
estaban bien y hay respaldo:

- **La cara de reojo**: los ojos van en (0,1) y (3,1), **asimétricos a
  propósito**. El desprecio se define por asimetría (*"a half-smirk, one side
  raised... subtle and asymmetric"*) y *"echar los ojos hacia atrás es el gesto
  clásico del comportamiento desdeñoso"*. Un fastidio simétrico se vería
  robótico.
- **El parpadeo duro** (cierre instantáneo, 130 ms) es un *"full blink de
  énfasis"*: las guías lo describen como "reset, emphasis", y el *"tight
  blink"* como determinación — de ahí el "aguanta la calma".
- **La ceja que tiembla por el borde interno** es un build lento, que es
  exactamente lo que recomiendan frente al "se llena de golpe".
- **El "tsk"** es una microexpresión fugaz, como manda la guía: el desprecio
  *"se manifiesta como una microexpresión fugaz, de una fracción de
  segundo"*.

### Lo que las ocho dan en conjunto

- **Reactividad: de 650-1415 ms a 0 ms en las ocho.** Sin excepción.
- **Fluidez: sube solo cuando el paso original era más largo que un frame**
  (40-45 ms → ×2 a ×3; 30-35 ms → ×1,7; 18-30 ms → nada que ganar). No es una
  regresión cuando no sube: es que no había escalera que arreglar.
- **Escrituras al framebuffer: bajan** en seis de las ocho, porque la cara ya
  no se repinta en cada pasada.
- **`__aeabi_idiv` en toda la firmware: 0.** Ninguna animación paga divisiones
  enteras por software.

### Neutral: los gestos ya estaban bien, y hay respaldo

No se cambió **ni un tiempo**. Los dos gestos tienen sustento:

- **El peek** (se apagan los párpados exteriores y quedan las pupilas, con
  420 ms de mirada fija): *"el párpado superior hace la mayor parte del
  movimiento y el inferior la sigue"*, *"usar **parpadeos a medio cerrar**
  para personajes informales"*, y *"una mirada sostenida con pocos parpadeos
  transmite concentración o foco intenso"*. Es un medio parpadeo, no un
  parpadeo.
- **El "meh"** (solo se mueve el labio izquierdo, el derecho queda quieto): es
  **asimetría**, y hay evidencia de que funciona — *"incluir uno o una
  combinación de movimientos asimétricos de ceja, boca o párpado aumenta la
  credibilidad, atractivo y naturalidad percibidos"*, y *"las caras puramente
  simétricas pueden haber contribuido a que el personaje virtual parezca
  artificial"*. Un neutral simétrico es justo lo que hace que una cara se vea
  robótica. La misma literatura aclara que la asimetría rinde sobre todo en
  emociones complejas/ambivalentes: un "meh" es exactamente eso.

Verificado en la placa: aparecen los tres estados, y el del "meh" tiene **un
solo píxel encendido del lado izquierdo**.

Su fps subió **26,8 → 46,0** porque sus pasos eran de 30-35 ms, más del doble
de un frame: había escalera real que arreglar.

### Sorprendido: el único con CICLO VARIABLE

El pulso de asombro sale cada 2-3 ciclos, así que el ciclo **no tiene duración
fija**: 2630 ms con pulso, 1880 ms sin él. Con un único ciclo fijo de 2630, dos
de cada tres ciclos tendrían 750 ms de cara quieta de más — un 40% más lento, y
el ritmo se notaría.

Se resolvió carrying un **ancla absoluta del ciclo corriente** (`inicioCiclo`)
que se avanza en cada wrap, en vez de calcular la fase con un módulo desde un
ancla fija. Sin división, sin drift, y sin el pulso desfasado.

> **Lo que costó encontrar**: la primera versión alternaba
> `(ahora - faseBase) % 2630` y `(ahora - faseBase) % 1880`. Compilaba, el bench
> en el host daba bien, y en la placa daba **54 "ciclos" en 80 s con duraciones
> de 50 ms a 2,6 s**. Dos módulos de período distinto latiguean entre sí: los
> límites de ciclo caen donde caiga. **El bench de host no lo detectó porque
> siempre midió el ciclo con pulso**; lo delató la medición end-to-end en la
> placa. Corregido y verificado: 23 ciclos de 1880, 13 de 2630, cero raros, un
> pulso cada 2,6 ciclos.

**Lo que se dejó intacto**: los ojos quedan 280 ms mirando fijo. Las guías de
animación de sorpresa dicen que "puede haber un momento de quietud mientras el
personaje procesa lo que está pasando" y que "una reacción auténtica pasa muy
rápido, en apenas unos frames" — el hold *es* la sorpresa. Y el pulso queda por
escalones, porque un startle es involuntario y abrupto: una rampa suave se
leería como una transición normal.

### Enojado: la escalada de las cejas

La fase más larga eran las cejas: **1.110 ms sin un solo `revisarSerial()`**,
la mayor de las ocho. Ahora 0.

Dos cosas que se dejaron **a propósito**, porque ya estaban bien:

- **Las 3 oleadas de cejas se aceleran** (400 / 370 / 340 ms). Eso ya es una
  escalada, y la escalada es lo que hace creíble un enojo: *"no solo
  intensifica el estallido final, también lo hace más creíble porque el
  espectador siente la progresión emocional"*.
- **El parpadeo brusco se queda por escalones** (255, 127, 0). Un golpe tiene
  que resolver en pocos frames; estirarlo le quita potencia.

Una que **se propuso** y quedó conmutable en `ARC_CEJA`: el original encendía
las dos cejas interiores *de golpe*. Las guías de animación de cejas dicen que
el movimiento va **en arco**: la exterior adelanta y la interior la sigue con
unos 40 ms de retraso. Con `ARC_CEJA = 0` queda igual que el original; con
`0.036` la derecha sigue a la izquierda. Verificado en la placa: se ven los
dos estados intermedios (`##..#` y `#..##`).

### El patrón da siempre lo mismo: reactividad

Después de cinco migraciones, el patrón de las cifras es clarísimo:

| | fps antes → después | latencia |
|---|---|---|
| Alegría | 25,2 → 49,5 | 1290 → 0 |
| Cansado | 25,9 → 77,5 | 995 → 0 |
| Miedo | 56,1 → 54,9 | 670 → 0 |
| Enojado | 33,7 → 33,2 | 948 → 0 |

**La fluidez solo sube cuando los pasos originales eran más largos que un
frame** (40-50 ms en Alegría y Cansado). Cuando ya eran de 18-30 ms (Miedo,
Enojado) el fps se queda igual… y no es una regresión: es que no había
fluidez que ganar. **Lo que siempre mejora es la reactividad, sin excepción**,
y las escrituras al framebuffer bajan (Enojado 71 → 44) porque la cara ya no se
repinta en cada pasada.

### Miedo: el latido y el temblor

Segunda emoción con azar (la primera fue Triste). Acá el azar es un solo punto:
el temblor del cuerpo, `85 + uBit.random(70)`. Se resolvió igual que en Triste,
con `pseudo()` (hash sin estado, ~15 ciclos contra los ~100 del LFSR).

El "pum-pum" quedó **por escalones a propósito**: un latido es un golpe, no una
curva, y el segundo latido más corto y más tenue es el "dub" del "lub-dub"
— que es justo lo que ya hacía el original (160/50 ms y 140/40 ms).

> **Pendiente de tu ojo, a una línea**: el temblor se re-ranura cada
> `TEMBLO_MS = 30`, o sea ~33 Hz. Las guías de movimiento sitúan el
> "temblor/espiro" entre **5 y 15 Hz** (período 0,07–0,2 s): por arriba de eso
> un brillo que salta al azar empieza a leerse como **parpadeo** y no como
> cuerpo temblando. Si cuando lo mires te parece que centellea en vez de
> temblar, subí `TEMBLO_MS` a `100` (10 Hz, en plena banda). Está en
> `Miedo.cpp`, con la nota al lado. No se cambió por decisión propia: es diseño
> visual del personaje.

Ojo con Miedo en los números: **el fps bajó apenas** (56,1 → 54,9). No es una
regresión: Miedo ya era la más fluida de las ocho (pasos de 30–50 ms), así que
no había fluidez que ganar. Lo que se ganó fue **reactividad**: 670 ms → 0.

### Cansado: el caso mecánico (la referencia para las 3 que faltan)

Sin azar: todo sale de la tabla de segmentos y de la fase. Fue el que mejor
salió de las tres migraciones, **25,9 → 77,5 fps** y **995 ms → 0 ms** de
ventana muerta.

Dos cosas que se decidir acá con criterio, no por intuición:

- **No se le puso easing encima.** En un bucle ambiental la propia fase **ya**
  es la curva; aplicar easing encima se pelea con ella. Las guías de easing
  para pixel art lo dicen así: *"los bucles deben derivar de la fase, no de
  estado con easing"*.
- **Los tiempos del bostezo no se tocaron.** Un bostezo real se hace con abrir
  lento + hold largo + cerrar lento, y un emote de yawn de verdad usa
  `hold 400 / tween 600 easeOut / wait 500 / tween 500 easeIn`. Lo que había
  (180 ms abrir / 400 ms abierto / 180 ms cerrar) ya era correcto.

Nota: el `gap máximo sin cambio` de Cansado quedó en 400 ms, y **no es un
defecto**: es el hold del bostezo, que existe justamente para que se lea como
un bostezo y no como un parpadeo de la boca.

### Triste: el caso difícil (el del azar)

Es la única de las ocho con comportamiento aleatorio, y eso choca de frente con
el patrón de frame. El original usaba `uBit.random()` en dos sitios: el temblor
del labio (42 llamadas por pasada) y la lágrima.

**El problema con `codal::random()`**: es un LFSR de Schneier con
`static uint32_t random_value` **global compartido** (`CodalCompat.cpp:33`) que
también usa `hacerTransicion()` para elegir la transición. O sea que el valor
del temblor **dependía de cuántos números se habían sacado antes en toda la
firmware**. No es una función del tiempo, así que con el patrón de frame no
alcanzaba.

Cómo se resolvió:

- **El temblor** sale de un hash del tiempo (`pseudo()`), no del RNG. Mismo
  instante → mismo temblor, así que se puede saltar a cualquier punto de la
  animación y sigue saliendo bien. Y sale **más barato**: ~15 ciclos (dos
  multiplicaciones single-cycle del M0) contra los ~100 del LFSR con rechazo.
  La técnica se llama *ruido pseudoaleatorio sin estado, sembrado por el
  índice de frame*.
- **La lágrima** sí es una decisión de calendario, no visual, así que puede
  quedar como estado: un contador que baja **una vez por ciclo** (cuando la
  fase da la vuelta), no una vez por frame. Con 60 frames por ciclo, bajarlo
  por frame haría caer una lágrima **por segundo**. Medido en la placa: una
  cada ~5 ciclos; el original decía cada 4-7.

La cara de Triste se rastrea con los **25 píxeles**, no solo los 7 de la cara:
la lágrima pasa por la mejilla y pisa píxeles de la boca, así que un rastreo
parcial mentiría. Cuesta ~1,5 ms de CPU por ciclo de 6,8 s (0,02%): se paga por
robustez, porque es lo que hace que la cara se auto-repare si un loading se
detiene o un comando desconocido imprime `?` encima.

> **Trampa de CODAL**: `uBit.serial.printf()` **solo soporta `%d` y `%s`**
> (`Serial.cpp:425`). Con `%lu` el contador de debug `T<n>` salía vacío, sin
> ningún error de compilación.

### Verificar

```bash
# 1) en el host, contra el shim: la version vieja (sacada de git) vs. la que
#    se flashea
sh MicroBit/Bench/run.sh                  # Alegria
sh MicroBit/Bench/run_triste.sh           # Triste (incluye el reloj de lagrima)
sh MicroBit/Bench/run_transiciones.sh     # las 3 transiciones
sh MicroBit/Bench/run_emociones.sh        # ranking de las 8

# 2) en la placa real, con el firmware ya flasheado
python3 MicroBit/prueba_latencia.py
```

La segunda es la que importa: mide el tiempo entre que sale un comando por USB
y que la placa lo acusa, barriendo el ciclo de la animación.

### Las transiciones (arregladas)

Las tres (Morfosis, Cortina, Fundido) duran ~2,0 s y eran **completamente
sordas**: no llamaban `revisarSerial()` ni una vez. Con la IA hablando en una
feria, cada cambio de emoción dejaba la placa muda 2 segundos. Medido antes:
**1984-2080 ms** de peor latencia, y mandando cambios cada 0,8 s los comandos
se apilaban (de 16 ACKs solo llegaron 2).

| | duración | chequeos de serial | peor latencia |
|---|---|---|---|
| Morfosis | 2016 ms | 1 → 127 | **1984 ms → 0 ms** |
| Cortina | 2112 ms | 1 → 135 | **2080 ms → 0 ms** |
| Fundido | 2016 ms | 1 → 129 | **1984 ms → 0 ms** |

Costo: un `readUntil()` por frame, ~0,03 ms. Son 0,27 ms en toda la transición
(0,014%). La duración **no cambia**: es el mismo efecto visual.

**El guard de no-anidamiento es obligatorio, no una optimización.** El chequeo
por frame trae un riesgo: si un comando llega en medio de una transición,
`revisarSerial()` lo procesa *desde adentro del frame*, y si ese comando es
otro cambio de emoción llama a `hacerTransicion()`... otra vez. El código corre
en la pila del proceso principal, no en una fibra: medido en el `.obj`,
`transicionMorfosis` reserva **572 bytes** de pila y la pila útil es ~2 KB. Tres
niveles anidados desbordan la pila. `hacerTransicion()` anota el destino
pendiente y vuelve; el director lo pinta al terminar.

Verificado con una avalancha de 24 cambios de emoción seguidos: la placa
sigue viva, responde y no deja píxeles sueltos.

```bash
python3 MicroBit/prueba_transiciones.py    # avalancha + pixeles sueltos
sh MicroBit/Bench/run_transiciones.sh      # A/B en el host (vieja vs nueva)
```

**Lo que se descartó con medición** (no por intuición):

- **Divisiones enteras**: se sospechó que Morfosis pagaba ~4000 llamadas a
  `__aeabi_idiv` (el M0+ no tiene divisor por hardware). Al desensamblar el
  `.obj` real: **0 llamadas**. Y **0 en toda la firmware compilada**: con `-O2`
  GCC prueba el rango del dividendo y convierte la división en
  multiplicación+shift. El retardo precalculado queda como código más limpio,
  no como aceleración. Ver `Bench/div_codegen_arm.c` para el caso donde sí
  aparece la llamada.
- **El `clear()` de Morfosis**: 6600 bytes por transición, pero son 0,5 ms de
  `memclr` en 2 segundos. No vale la pena tocarlo.

**Lo que sí se corrigió, aparte del serial**: Cortina escribía la columna
central **dos veces** por píxel (220 escrituras de regalo) y Fundido hacía 126
llamadas a `setBrightness` de las cuales 13 **no cambiaban nada visible** (el
quantum del PWM es `0,8169 × brillo`, así que avanza de a saltos de ~1,22
unidades) — y cada llamada es una división por software. Al abortar un Fundido
a mitad hay que **restaurar el brillo a 90**, o la cara queda a media luz.

**Nota de calibración**: los comentarios decían "72 frames" y "~1,2 s". La
realidad es 126/132 frames y ~2,0 s. Corregido.

#### Las bocas de TALK (4 de 8 migradas)

`Codigo/Animaciones/Emociones/*/Hablar*.cpp` son las bocas de lip-sync. Se
escaparon del análisis de las emociones por una razón concreta: usan
**`fiber_sleep()` en vez de `uBit.sleep()`**, así que un grep de "sleep" no las
encuentra.

Y el problema era el más serio del proyecto, porque `Principal.cpp` le da
**prioridad a TALK sobre la animación de la emoción**:

```cpp
if (modoHablar) { animarBocaCansado(); }   // primero esto
else if (...)   { animarCansado(); ... }   // la emoción solo si NO habla
```

O sea que **mientras la IA habla, la boca es todo lo que corre en el hilo
principal**. Y eran secuencias de `fiber_sleep()` sin un solo checkpoint:

| boca | antes | estado |
|---|---|---|
| Cansado | 1350 ms | **0 ms** |
| Miedo | 1165 ms | **0 ms** |
| Fastidio | 974 ms | — |
| Neutral | 820 ms | — |
| Alegría | 820 ms | **0 ms** |
| Triste | 880 ms | **0 ms** |
| Enojado | 665 ms | — |
| Sorprendido | 690 ms | — |

Las cuatro migradas son exactamente las cuatro peores. Verificado en la placa
con TALK activo: **24/24 ACKs y 12-19 ms de peor latencia** (con TALK en el
bucle viejo, hasta 1350 ms).

Dos cosas que conviene saber:

- **La fibra de los ojos no se toca.** Usa `fiber_sleep()` (que cede la CPU),
  rompe el loop en cuanto `modoHablar` es false, se libera sola, y no procesa
  comandos. Además, en una fibra el estado **no** tiene que ser función del
  reloj: un loop de fibra ya es una máquina de estados con su propio ritmo, así
  que ahí `uBit.random()` está perfecto. Lo que necesitaba ser función del
  reloj era la boca, porque corre en el hilo principal.
- **La boca no lleva checkpoint propio, a propósito.** El bucle principal ya
  hace `revisarSerial()` antes de la cadena de render, así que con la boca
  devolviendo cada 16 ms el puerto se chequea a 60 Hz solo. Agregar otro sería
  trabajo redundante. Y `uBit.sleep()` **es** `fiber_sleep()`
  (`CodalDevice.cpp:30`), así que las emociones y las bocas ceden la CPU igual.

**Los cuatro guiones ya estaban bien, y hay respaldo:**

- **Cansado (2,2 sílabas/s)**: su tic de 460 ms es el más largo de las ocho
  bocas y está por debajo del rango normal de habla (4 sílabas/s, medido entre
  3,3 y 5,9). Eso es lo que hace que el "siii... ya voy..." se lea. Encima el
  brillo tope es 200 en vez de 255: más lento **y** más tenue, las dos cosas en
  el mismo sentido. *Pendiente de tu ojo, en `PAUSA_ENTRE_TICS_MS`*: la
  literatura del habla lenta dice que no es solo más lento, sino "articular
  lentamente con un **mayor número de pausas** e hiperarticulación". Esta boca
  tiene la primera parte pero no las otras dos: los tres tics van pegados y con
  amplitud reducida. Con la constante en 0 queda como siempre; a 250 aparecen
  las pausas.
- **Miedo (guion compuesto, 1180 ms)**: es la única con guion de 5 pasos en vez
  de oscilador libre, y tiene las tres alteraciones canónicas de la tartamudez
  —"el flujo se interrumpe por **bloqueos, repeticiones o prolongaciones**"— en
  orden: los dos tics cortos son las repeticiones, la pausa de 150 ms el
  bloqueo, y el tic de 120 ms la prolongación, que va **al final** porque es la
  palabra que por fin sale trabada. Y un detalle fino: durante el bloqueo la
  boca queda **abierta a 255**, no cerrada; es el intento de decir la palabra
  antes de que se trabe, así que el bloqueo es el pico de la frase.
- **Triste**: abre la mandíbula **hacia abajo** (el píxel central del frown
  deja su fila y aparece el de abajo). Es lo estándar: "la mandíbula dirige el
  movimiento... baja en las vocales abiertas", y el ritmo lento con hold evita
  la "boca de máquina de escribir" que las guías marcan como amateur.
- **Alegría**: los dientes a 255, el tic más corto (280 ms = 3,6 sílabas/s,
  ritmo normal).

```bash
sh MicroBit/Bench/run_bocas.sh      # A/B en el host (vieja vs. nueva)
python3 MicroBit/prueba_talk.py     # en la placa: arranca, se mueve, para, sale
```

## Lo que falta migrar

- **4 bocas de TALK** (Enojado, Sorprendido, Neutral, Fastidio) — todas
  mecánicas: mismo tic libre que Alegría/Triste/Cansado.
- **La réplica LED no puede mostrar el brillo**: compara
  `getPixelValue(x,y) > 0`, o sea solo on/off. Todo lo que se anima con el PWM
  global de `setBrightness` (la respiración, el latido, el temblor, el "tsk",
  la lágrima al atenuarse) **no se ve en el espejo de la web**, aunque en la
  placa real sí se vea. No es del firmware: es que el replicador manda 1 bit
  por píxel.

> El display refresca a **60 Hz** (`NRF52_LED_MATRIX_FREQUENCY`), así que los
> 16 ms de cada frame ya son el techo: más rápido no se ve, solo gasta.

---

## 🛠️ COMPILAR Y FLASHEAR

```bash
# 1. Sincronizar TODO Codigo/ (incluye Animaciones/ y Funciones/)
rsync -a --delete Codigo/ Referencias/codal-microbit-v2-samples/source/
rm -f Referencias/codal-microbit-v2-samples/source/Principal.cpp
cp Codigo/Principal.cpp Referencias/codal-microbit-v2-samples/source/main.cpp

# 2. Compilar desde cero si se agregaron archivos .cpp nuevos
#    (CMake usa un glob recursive y no siempre detecta el cambio solo)
cd Referencias/codal-microbit-v2-samples
rm -rf build
python3 build.py

# 3. Flashear: copiar el hex al drive del micro:bit (el bootloader lo consume solo)
cp MICROBIT.hex /media/zkazen/MICROBIT/
```

⚠️ **Si el micro:bit deja de responder ACK** (transmite LEDs pero no confirma
comandos): era basura en el buffer RX del UART. **Ya está arreglado en el
firmware** (ver abajo); re-flashear el hex era solo un parche que limpiaba el
UART sin tocar la causa.

### RX sano y ACKs que no se pierden (arreglado)

Dos bugs reales que se persistieron durante mucho tiempo con "re-flashear y ver
si se arregla":

**1. Basura en el RX.** `Serial::readUntil()` en modo `ASYNC` devuelve cadena
vacía y **no avanza `rxBuffTail`** cuando todavía no encuentra el `\n`
(`Serial.cpp:786`). Los bytes que llegan sin newline se quedan en el buffer
**para siempre** y se pegan al comando siguiente:

```
STOP -> ACK:^\xf7\xf7\x94\xffSTOP     (debería ser ACK:STOP)
```

El backend nunca matcheaba ese ACK → timeout. Y el backend también se
defendía por su lado (`serial_transport.py:278`, "descartar la basura del
boot"): los dos parcheando la misma causa. Ahora el firmware:

- **`rxLimpiar()`**: una línea con bytes de control no es un comando. Se queda
  solo lo imprimible (0x20–0x7E), lo que también se come el `\r` de Windows. Si
  la línea era basura, **no se ejecuta nada y no se manda un ACK mentiroso**:
  avisa `RX:BASURA:Limpio=<cmd>`.
- **`rxDescartarSiVencio()`**: un parcial que lleva 2 s esperando su `\n` ya no
  va a llegar. Se tira entero y avisa `RX:PARCIAL:Descartados <n>`. El mismo
  corte de 2 s que ya usaba la fibra BLE (`BleUart.cpp:175`), así que las dos
  rutas se comportan igual.

**2. ACKs que se perdían por colisión de TX (la causa real del "deja de
responder").** `Serial::send()` es **no bloqueante**: si otra fibra está
transmitiendo devuelve `DEVICE_SERIAL_IN_USE` y **descarta el mensaje en
silencio** (`Serial.cpp:376`). La fibra de `ReplicaLed` manda un frame `LED:`
cada 50 ms **sin mirar el retorno**, así que el ACK se perdía *justo cuando la
cara más se movía* — que es cuando la IA está hablando. Por eso el síntoma
parecía intermitente y se "arreglaba" re-flasheando.

- `responder()` ahora reintenta (`enviarSerial()`): 20 intentos de 2 ms.
- `ReplicaLed` hace lo contrario a propósito: si el UART está ocupado, **se
  saltea el frame** sin reintentar. La réplica es telemetría; el ACK es
  protocolo. Reintentar la réplica le robaría el UART al ACK.
- Si el UART se pierde del todo, el mensaje se manda por BLE (`TX:OCUPADO:`).

**Prueba en vivo** (con la placa conectada):

```bash
python3 MicroBit/prueba_rx.py
```

Cubre comando limpio, parcial sin `\n`, línea con bytes de control, y que el
estado envenenado se cure solo. Ojo al escribir tests contra esta placa: el
retransmisor LED satura el puerto, así que hay que **leer por deadline**, no
`read(N)` (se bloquea hasta juntar N bytes y el test miente).

**Quirck conocido del transporte, NO es del firmware:** un byte `0x00` en el
cable hace que el transporte entregue **2 bytes fantasma** y se trague el
comando siguiente. El firmware no los ve, así que no hay nada que sanear. Lo
que sí se verificó es que el estado envenenado ahora **se cura solo a los 2 s**
(antes era permanente).

---

## 🔌 HABLARLE DESDE LA PC (Python)

```python
import serial, time
ser = serial.Serial('/dev/ttyACM0', 115200, timeout=2)
ser.write(b'HAPPY\n')      # manda la emoción
resp = ser.read(200)       # lee el ACK
ser.close()
```

---

## 🔬 SENSORES PARA TOOL CALLING (funciones de la IA)

`Funciones/` expone lecturas reales que el backend puede pedir:

```cpp
int  leerTemperatura();                    // °C  (uBit.thermometer.getTemperature())
int  leerLuz();                            // 0..255 (uBit.display.readLightLevel())
int  leerBotones();                        // bit0=A, bit1=B (isPressed)
LecturaAcelerometro leerAcelerometro();    // x,y,z (mili-g) + pitch,roll (grados)
LecturaMicrofono leerMicrofono();          // nivel + 5 bandas relativas (0..100)
LecturaBateria leerBateria();              // mV de bateria/entrada + fuente
LecturaGesto leerGesto();                  // último gesto CODAL + magnitud (mili-g)
LecturaBrujula leerBrujula();              // rumbo + campo + calibración
LecturaTactil leerToque();                 // estado del logo + lectura capacitiva
```

Protocolo: `SENSOR:TEMP` → `TEMP:24`, `SENSOR:LUZ` → `LUZ:120`,
`SENSOR:BOTON` → `BOTON:1:0`, `SENSOR:ACCEL` → `ACCEL:x:y:z:pitch:roll`,
`SENSOR:MIC` → `MIC:nivel:b0:b1:b2:b3:b4:ventanas`,
`SENSOR:BAT` → `BAT:bateria_mv:vin_mv:fuente`,
`SENSOR:GESTO` → `GESTO:codigo:magnitud_mg`,
`SENSOR:BRUJULA` → `BRUJULA:rumbo:campo:calibrada` y
`SENSOR:TOQUE` → `TOUCH:presionado:lectura`.

El sonido es una medida **relativa**, no dB absolutos: el FFT se enciende sólo
para tomar la muestra y se apaga al terminar. Si hay una escucha manual activa,
responde `MIC:BUSY` para no interrumpir el audio. La lectura de batería es
aproximada y depende de lo que exponga el chip de interfaz; `0` significa que
no hay dato disponible, no que la batería esté vacía. La lectura de luz usa la
matriz LED como sensor durante un instante y puede producir un parpadeo mínimo.

`leer_gesto` devuelve el **último gesto estable** que CODAL reconoce, no una
estimación de velocidad: los códigos cubren inclinación, cara arriba/abajo,
caída libre, sacudida e impulsos de 2/3/6/8 G. La magnitud es
`sqrt(x²+y²+z²)` e incluye la gravedad, por lo que en reposo ronda 1000 mili-g.

La brújula usa el magnetómetro combinado del chip de movimiento. `BRUJULA:-1:...:0`
significa que todavía no hay calibración: CODAL no debe entrar automáticamente
en una calibración interactiva de hasta 32 segundos desde una consulta de la IA.
El campo se informa como lectura reportada por CODAL, sin llamarlo dB ni
convertirlo a una unidad no verificada. Para activar la calibración estándar,
mandá `CALIBRAR:BRUJULA` desde USB o BLE (o `python3 mb.py calibrarbrujula` y
dejá que la pantalla indique cuándo terminar); mové la placa siguiendo la UX en
todas las direcciones. Una consulta puntual no la reemplaza.

El logo táctil devuelve un estado debounced (`1`/`0`) y la lectura capacitiva
cruda. No es una medición de fuerza de contacto.

Idea: que Kira/Kiro puedan decir "qué temperatura hace", "¿me sacudiste?",
"¿hacia dónde apunta la brújula?", "¿estoy tocando el logo?", "¿hay mucho ruido?",
"¿cuánta batería le queda?", etc., usando estos datos reales.

---

## 🐛 TRUCO DE LA LINTERNA (sensor de luz)

El sensor de luz lee a través de la matriz LED. Con la cara encendida + luz
ambiente baja, lee poco (0-50). Para probarlo de verdad: **apuntar la linterna
directo a la matriz** o usarlo con luz natural. Si lo tapás, lee 0 (correcto:
sin luz = 0).
