# GUIA_DEPLOY_VPS.md — Manual para trabajar en este proyecto (para IA y humanos)

> ⚠️ **ARCHIVO CON CONTRASEÑA REAL ADENTRO — NO SUBIR A GITHUB NI COMPARTIR.**
> Si este repo se vuelve público algún día, BORRAR la contraseña de este
> archivo antes (o mejor: volver al placeholder `PW='LA_CONTRASEÑA'`).

> Guía de traspaso: cómo está montado Kira & Kiro, qué vive en la nube y
> cómo actualizar la web sin romper nada. Si sos una IA mejorando el
> frontend: leé esto completo antes de tocar nada.

## 🗺️ Qué es este proyecto (30 segundos)

**Kira & Kiro**: dos IA con personalidad emergente que viven en una
micro:bit v2 (carita LED), controlables desde el celular por Web
Bluetooth, con el cerebro en un VPS. Chat + voz (TTS/STT) + emociones +
sensores + metrónomo. Arquitectura completa en `PLAN_PERSONALIDAD_EMERGENTE.md`
y `Cerebro/LEEME.txt`.

## 📁 Estructura local (la fuente de verdad SIEMPRE es acá)

```
~/Escritorio/Kira/
├── Cerebro/
│   ├── Backend/            ← FastAPI (kira_server.py) — TODO el servidor
│   │   ├── test_*.py       ← tests (corren con .venv/bin/python test_X.py)
│   │   ├── config.py       ← API keys + MODELO_IA (cambiar modelo = 1 línea)
│   │   ├── memoria/        ← ⚠️ RECUERDOS DE KIRA (NUNCA borrar, NUNCA sync con --delete sin exclude)
│   │   └── Personaje/      ← kira.json / kiro.json (sus "hogares")
│   ├── Frontend/           ← React + Vite + TS (LA WEB — código fuente)
│   │   └── dist/           ← el build (esto es lo que se deploya)
│   ├── Referencias/        ← repos de investigación (Stanford genagents)
│   └── PLAN_PERSONALIDAD_EMERGENTE.md  ← la biblia del proyecto
├── MicroBit/               ← firmware C++ (CODAL) + docs (MICROBIT.md, MEMORIA.md)
├── Celular/                ← la odisea Bluetooth + modo feria (LEEME.md)
└── REQUISITOS.md, etc.
```

## ☁️ El VPS (producción)

| Dato | Valor |
|---|---|
| IP | `207.244.244.197` (usuario: `root`, contraseña abajo ⚠️) |
| URL pública | **https://207.244.244.197.nip.io** (HTTPS automático por Caddy) |
| SO | Ubuntu 24.04, 4 cores, 8GB RAM |
| App en | `/opt/kira/` (Backend + Frontend/dist + Personaje) |
| Servicio | systemd `kira` → uvicorn en 127.0.0.1:9000 |
| Proxy TLS | Caddy (config: `/etc/caddy/Caddyfile`, bloque `207.244.244.197.nip.io`) |
| Logs | `journalctl -u kira -n 50` (ver qué pasa en vivo) |

## 🚀 DEPLOY (los 3 comandos, MEMORIALIZARLOS)

```bash
PW='CONFIGURAR_ESTA_VARIABLE_EN_TU_ENTORNO'

# 1) BACKEND (si tocaste kira_server.py o config.py):
sshpass -p "$PW" rsync -az --delete \
  --exclude ".venv" --exclude "__pycache__" --exclude "grabaciones" --exclude "memoria" --exclude "conversaciones" \
  -e "ssh -o StrictHostKeyChecking=no" \
  Cerebro/Backend/ root@207.244.244.197:/opt/kira/Backend/

# 2) FRONTEND (si tocaste la web: PRIMERO build, luego sync):
cd Cerebro/Frontend && npm run build && cd ../..
sshpass -p "$PW" rsync -az --delete \
  -e "ssh -o StrictHostKeyChecking=no" \
  Cerebro/Frontend/dist/ root@207.244.244.197:/opt/kira/Frontend/dist/

# 3) PERSONAJES (si tocaste kira.json/kiro.json):
sshpass -p "$PW" rsync -az \
  -e "ssh -o StrictHostKeyChecking=no" \
  Cerebro/Personaje/ root@207.244.244.197:/opt/kira/Personaje/

# 4) REINICIAR el backend (backend/personajes; el frontend no necesita):
sshpass -p "$PW" ssh -o StrictHostKeyChecking=no root@207.244.244.197 \
  "systemctl restart kira && sleep 2 && systemctl is-active kira"

# Verificación de humo:
curl -s -o /dev/null -w "%{http_code}\n" https://207.244.244.197.nip.io/        # → 200
curl -s https://207.244.244.197.nip.io/api/status                               # → JSON
```

