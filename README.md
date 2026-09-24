# Kira

Kira es una IA conversacional con memoria local, micro:bit, TTS y una interfaz web instalable.

## Estructura

- `Cerebro/Backend/`: API FastAPI, memoria JSONL, RAG local, autenticación y hardware.
- `Cerebro/Frontend/`: SPA React/Vite/PWA.
- `MicroBit/`: firmware y código C++ de la placa.
- `Cerebro/ARQUITECTURA.md`, `TODO.md` y el resto de los `.md`: documentación del proyecto.

## Puesta en marcha

1. Copiá `Cerebro/Backend/.env.example` a `Cerebro/Backend/.env` y completá las claves de proveedores.
2. Instalá las dependencias del backend y del frontend.
3. Levantá el backend con `Cerebro/Backend/run.sh`.
4. Levantá el frontend con `npm install` y `npm run dev` dentro de `Cerebro/Frontend`.

## Memoria y cuentas

La primera cuenta creada recibe una copia del snapshot legacy de Kira; el original queda intacto. Cada cuenta tiene memoria, diario, conversaciones, grabaciones y RAG derivados en su propio directorio. Los datos runtime, credenciales, bases SQLite, `.env`, `node_modules`, `.venv` y referencias/binarios externos no se versionan.

## Micro:bit

La escucha del micrófono es manual: el comando `ESCUCHAR` deja la placa
armada, el primer A abre el micro, el segundo A envía el audio y B lo cancela.
No se usa VAD para cerrar el turno. El firmware se compila con el proyecto
CODAL local y se flashea como `MICROBIT.hex`.

## Verificación

Desde `Cerebro/Backend`:

```bash
./.venv/bin/pytest -q
./.venv/bin/python run_tests.py
```

Desde `Cerebro/Frontend`:

```bash
npm run build
```
