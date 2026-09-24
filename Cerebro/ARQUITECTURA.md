# Arquitectura actual de Kira

> Documento de trabajo backend. La identidad del producto es **Kira**.
> No se despliega automáticamente. La clave de DeepSeek se carga desde el `.env`
> local mediante `run.sh`; no se mantiene su valor en el código.

## 1. Vista general

Kira sigue siendo un backend FastAPI servido por Uvicorn. La ruta de importación
y el contrato histórico no cambian:

```bash
./run.sh
# Equivalente manual, después de cargar el .env:
# uvicorn kira_server:app --host 127.0.0.1 --port 8000
```

`kira_server:app` es deliberadamente un facade de compatibilidad. La
implementación se está organizando por capas, sin cambiar los endpoints
que consume el frontend.

## 2. Capas actuales

```text
Cerebro/Backend/
├── kira_server.py                 # facade, lifecycle y orquestación heredada
├── app/
│   ├── main.py                    # create_app() y lifespan
│   ├── dependencies.py            # BackendContext / inyección de collaborators
│   ├── schemas.py                 # schemas permisivos de request
│   ├── api/
│   │   ├── system.py              # personajes, memoria, status, título
│   │   ├── device.py              # BLE y control del micro:bit
│   │   ├── chat.py                # chat normal y SSE
│   │   └── audio.py               # TTS, transcripción, grabación y escucha
│   ├── domain/                    # visión, parsing, emociones, TTS puro
│   ├── intelligence/              # recuperación de memoria y diario
│   ├── services/                  # IA, voz, storage, tareas, HTTP
│   └── hardware/                  # serial, BLE relay, estado y captura
├── test_*.py                      # scripts ejecutables existentes
├── tests/                         # contratos pytest
├── run_tests.py                   # suite determinista
├── run_live_tests.py              # smoke tests externos, opt-in
└── requirements-dev.txt           # pytest para desarrollo
```

### Dominio puro

`app/domain/` no conoce FastAPI, serial, red ni sistema de archivos. Ahí viven
la normalización de imágenes, el parser tolerante de JSON/SSE, emociones,
segmentación de frases y helpers de TTS.

### Servicios

- `AIProvider`: conserva los proveedores, retries, backoff, fallback y la
  secuencia de eventos SSE.
- `SpeechService`: mantiene el TTS lazy. El evento `audio` sólo registra la
  URL; Fish se ejecuta cuando el navegador hace el GET correspondiente.
- `MemoryStore` y `JsonStore`: mantienen los formatos JSON/JSONL existentes,
  pero las escrituras usan temporal, `fsync` y `os.replace`.
- `BackgroundTasks`: conserva referencias a memos/reflexiones y las cancela al
  apagar el servidor.
- `HttpClients`: reutiliza clientes `httpx.AsyncClient` durante el lifespan.

### Hardware

- `SerialTransport`: worker, cola, ACK, sensores y audio. `SerialManager` queda
  como alias compatible.
- `BleRelayHub`/`RelayBroker`: propiedad, latido, cola, expiración y NACK del
  puente del navegador. Se mantienen los límites históricos del relay.
- `DeviceState`: estado de conexión, ACK y réplica LED.
- `consume_audio_capture`: parser puro para `AUDIO:START`/`AUDIO:END` y
  `AUDIO:CANCEL`, incluso cuando los markers llegan partidos entre lecturas.
- La escucha del micro:bit es manual: `ESCUCHAR` arma el modo, A abre y envía,
  B cancela; no se ejecuta VAD para decidir el fin del turno.
- Las operaciones de captura, escucha y sensor usan un lock exclusivo para que
  dos chats no pisen la misma respuesta del micro:bit.

## 3. Contrato HTTP que se conserva

Los 36 paths API siguen registrados con los mismos métodos y shapes principales:

- `GET /api/personajes`
- `GET /api/archivos/{nombre}` y `GET /grabaciones/{nombre}` (archivos del usuario autenticado)
- `POST /api/auth/register`, `POST /api/auth/login`, `GET /api/auth/me`, `POST /api/auth/logout`
- `GET /api/memoria/{personaje}` (hechos, experiencias y reflexiones separados)
- `POST /api/memoria/{personaje}/olvidar` (olvida una entrada y conserva el rastro)
- `POST /api/memoria/{personaje}/borrar-todo` (vacía recuerdos activos, no el diario)
- `GET /api/conversaciones/{personaje}`
- `POST /api/titulo`
- `GET /api/status`
- `GET /api/rag/status` (estadísticas del índice derivado y del modelo)
- `POST /api/chat` y `POST /api/chat/stream`
- `GET /api/tts-stream/{tts_id}`
- `/api/ble/conectar`, `/tx`, `/ping`, `/nack`, `/rx`, `/desconectar`, `/error`
- `/api/loading`, `/voz`, `/talk`, `/calla`, `/emocion`, `/sync`, `/stop`,
  `/comando`
