# Celular/ — Kira controlada desde el teléfono 📱🔌

Objetivo: en la feria, **solo el celular + la micro:bit con batería** — sin laptop.
El chat de Kira corre en el cel, y los comandos de la IA llegan a la placa por Bluetooth.

## Estado

| Fase | Estado |
|---|---|
| 1. Firmware BLE (config + UART service + saludo al conectar) | ✅ HECHO — verificado en hardware |
| 2. Conexión desde el celular | ✅ LOGRADO — Chrome Web Bluetooth ve y conecta la placa |
| 3. Kira completa en la nube (modo feria) | ✅ **EN PRODUCCIÓN** |
| 4. Relay BLE: IA -> cel -> placa | ✅ **EN PRODUCCIÓN** |

## 🌐 MODO FERIA (en vivo)

- **URL:** `https://207.244.244.197.nip.io` (HTTPS con certificado real de
  Let's Encrypt, emitido automaticamente por Caddy que ya estaba en el VPS)
- **VPS:** Ubuntu 24.04, `/opt/kira/` (Backend + Frontend/dist + Personaje),
  servicio systemd `kira` (uvicorn :9000), Caddy proxea 443 -> 9000
- **Deploy:** `rsync` de Backend + Frontend/dist + Personaje, restart kira
- **Relay BLE:** el navegador es el "cable": botón "conectar placa" en la web
  -> Web Bluetooth -> poll `/api/ble/tx` (150ms) -> escribe por aire; las
  lineas de la placa vuelven por `/api/ble/rx` -> la IA lee sensores y
  recibe ACKs sin que exista un cable USB
- **Orden de uso en el stand:** 1) encender placa con batería, 2) abrir la
  web en el cel, 3) tocar "conectar placa" (esperar el hello), 4) chatear
  con Kira y pedirle emociones/metrónomo/sensores
- **VoIP de la voz:** el TTS (Fish Audio) suena en el parlante del cel
- **Pendientes chicos:** reintento automatico si nano-gpt tira 503; candadito
  (basic auth) porque la URL es publica y gasta API keys; probar el microfono
  del cel (HTTPS ya habilitado, getUserMedia deberia funcionar)

## La odisea del diagnóstico (lecciones aprendidas)

1. **El build CACHÉA el `codal.json`**: editar flags con `build/` existente NO recompila
   las librerías con los flags nuevos. Fix: `rm -rf build libraries` y rebuild limpio.
2. **Diagnóstico de lujo**: `pyocd` (con sudo) lee la RAM del chip por el mismo USB:
   `nm build/MICROBIT | grep m_nrf_sdh_enabled` → dirección → `pyocd cmd -c "read8 DIR"`.
   `00` = radio muerto, `01` = SoftDevice vivo. Sin adivinar.
3. **Android 6 esconde TODOS los resultados de scan BLE** sin permiso de ubicación de
   la app + ubicación del sistema ON. Se arregla por adb:
   `adb shell pm grant <paquete> android.permission.ACCESS_COARSE_LOCATION`
4. La app nRF Connect quedó descartada (el usuario no quiere apps externas) —
   **Web Bluetooth de Chrome** es el camino oficial del proyecto (además es el
   mismo stack que usará la web de Kira en la feria).
5. DMESG del boot: config `DMESG_SERIAL_DEBUG: 1` en codal.json + reset mientras
   se escucha el serial → sale el log de arranque del SoftDevice.

## Página de prueba usada

`https://thegecko.github.io/microbit-web-bluetooth/examples/index.html`
(ejemplo oficial de la librería `microbit-web-bluetooth`, hospedado en HTTPS)

## Lo que ya está en el firmware (Fase 1)

- `codal.json` del build: `MICROBIT_BLE_ENABLED:1`, `MICROBIT_BLE_OPEN:1`
  (sin emparejamiento, anuncia para siempre), DFU/Event services apagados (RAM).
- Módulo nuevo: `MicroBit/Codigo/Animaciones/Sistema/BleUart/` —
  Nordic UART Service (UUID `6e400001-b5a3-f393-e0a9-e50e24dcca9e`),
  los comandos entran al MISMO despachador que el USB.
  Al conectar suena "hello" y responde "BLE conectado" por el serial USB.
- ACKs y respuestas de sensores salen por USB **y** por BLE (`responder()`).
- Flush por inactividad (400 ms): los comandos no necesitan `\n` final.
- RAM del build: 98.33% usada — justo, pero compila y arranca (probado).

## Cómo probar con nRF Connect (Fase 2)

1. Instalar **nRF Connect for Mobile** (Play Store, gratis) en el Moto G3
2. Abrir BLE en el cel + SCANNER → buscar **"BBC micro:bit [XXXX]"**
3. CONECTAR → expandir **Nordic UART Service**
4. La característica **RX (6e400002)** = escribir aquí los comandos:
   `METRO:120:4`, `HAPPY`, `SENSOR:TEMP`, `SENSOR:LUZ`, `SENSOR:ACCEL`, `SENSOR:MIC`, `SENSOR:BAT`, `SENSOR:GESTO`, `SENSOR:BRUJULA`, `SENSOR:TOQUE`, `CALIBRAR:BRUJULA`, `STOP`...
   (el botón de flecha ↑ permite mandar texto; con el flush del firmware
   no hace falta el `\n` final)
5. La característica **TX (6e400003)** = notificaciones (ACKs y respuestas):
   activar las 3 rayitas ↓ para suscribirse y ver el `ACK:...`
6. Al conectar: la placa suena el saludo 🎵

## Próximos pasos

- Fase 3: proyecto Android (React Native o Capacitor envolviendo el Frontend de
  `Cerebro/Frontend`) — botones grandes: emociones, presets de metrónomo, caja
  de comando libre, lectura de sensores.
- Fase 4: el chat completo de Kira en el cel (el backend vive en una PC/túnel,
  el cel hace de puente BLE).
