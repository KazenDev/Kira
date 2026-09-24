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
│       ├── Microfono/  Bateria/
│       └── Funciones.h         ← API única de temperatura, luz, botones, movimiento, sonido y batería
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
comandos): es basura en el buffer RX del UART. Solución: **re-flashear el hex**
(resetea el UART por completo).

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
```

Protocolo: `SENSOR:TEMP` → `TEMP:24`, `SENSOR:LUZ` → `LUZ:120`,
`SENSOR:BOTON` → `BOTON:1:0`, `SENSOR:ACCEL` → `ACCEL:x:y:z:pitch:roll`,
`SENSOR:MIC` → `MIC:nivel:b0:b1:b2:b3:b4:ventanas` y
`SENSOR:BAT` → `BAT:bateria_mv:vin_mv:fuente`.

El sonido es una medida **relativa**, no dB absolutos: el FFT se enciende sólo
para tomar la muestra y se apaga al terminar. Si hay una escucha manual activa,
responde `MIC:BUSY` para no interrumpir el audio. La lectura de batería es
aproximada y depende de lo que exponga el chip de interfaz; `0` significa que
no hay dato disponible, no que la batería esté vacía. La lectura de luz usa la
matriz LED como sensor durante un instante y puede producir un parpadeo mínimo.

Idea: que Kira/Kiro puedan decir "qué temperatura hace", "¿me sacudiste?",
"estoy acostado", "¿hay mucho ruido?", "¿cuánta batería le queda?", etc.,
usando estos datos reales.

---

## 🐛 TRUCO DE LA LINTERNA (sensor de luz)

El sensor de luz lee a través de la matriz LED. Con la cara encendida + luz
ambiente baja, lee poco (0-50). Para probarlo de verdad: **apuntar la linterna
directo a la matriz** o usarlo con luz natural. Si lo tapás, lee 0 (correcto:
sin luz = 0).
