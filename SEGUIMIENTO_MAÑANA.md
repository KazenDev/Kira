# 📌 KIRA & KIRO — Estado al 5 de agosto (día 2)

## ✅ MICRÓFONO (CERRADO ayer, re-verificado HOY)
- `micFftDetener()` implementado y flasheado. HOY se re-flasheó el firmware
  (limpió el RX del UART que estaba con basura del caos de puertos de ayer).
- **Verificado hoy**: ACK:HAPPY, ACK:LOAD0, réplica LED fluyendo, server OK.

## ✅ FRONTEND — TODO LO QUE PEDISTE AYER (hecho HOY)
1. **Descripción de bienvenida** → "¡Bienvenido al chat de la feria de las
   ciencias! 🎉 Experimentá conversando con dos hermanos digitales..." (App.tsx)
2. **Logo quieto**: `animation: flotar` sacado de `.avatar-svg` (la ballena ya
   NO vuela en burbujas/avatares del chat). El hero de bienvenida sí flota.
3. **STREAMING REAL palabra por palabra** ✅ FUNCIONA (verificado con curl):
   - Backend: `/api/chat/stream` (SSE) con `stream: true` + JSON mode.
     Extrae `message` progresivamente con regex → eventos `delta` / `fin` / `error`.
   - Frontend: `enviarMensajeStream()` (fetch + ReadableStream) → mensaje parcial
     que crece (typewriter con cursor parpadeante en `Burbuja` con prop `parcial`).
   - El loading SIGUE hasta que el TTS arranca (`jugarAudio` corta `escribiendo`
     en `onplaying`); sin voz se corta y va la emoción directo.
   - Errores mid-stream: evento `error` → mensaje parcial vacío se borra.
4. **Sidebar colapsable** con animación (botón « en el header, width 330→62px
   con transición, clase `.colapsada`). En móvil siempre se abre completo.

## 🔎 DETALLES TÉCNICOS DEL STREAMING
- SSE: `data: {"tipo":"delta","texto":"..."}` → `data: {"tipo":"fin",...}`.
- El backend NO manda la emoción al micro:bit al terminar (el frontend controla
  LOADING → TALK → emoción con el audio).
- `/api/chat` (no-streaming) sigue intacto como fallback.

## 🔧 ESTADO ACTUAL
- Server corriendo en :8000 (setsid nohup ./run.sh)
- Micro:bit conectado y respondiendo (ACK, réplica LED en vivo)
- Frontend compilado en dist/

## 💾 Rutas rápidas
- Firmware: `MicroBit/Codigo/` → rsync a `Referencias/.../source/` → `build.py` → copiar hex a `/media/zkazen/MICROBIT/`
- Frontend: `Cerebro/Frontend/` → `npm run build`
- Backend: `Cerebro/Backend/` → `./run.sh`

## 📋 POSIBLES SIGUIENTES PASOS
- Probar el streaming en el navegador de verdad (recargar la página)
- Pulir velocidad del typewriter (intervalo entre deltas)
- Verificar el flujo completo: mensaje → loading → typewriter → audio → emoción