## ⚠️ REGLAS DE ORO (romperlas = matar a Kira)

1. **NUNCA sync `memoria/` con `--delete`** — son los RECUERDOS y el DIARIO
   de las IA (personalidad emergente). El exclude ya está en el comando:
   no lo saques. Lo mismo `grabaciones/` (audios) y `.venv`.
2. **La web no necesita reiniciar el servicio** — solo rsync del `dist/`.
   El `systemctl restart kira` es SOLO para backend/personajes.
3. **El build debe pasar limpio** (`npm run build` sin errores de TS) antes
   de deployar. Test mínimo post-deploy: curl a `/` y `/api/status`.
4. **No cambiar el contrato de la API** que consume el frontend (abajo).
5. El `codal.json` / firmware: no tocar sin leer `MicroBit/MICROBIT.md`
   (el build cachea — requiere limpiar `build/` para cambios de config).

## 🔌 Contrato API que la web usa (no romper)

- `POST /api/chat/stream` — SSE: eventos `delta` (texto parcial),
  `audio` ({url, orden}: una frase ya lista para hablar), `fin`
  ({emotion, message, tts_url, tts_urls, fuentes}), `error`, `reintento`.
- `POST /api/transcribir` (FormData archivo) — voz del celular → texto
- `POST /api/loading`, `/api/voz`, `/api/talk`, `/api/emocion` {emotion} —
  estados del micro:bit (loading / aro escucha / boca / cara)
- `GET /api/status` — {microbit: {conectado, ble_relay, leds, ...}}
- BLE (PuenteBle.tsx): `GET /api/ble/tx?t=&espera=`, `POST /api/ble/rx`,
  `POST /api/ble/conectar|desconectar|error` (con token de dueño)
- `GET /api/personajes`, conversaciones (localStorage del navegador)
- `GET /api/memoria/{personaje}` — superficie de memoria (panel 🧠), con
  `hechos`, `experiencias` y `reflexiones` separados
- `GET /api/conversaciones/{personaje}?limite=N` — historial debug crudo
  (usuario + respuesta + emoción + herramientas + errores)

## 🧠 Datos vivos en el VPS (runtime, no código)

- `memoria/*.jsonl` — recuerdos; `memoria/diario_kira.json` — SU personalidad;
  `memoria/diario_snapshots/` — línea de tiempo del carácter.
- El backend los lee/escribe en vivo: borrarlos = lobotomizarla.

## 🎨 Si tu trabajo es el FRONTEND (interfaz gráfica)

- Código: `Cerebro/Frontend/src/` (App.tsx = todo el chat; estilos.css
  central; PuenteBle.tsx = puente Bluetooth — CUIDADO con ese archivo:
  maneja la reconexión, wake-lock y tokens; cambiarlo sin entender rompe
  el modo feria).
- Tipos de web-bluetooth instalados (`@types/web-bluetooth`).
- Responsive ya tiene base (media queries al final de estilos.css).
- TTS: usa un elemento de audio persistente desbloqueado por gesto —
  NO volver a `new Audio()` por mensaje (autoplay de Android lo bloquea).
- Sonidos UI: `sonidos.ts` (toggle en ajustes).

## ✅ Checklist después de cualquier cambio

```
[ ] npm run build sin errores
[ ] rsync correcto (con los --exclude si es backend)
[ ] systemctl restart kira (si tocaste backend)
[ ] curl / → 200  y  /api/status → JSON
[ ] abrir la web en el cel (recarga dura) y probar un mensaje
[ ] journalctl -u kira -n 20 sin errores nuevos
```