- `POST /api/transcribir`, `/grabar`, `/escuchar`
- `POST /api/cancelar` (descarta `AUDIO:CANCEL` sin pasar por transcripción)

Los schemas aceptan campos extra y mantienen la normalización legacy. Las rutas
que ya devolvían 400/404 para errores controlados conservan esos códigos; no se
cerraron campos que el frontend pudiera enviar.

### Cuentas, cookies y aislamiento

- `auth.sqlite3` guarda usuarios, hashes scrypt y sesiones opuestas; nunca se
  guarda la contraseña ni el token de sesión en texto plano.
- La cookie de sesión es `HttpOnly`, `SameSite=Lax` y `Secure` cuando corresponde
  (HTTPS o `KIRA_COOKIE_SECURE=1`). Las mutaciones autenticadas requieren el
  header CSRF ligado a la sesión.
- Cada cuenta tiene un directorio opaco bajo `usuarios/<user_id>/` con su
  memoria JSONL, diario, estado de reflexión, conversaciones y RAG SQLite. El
  hardware, BLE, clientes IA/TTS y el modelo de embeddings siguen compartidos.
- Al registrar la primera cuenta se copia el snapshot legacy de Kira a su
  directorio; los archivos originales no se mueven ni se borran. Las cuentas
  siguientes empiezan aisladas.

### Ajustes de memoria y privacidad

El modal `Frontend/src/Settings.tsx` agrupa sonido/voz, memoria y privacidad,
chats, dispositivo y diagnóstico. Las preferencias de memoria viven en
`localStorage` (`kira_ajustes_v1`) y viajan en `memoria_config` con cada turno.
`recordar` es el interruptor maestro; `hechos` controla la extracción
semántica, `experiencias` el memo post-charla y `usar` la inyección de RAG,
identidad y diario. Apagar no borra datos: el borrado explícito usa
`POST /api/memoria/{personaje}/olvidar` o
`POST /api/memoria/{personaje}/borrar-todo`, que crean tombstones y conservan
el JSONL. El panel de memoria permite olvidar una entrada puntual y Ajustes
ofrece una exportación JSON del snapshot activo.

### Eventos SSE

El stream continúa emitiendo, como mínimo:

`esperando`, `delta`, `reintento`, `tool`, `audio`, `fin` y `error`.

La carga útil, el orden de las frases y la relación entre `audio.orden` y la
reproducción del navegador no se cambiaron.

## 4. RAG de memoria

La memoria sigue teniendo como fuente de verdad los JSONL. RAG agrega un índice
derivado en `memoria/rag/index.sqlite3` (o `KIRA_RAG_DB`) con:

- FTS5/BM25 para nombres, fechas y términos exactos.
- Vectores normalizados de `intfloat/multilingual-e5-small` (384 dims).
- RRF para fusionar ambos rankings sin calibrar escalas incompatibles.
- Un umbral de similitud configurable (`0.80` con corroboración FTS; `0.84`
  para vector sin FTS) evita que el vectorial domine con vecinos irrelevantes;
  la posición relativa y FTS5 completan la decisión.
- `content_hash`, modelo, versión, provenance, `supersedes` y estado active.
- Tombstones para olvidar sin borrar el historial.
- Tools `buscar_recuerdos`, `actualizar_recuerdo` y `olvidar_recuerdo` para que
  la IA pueda consultar, corregir y olvidar de forma explícita.
- `GET /api/rag/status` muestra modelo, dimensión, cantidad de vectores, umbral y
  la ruta exacta del índice activo.
- Si el resumen opcional del proveedor falla, el bloque conserva los recuerdos
  locales crudos; si no hay evidencia, queda vacío.
- Al abrir un chat nuevo se pueden recuperar recuerdos recientes de otra sesión;
  dentro de una charla con historial se filtran para no duplicarlos.

### Memoria explícita post-turno

La captura de datos que el usuario dice sobre sí (por ejemplo, su nombre) **no
se decide con regex**. Después de cada turno, el extractor semántico pide a la
IA un JSON de hechos durables con `text`, `tipo`, `confidence`, `evidence` e
`importance`. El backend aplica sólo guardrails de forma:

- exige una confianza de al menos `0.9`;
- exige que `evidence` sea una cita del mensaje del usuario, no de Kira;
- no escribe si la IA devuelve una lista vacía o no está segura;
- versiona cambios con `supersedes` y conserva el JSONL append-only.
- Las memorias se clasifican en `semantic`, `episodic` y `reflection`; el panel
  las muestra por separado para no presentar cada reacción interna como un dato
  del usuario.
