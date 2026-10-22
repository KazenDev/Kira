<div align="center">

# CÓMO HACERLO

**Guía práctica para levantar, probar y tocar Kira sin romper nada.**

<sub>Todo lo de acá está verificado contra el código. Si algo no coincide, el código tiene razón.</sub>

<br/>

**← [README](README.md)** · **[Requisitos](REQUISITOS.md)** · **[Arquitectura](Cerebro/ARQUITECTURA.md)**

</div>

---

## Índice

- [Antes de empezar](#antes-de-empezar)
- [Levantar el backend](#1-levantar-el-backend)
- [Levantar el frontend](#2-levantar-el-frontend)
- [Crear tu cuenta](#3-crear-tu-cuenta)
- [Conectar la micro:bit](#4-conectar-la-microbit)
- [Compilar el firmware](#5-compilar-el-firmware)
- [Probar que la IA responde](#6-probar-que-la-ia-responde)
- [Tocar la memoria y el RAG](#7-tocar-la-memoria-y-el-rag)
- [Correr los tests](#8-correr-los-tests)
- [Cambiar el tema visual](#9-cambiar-el-tema-visual)
- [Agregar una herramienta a la IA](#10-agregar-una-herramienta-a-la-ia)
- [Problemas comunes](#problemas-comunes)

---

## Antes de empezar

| Necesitás | Versión | Para qué |
|:--|:--|:--|
| Python | 3.10+ | el cerebro |
| Node.js | 18+ | la interfaz |
| Una cuenta en Hive | — | la IA |
| micro:bit v2 | opcional | la cara y los sentidos |

Sin micro:bit la app funciona igual: solo no hay cara física ni sensores.

---

## 1. Levantar el backend

```bash
cd Cerebro/Backend

# entorno aislado (una sola vez)
python3 -m venv .venv
./.venv/bin/pip install -r requirements.txt

# configuración (una sola vez)
cp .env.example .env
```

Abrí `.env` y completá **como mínimo** la clave de IA:

```env
HIVE_API_KEY=tu_clave_de_hive
```

Opcionales, según lo que quieras usar:

```env
ELEVENLABS_API_KEY=...    # voz de Kira
ASSEMBLYAI_API_KEY=...    # transcribir audio del celular
EXA_API_KEY=...           # búsqueda web
AI_GATEWAY_API_KEY=...    # respaldo de IA
```

Ahora arrancalo:

```bash
./run.sh
```

Queda escuchando en **http://127.0.0.1:8000**.

> **¿Qué hace `run.sh`?** Carga el `.env`, detecta si el puerto serial de la
> micro:bit necesita `sudo` (grupo `dialout` en Linux), y lanza Uvicorn.
> Si no tenés placa conectada, no pasa nada.

**Comprobar que vive:**

```bash
curl http://127.0.0.1:8000/health
```

---

## 2. Levantar el frontend

En otra terminal:

```bash
cd Cerebro/Frontend
npm install
npm run dev
```

Queda en **http://127.0.0.1:5173**.

El proxy de Vite manda `/api` al backend automáticamente, así que no hay que configurar CORS ni URLs.

> **No abras `dist/index.html` con doble clic.** El frontend necesita el servidor
> de Vite (o el backend sirviendo el build) porque las rutas son relativas.

---

## 3. Crear tu cuenta

La app pide login. La **primera cuenta** que registres hereda la memoria de ejemplo
que viene con el proyecto; las siguientes arrancan vacías y aisladas.

1. Abrí http://127.0.0.1:5173
2. Elegí **Crear cuenta**
3. Usuario y contraseña (mínimo 8 caracteres)

Cada cuenta guarda su propia memoria, conversaciones, grabaciones e índice RAG.

---

## 4. Conectar la micro:bit

Hay **dos caminos**. Elegí uno.

### Por USB (recomendado para desarrollar)

1. Conectá la placa por cable
2. En Linux, asegurate de tener permisos:

```bash
sudo usermod -aG dialout $USER
# cerrá sesión y volvé a entrar
```

3. Verificá que aparezca:

```bash
ls /dev/ttyACM*
```

Si `run.sh` detecta la placa pero no puede abrirla, te avisa y usa `sudo`.

### Por Bluetooth (modo feria, sin PC)

No necesitás cable. El **navegador hace de puente**: toma los comandos del servidor
y los escribe por Web Bluetooth.

1. Abrí la app en **Chrome** (en `localhost` o HTTPS; Web Bluetooth lo exige)
2. Menú del chat → **Conectar micro:bit por Bluetooth**
3. Elegí la placa en el selector

El enlace se reconecta solo si se cae, y mantiene la pantalla del celular encendida
para que Chrome no congele el JavaScript.

---

## 5. Compilar el firmware

Solo si tocaste el código C++ de la placa.

```bash
cd MicroBit

# 1) copiar el código propio al proyecto CODAL
rsync -a --delete Codigo/ Referencias/codal-microbit-v2-samples/source/
rm -f Referencias/codal-microbit-v2-samples/source/Principal.cpp

# 2) compilar dentro del proyecto
cd Referencias/codal-microbit-v2-samples
python3 build.py
```

Genera `MICROBIT.hex`. Para flashearlo, conectá la placa: aparece como una unidad USB.

```bash
cp MICROBIT.hex /media/$USER/MICROBIT/
```

La placa se reinicia sola y ya corre el firmware nuevo.

> **Detalle:** `MicroBit/Referencias/` es grande y externo, así que **no se versiona**
> (está en `.gitignore`). Si clonaste el repo, tenés que bajar CODAL aparte.
> Los detalles del protocolo y las emociones están en [`MICROBIT.md`](MicroBit/MICROBIT.md).

---

## 6. Probar que la IA responde

Por terminal, sin interfaz:

```bash
cd Cerebro/Backend

# 1) crear sesión (guarda la cookie)
curl -c /tmp/kira.txt -X POST http://127.0.0.1:8000/api/auth/register \
  -H 'Content-Type: application/json' \
  -d '{"username":"prueba","password":"prueba12345"}'

# 2) sacar el token CSRF de la cookie
CSRF=$(grep kira_csrf /tmp/kira.txt | awk '{print $7}')

# 3) hablarle (streaming)
curl -N -b /tmp/kira.txt -H "X-Kira-CSRF: $CSRF" \
  -H 'Content-Type: application/json' \
  -X POST http://127.0.0.1:8000/api/chat/stream \
  -d '{"personaje":"kira","mensaje":"Hola, ¿quién sos?","historia":[],"sesion":"prueba"}'
```

Vas a ver eventos SSE:

```text
data: {"tipo":"esperando","intento":1}
data: {"tipo":"delta","texto":"¡Hola!"}
data: {"tipo":"tool","nombre":"leer_temperatura"}
data: {"tipo":"audio","url":"/api/tts-stream/...","orden":0}
data: {"tipo":"fin","emotion":"happy","message":"..."}
```

Si ves `delta` y `fin`, la IA funciona. Si ves `error`, el mensaje dice qué pasó.

---

## 7. Tocar la memoria y el RAG

**La fuente de verdad son los JSONL.** El índice RAG es derivado: se puede borrar
y reconstruir sin perder nada.

```bash
cd Cerebro/Backend

# ver el índice actual
curl -b /tmp/kira.txt http://127.0.0.1:8000/api/rag/status

# reconstruirlo desde los JSONL
./.venv/bin/python rebuild_rag_index.py

# sin regenerar embeddings (más rápido, usa los que ya hay)
./.venv/bin/python rebuild_rag_index.py --no-embeddings
```

Estructura de la memoria de cada cuenta:

```text
Backend/usuarios/<hash>/
├─ memoria/           recuerdos en JSONL (la verdad)
│  └─ rag/            índice derivado (se puede borrar)
├─ conversaciones/    chats
└─ grabaciones/       audios
```

**Privacidad:** en Ajustes podés apagar `recordar`, `hechos`, `experiencias` o `usar`.
Apagar un interruptor **no borra** lo guardado: solo deja de escribir o consultar.

---

## 8. Correr los tests

```bash
# backend: suite determinista (no llama a proveedores ni abre serial)
cd Cerebro/Backend
./.venv/bin/pytest -q
./.venv/bin/python run_tests.py

# backend: tests que SÍ usan red (pide confirmación)
./.venv/bin/python run_live_tests.py --yes

# frontend
cd Cerebro/Frontend
npm test
npm run build
```

> `run_tests.py` es la suite larga y determinista. `run_live_tests.py` gasta
> crédito de IA, por eso exige `--yes`.

---

## 9. Cambiar el tema visual

Todo el color vive en **un solo archivo**: `Cerebro/Frontend/src/Estilos/tokens.css`.

```css
:root {
  --bg: #0b1210;           /* fondo */
  --elev-1: #0f1714;       /* superficie 1 */
  --elev-2: #16211c;       /* superficie 2 */
  --text-1: #e8f1ea;       /* texto principal */
  --accent: #a3e635;       /* acento (lima) */
  --accent-hover: #b8ef5a;
}
```

Cambiá esos valores y todo el tema se actualiza. **No toques `00-legacy.css` ni
`compat-*.css`**: son capas de compatibilidad que preservan la cascada.

Para el tono de cada personaje hay un override por `data-pj` al final de `tokens.css`.

---

## 10. Agregar una herramienta a la IA

Hay **dos tipos** de herramienta. Elegí según lo que necesites.

### Tipo A: leer un sensor de la placa

Se declara en `HERRAMIENTAS` (en `kira_server.py`) con un comando serial y su prefijo
de respuesta. El sistema manda el comando, espera la línea y se la pasa a la IA.

```python
HERRAMIENTAS: dict[str, dict] = {
    # ...
    "leer_mi_sensor": {
        "descripcion": "Qué mide, en una frase. La IA lee esto para decidir cuándo usarla.",
        "comando": "SENSOR:MI_SENSOR",
        "prefijo": "MI_SENSOR:",
    },
}
```

Y del lado del firmware tenés que responder esa línea. Si la placa no contesta,
la herramienta devuelve el fallo y la IA lo dice.

### Tipo B: una función del servidor

Se resuelve en `ejecutar_herramienta`. Es el camino para cosas que no dependen de la placa.

```python
if nombre == "mi_herramienta":
    return {"ok": True, "resultado": "lo que quieras devolverle a la IA"}
```

Y la declarás igual, pero **sin** `comando` ni `prefijo`:

```python
"mi_herramienta": {
    "descripcion": "Qué hace, en una frase.",
},
```

### Reglas que no se rompen

- **Devolvé siempre un texto**, aunque sea un error legible
  (`"no pude leer el sensor: no hay placa conectada"`).
- **Si falla, decí por qué.** Kira prefiere decir "no sé" antes que inventar.
- **No inventes datos.** Si no hay medición real, devolvé el fallo y listo.
- **La descripción es el contrato.** Si dice "temperatura ambiente" y en realidad
  es la del chip, la IA va a afirmar algo falso. Escribila exacta.

### Probar que la IA la usa

```bash
curl -N -b /tmp/kira.txt -H "X-Kira-CSRF: $CSRF" \
  -H 'Content-Type: application/json' \
  -X POST http://127.0.0.1:8000/api/chat/stream \
  -d '{"personaje":"kira","mensaje":"Usá mi_herramienta para saber X","historia":[]}'
```

Tenés que ver un evento `{"tipo":"tool","nombre":"mi_herramienta"}` y después
los `delta` con la respuesta.

## Problemas comunes

<details>
<summary><b>El backend arranca pero la web dice "necesitás iniciar sesión"</b></summary>

Es correcto: la API está protegida. Registrate en la interfaz, o usá el `curl`
del [paso 6](#6-probar-que-la-ia-responde) que guarda la cookie.

</details>

<details>
<summary><b>"No pude leer el sensor: el micro:bit no respondió"</b></summary>

La IA está pidiendo un dato real y no hay placa. Conectala por USB o por Bluetooth.
El mensaje es honesto a propósito: Kira no inventa mediciones.

</details>

<details>
<summary><b>La voz no suena</b></summary>

1. ¿Está `ELEVENLABS_API_KEY` en el `.env`?
2. ¿El interruptor **Voz de Kira** está prendido en Ajustes?
3. Los navegadores bloquean el audio automático: **tocá la pantalla una vez**
   y después mandá un mensaje. La app desbloquea el audio en el primer toque.

</details>

<details>
<summary><b>El micrófono no abre</b></summary>

- Tiene que ser `localhost` o HTTPS (los navegadores lo exigen).
- Revisá que hayas dado permiso de micrófono al sitio.
- En la ruta del celular, la grabación corta sola a los 60 segundos.

</details>

<details>
<summary><b>Bluetooth no aparece</b></summary>

Web Bluetooth necesita **Chrome/Edge** y contexto seguro (`localhost` o HTTPS).
Firefox y Safari no lo soportan. En Android tiene que ser Chrome.

</details>

<details>
<summary><b>La IA responde lento</b></summary>

Mirá el `.env`: el proveedor primario es Hive con `reasoning_effort: none`.
Si tu clave no tiene saldo, Hive devuelve **405** y el sistema cae al respaldo
automáticamente. Los logs del backend dicen `[IA] hive se agotó ... CAE AL RESPALDO`.

</details>

<details>
<summary><b>Quiero empezar de cero</b></summary>

```bash
cd Cerebro/Backend
rm -rf usuarios/ auth.sqlite3
```

Después registrá una cuenta nueva. Eso **borra todas las cuentas y sus memorias**
de esta máquina.

</details>

---

<div align="center">

**← [README](README.md)** · **[Requisitos](REQUISITOS.md)** · **[Arquitectura](Cerebro/ARQUITECTURA.md)** · **[micro:bit](MicroBit/MICROBIT.md)**

<br/>

<sub>Si algo de esta guía no te funcionó, el problema es de la guía: abrí un issue.</sub>

</div>
