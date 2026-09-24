# 📋 REQUISITOS — KIRA & KIRO

Todo lo que hace falta para correr el Cerebro en una PC limpia (Linux o Windows).

---

## 🐍 Python

| | |
|---|---|
| **Versión mínima** | **3.10** (obligatoria: el código usa anotaciones `X | None` que no existen en 3.9) |
| **Probado en** | 3.12.3 (recomendado: 3.10, 3.11 o 3.12) |
| **Descarga** | https://www.python.org/downloads/ |

> ⚠️ En Windows: marcar la casilla **"Add Python to PATH"** durante la instalación.

---

## 📦 Módulos de Python (`Backend/requirements.txt`)

```
fastapi>=0.115        # servidor web / API
uvicorn>=0.30         # server ASGI que corre FastAPI
httpx>=0.27           # llamadas HTTP a la IA y al TTS (async + streaming)
pyserial>=3.5         # serial con el micro:bit
numpy>=1.26           # VAD Silero (audio)
onnxruntime>=1.17     # corre el modelo silero_vad.onnx
assemblyai>=0.34      # transcripción de voz (grabar/escuchar)
simpleeval>=0.9       # tool "calcular" de la IA (eval seguro de mates)
```

Instalación manual (si no usás los launchers):

```bash
python3 -m pip install -r Backend/requirements.txt
```

Los launchers (`KIRA.bat` / `KIRA.sh`) instalan todo solos la primera vez.

---

## 🎬 Programas externos

| Programa | ¿Para qué? | ¿Obligatorio? |
|---|---|---|
| **ffmpeg** | Convertir grabaciones a MP3/WAV (botones de grabar y escuchar) | Solo si vas a usar el micrófono del micro:bit. El chat normal anda sin él |
| **Node.js + npm** | Recompilar el frontend (`npm run build`) | NO. El `dist/` ya viene compilado |
| **Git** (opcional) | — | NO |

**ffmpeg en Windows** (no viene de fábrica):

```bat
winget install --id Gyan.FFmpeg
```
(y cerrar/abrir la terminal para que PATH se recargue)

**ffmpeg en Linux:**
```bash
sudo apt install ffmpeg      # Debian/Ubuntu
sudo pacman -S ffmpeg        # Arch
```

---

## 🔌 Hardware

- **micro:bit v2** con el firmware de Kira flasheado (carpeta `MicroBit/`).
- Se conecta por **USB** y se detecta solo:
  - Windows → puerto `COM*` (Windows 10/11 instala el driver DAPLink solo)
  - Linux → `/dev/ttyACM0` (si no aparece: `sudo usermod -aG dialout $USER` y reloguear)
- Serial a **115200 baudios**.

---

## 🔑 API keys (ya están cargadas en `Backend/config.py`)

| Servicio | Uso |
|---|---|
| **Nano GPT** | IA (DeepSeek v4 flash, chat) |
| **Fish Audio** | Voz TTS (modelo s2.1-pro-free) |
| **AssemblyAI** | Transcripción de voz |
| **Exa** | Búsqueda web (tool de la IA) |

> Las keys viven en `Backend/config.py` (algunas leen variable de entorno si existe,
> ej: `ASSEMBLYAI_API_KEY`, `EXA_API_KEY`). OJO: no subir este archivo a un repo público.

---

## 🌐 Internet

**Obligatorio**: la IA, la voz y la búsqueda web son APIs en la nube.
Sin internet el server levanta pero Kira no puede pensar ni hablar.

---

## 🚀 Cómo arrancar

| SO | Acción |
|---|---|
| **Windows** | Doble clic en `Cerebro/KIRA.bat` |
| **Linux** | `./Cerebro/KIRA.sh` (primera vez: `chmod +x Cerebro/KIRA.sh`) |

Abrí **http://127.0.0.1:8000** (el launcher abre el navegador solo).

Sin micro:bit conectado la web anda igual (el chat, la voz y el micro:bit
virtual funcionan; solo no hay carita física real).

---

## ✅ Checklist rápido "¿por qué no anda?"

1. ¿Python 3.10+? → `python --version`
2. ¿Dependencias? → `pip install -r Backend/requirements.txt`
3. ¿ffmpeg en PATH? (solo para grabar voz) → `ffmpeg -version`
4. ¿Internet? (IA y voz son APIs online)
5. ¿API keys válidas en `config.py`?
6. ¿micro:bit? → probar `python3 MicroBit/mb.py test` (o `python MicroBit\mb.py test` en Windows)