- El memo post-turno puede devolver `NADA`, se descarta por debajo de `6/10` y
  se deduplica antes de guardarse. `guardar_recuerdo` exige que el usuario haya
  pedido guardar explícitamente.

La tarea corre en background después de generar la respuesta, en `/api/chat` y
`/api/chat/stream`. El turno actual ya está en el historial; la memoria explícita
se prioriza en un bloque separado y no se duplica en el bloque RAG general. Si la
IA no logra una extracción confiable, Kira no inventa un nombre.

El modelo se prepara con:

```bash
.venv/bin/python prepare_rag_model.py
```

El índice se reconstruye desde los JSONL con:

```bash
.venv/bin/python rebuild_rag_index.py
# sólo FTS, sin cargar embeddings:
.venv/bin/python rebuild_rag_index.py --no-embeddings
```

La calidad se mide con consultas golden:

```bash
.venv/bin/python evaluate_rag.py golden.json --k 5
# sólo FTS, para comparar contra el lexical:
.venv/bin/python evaluate_rag.py golden.json --no-embeddings
```

El evaluador reporta `recall@k` y `MRR@k`; no se considera serio un cambio
de modelo si esas métricas no tienen una línea base.

`KIRA_RAG_PROVIDER=none` desactiva embeddings y deja el modo FTS5. Con el
modelo preparado, el camino normal del chat no hace llamadas de red para
embeddings. Si FastEmbed o el modelo no están disponibles, el chat hace fallback
a la recuperación lexical anterior. Las embeddings se generan localmente; no se
envían a Exa, Brave ni a otro proveedor.

## 5. Datos locales

No se renombran ni sobrescriben los directorios de runtime:

- `memoria/` — recuerdos, estado de reflexión y diario.
- `conversaciones/` — historial JSONL por personaje.
- `grabaciones/` — MP3/WAV generados localmente.

Los tests de memoria usan fixtures temporales o limpian sus archivos de prueba.
No se debe hacer `sync`, `reset` ni deploy de estas carpetas junto con el
código.

## 6. Tests

### Suite determinista (no llama proveedores)

```bash
cd Cerebro/Backend
.venv/bin/python run_tests.py
```

También se pueden ejecutar las piezas individuales:

```bash
.venv/bin/python test_app_contract.py
.venv/bin/python test_services.py
.venv/bin/python test_hardware.py
.venv/bin/python test_stream_deterministic.py
.venv/bin/python test_vision_deterministic.py
.venv/bin/pytest -q
```

`pytest.ini` apunta únicamente a `tests/`, de modo que pytest no recoge por
accidente los scripts live del directorio raíz.

### Smoke tests live

Requieren una decisión explícita porque pueden llamar a proveedores externos,
consumir saldo y tocar fixtures:

```bash
.venv/bin/python run_live_tests.py --yes
```

Los fallos actuales conocidos de los proveedores son `HTTP 402` en el principal
y `HTTP 410` en el fallback. No se interpretan como regresiones de la
refactorización mientras la suite determinista siga pasando.

## 7. Seguridad y despliegue

La sección de seguridad está **aplazada por decisión explícita**. Todavía no se
han cambiado credenciales, CORS, autenticación, rate limits, SSRF, límites de
upload ni la validación de tokens BLE expirados. La eliminación de secretos y
la rotación de claves deben hacerse antes de cualquier despliegue nuevo.

No desplegar al VPS sin autorización explícita.

## 8. Troubleshooting

### El servidor no encuentra `app`

El proceso debe ejecutarse desde `Cerebro/Backend` o tener ese directorio en
`PYTHONPATH`. El entrypoint compatible sigue siendo `kira_server:app`.

### El puerto serial se abre al importar

Los tests deben arrancar con `KIRA_SIN_SERIAL=1`. En producción, el worker se
administra desde el lifespan y se cierra con `SerialTransport.close()`.

### Hay dos respuestas cruzadas entre microphone y sensor

No se deben llamar directamente a `SerialManager` desde dos hilos. La
orquestación debe pasar por `escuchar`, `grabar` o `leer_sensor`, que aplican el
lock exclusivo. Si se añade una nueva operación física, debe usar el mismo seam.

### La reflexión no termina

`MemoryStore.claim_reflection()` es el claim atómico. Un fallo del proveedor
deja `reflexionando=false` y limpia el pendiente; los registros insuficientes se
omiten y se cuentan en `reflexiones_omitidas`.

### El navegador recibe una URL de audio pero no suena

Es el comportamiento lazy esperado. Hay que hacer GET a la URL del evento
`audio`; el registro por sí solo no llama a Fish. La cache sigue siendo FIFO
con el límite histórico de 50 piezas.
