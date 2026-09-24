# TODO — Mejora progresiva del backend de Kira

> Objetivo: ordenar y robustecer el backend **sin cambiar el contrato que consume el frontend**.
> No se utilizan PRs ni Git. Cada fase se verifica antes de comenzar la siguiente.

## Reglas de trabajo

- [x] Auditar `Cerebro/Backend` sin modificarlo.
- [x] Confirmar que `kira_server.py` es el monolito principal y mapear sus seams.
- [ ] No cambiar rutas, métodos, cuerpos JSON ni eventos SSE.
- [ ] No modificar el protocolo serial/BLE.
- [ ] No tocar ni sincronizar `memoria/`, `conversaciones/` o `grabaciones/` en deploys.
- [ ] No desplegar al VPS sin una instrucción explícita.
- [ ] La fase de seguridad queda **aplazada por decisión del usuario**.

## Fase 1 — Armazón modular ✅

- [x] Crear paquete `app/` con `main.py` y factory `create_app()`.
- [x] Mantener `kira_server:app` como import compatible para Uvicorn y tests.
- [x] Separar endpoints en `APIRouter` por dominio.
- [x] Incorporar schemas Pydantic sin cerrar campos existentes de forma incompatible.
- [x] Mover configuración de CORS y lifecycle a la app factory.
- [x] Añadir pruebas de contrato del app y sus rutas.
- [x] Ejecutar compilación, tests puros y tests de integración controlados.
- [x] Documentar exactamente qué cambió.

## Fase 2 — Dominio puro ✅

- [x] Extraer helpers de visión/foto.
- [x] Extraer normalización emocional y helpers SSE.
- [x] Extraer segmentación/deduplicación de frases TTS.
- [x] Extraer recuperación de recuerdos y guardrails puros.
- [x] Extraer catálogo, parsing y deduplicación de herramientas.
- [x] Reexportar símbolos desde `kira_server.py` para no romper tests actuales.

## Fase 3 — Servicios e infraestructura ✅

- [x] Extraer `AIProvider` sin cambiar retries/fallback/eventos.
- [x] Extraer `SpeechService` con el mismo comportamiento lazy y orden de frases.
- [x] Extraer `MemoryStore` manteniendo formatos en disco.
- [x] Añadir escrituras atómicas y locks por personaje.
- [x] Registrar/cancelar tareas de memo y reflexión.
- [x] Compartir clientes `httpx.AsyncClient` desde lifespan.

## Fase 4 — Hardware ✅

- [x] Separar `SerialTransport`, `BleRelayHub` y estado del dispositivo.
- [x] Serializar operaciones físicas globales.
- [x] Corregir captura binaria alrededor de `AUDIO:START`.
- [x] Usar temporales únicos para WAV/ffmpeg.
- [x] Añadir cierre limpio del serial y revisar el montaje estático.

## Fase 5 — Pruebas y operación diaria ✅

- [x] Estandarizar tests como pytest sin perder los scripts ejecutables actuales.
- [x] Aislar por defecto toda API externa en tests.
- [x] Separar smoke tests live de las pruebas deterministas.
- [x] Añadir pruebas de concurrencia para memoria, TTS y serial.
- [x] Actualizar documentación de arquitectura y troubleshooting.

## RAG de memoria

El plan, decisiones, fuentes y estado detallado están en [`TODO_RAG.md`](TODO_RAG.md).
La primera implementación ya usa FastEmbed E5-small local + SQLite FTS5 +
vectores + RRF, sin cambiar la fuente de verdad JSONL.

Estado actual: provider, índice, backfill, búsqueda híbrida, lifecycle y tests
están implementados. Quedan la calibración golden y la evaluación de calidad
antes de considerar el RAG cerrado para producción.

## Memoria explícita post-turno

- [x] Extraer hechos durables con una llamada semántica estructurada en background.
- [x] Exigir confianza alta y evidencia literal del usuario; no guardar por regex.
- [x] Versionar cambios de identidad con `supersedes` y priorizarlos en el prompt.
- [x] Integrar la captura en chat normal y SSE.
- [x] Probar extracción, rechazo de evidencia inválida, cambios de nombre y ambos endpoints.

## Seguridad — aplazada

No se modifica en esta tanda, por decisión explícita del usuario:

- [ ] Rotación de claves y contraseña.
- [ ] Autenticación/autorización.
- [ ] CORS restringido.
- [ ] Rate limits y límites de upload.
- [ ] Protección SSRF de `leer_url`.
- [ ] Gestión segura de secretos.
- [ ] Rechazar `ble/rx` con token expirado.

## Registro de avance

### 2026-09-23 — Auditoría

- `kira_server.py`: 3.393 líneas, 29 endpoints.
- Runtime principal concentrado en un archivo; `config.py`, `vad.py` y tests ya están separados.
- Se decidió no hacer reescritura total ni mover SSE y hardware a la vez.

### 2026-09-23 — Fase 1 completada

- `kira_server.py` bajó de 3.393 a 3.094 líneas.
- Nuevo paquete `Cerebro/Backend/app/` con fábrica, contexto y schemas.
- Los 36 paths API se declaran en los routers de cuentas, sistema, dispositivo, chat y audio.
- `kira_server:app` y todos los paths/métodos siguen iguales.
- La implementación de chat, memoria, TTS y serial continúa en el runtime legado;
  en esta fase sólo se movió la superficie HTTP y el seam de inyección.
- Se añadieron schemas permisivos: siguen aceptando campos extras y errores legacy.
- Se añadió `test_app_contract.py`: contrato completo, lifespan, status, hardware,
  BLE y título con IA falsa.
- Verificación: compilación limpia; contrato, relay, tools, fallback, stream y visión
  determinista aprobados.
- Los smoke tests live siguen fallando por proveedor principal HTTP 402 y fallback
  HTTP 410; no son regresiones de la fase 1.
- Seguridad y CORS quedaron exactamente como estaban, según lo pedido.

### 2026-09-23 — Fase 2 completada

- `kira_server.py` bajó de 3.094 a 2.773 líneas.
- Se extrajeron módulos puros de visión, streaming/SSE, segmentación TTS,
  recuperación de memoria, guardrails del diario y protocolo de tools.
- Los módulos puros no importan FastAPI, serial, red ni sistema de archivos.
- `kira_server.py` reexporta los nombres anteriores; los tests y monkeypatches
  existentes siguen funcionando.
- Tests de dominio, contrato, stream, visión, tools, relay y fallback aprobados.
- No se cambió la forma del SSE, la cola TTS, los archivos de memoria ni el
  comportamiento de herramientas.

### 2026-09-23 — Fase 3 completada

- `kira_server.py` bajó de 2.773 a 2.482 líneas.
- `AIProvider` conserva los mismos proveedores, retries, fallback y eventos SSE.
- `SpeechService` conserva el TTS lazy, la cache FIFO y el payload de Fish.
- `MemoryStore` mantiene JSON/JSONL, pero ahora usa tempfile + `os.replace`,
  locks por personaje y `fsync` para estado, diario e historial.
- La reflexión usa un claim atómico: dos memos simultáneos no pueden dispararla
  dos veces; si no hay recuerdos suficientes, limpia el pendiente y evita loops.
- Las tareas de memo/reflexión quedan registradas y se cancelan al apagar el server.
- IA y TTS reutilizan clientes HTTP administrados por lifespan.
- `test_services.py` cubre concurrencia (24 escrituras), claim de reflexión,
  storage atómico, TTS lazy y lifecycle de tareas.
- Se mantienen aliases y wrappers para no romper imports ni monkeypatches legacy.

### 2026-09-23 — Fase 4 completada

- `kira_server.py` bajó de 2.482 a 2.002 líneas.
- `SerialTransport` ahora vive en `app/hardware/serial_transport.py`;
  `SerialManager` queda como alias compatible.
- `BleRelayHub`/`RelayBroker`, `DeviceState` y el parser de audio están separados.
- Grabación, escucha y sensores comparten un lock exclusivo por operación física.
- Se corrigió el parser `AUDIO:START`/`AUDIO:END` para chunks partidos y remanentes.
- WAV/ffmpeg y grabaciones usan nombres únicos; ya no hay `_tmp.wav` compartido.
- `SerialTransport.close()` cierra puerto y worker; lifespan lo invoca.
- `test_hardware.py` cubre parser partido, estado, exclusión, cierre y conversiones
  concurrentes sin colisiones.
- La autenticación/expiración BLE sigue deliberadamente sin tocar: pertenece a la
  fase de seguridad aplazada.

### 2026-09-23 — Fase 5 completada

- Se añadió `run_tests.py`: 14 suites deterministas, sin serial ni proveedores.
- Se añadió `run_live_tests.py`: los smoke tests externos requieren `--yes`.
- Se añadieron `pytest.ini`, `requirements-dev.txt` y `tests/` para migración
  gradual a pytest sin romper los scripts ejecutables.
- La documentación `Cerebro/ARQUITECTURA.md` fue actualizada al árbol modular,
  contratos SSE/HTTP, datos locales, lifecycle y troubleshooting.
- La suite determinista completa pasó: 14/14.
- pytest quedó instalado en el entorno de desarrollo y `pytest -q` pasó: 5/5.
- No se ejecutaron smoke tests live en esta fase. La 402/410 de proveedores
  sigue siendo una limitación externa conocida, no un fallo de los cambios.
- No se tocó seguridad ni se hizo deploy.

### 2026-09-23 — Memoria explícita semántica

- Se quitó el autoahorro de nombres por regex: ahora la IA extrae hechos
  durables en JSON después de cada turno.
- Se validan confianza y evidencia literal del usuario; la incertidumbre no
  escribe memoria.
- La identidad semántica se versiona, tiene bloque prioritario y funciona en
  chat normal y SSE.
- La clave de DeepSeek queda cargada desde `.env`; su valor no está en
  `config.py`.
- Verificación: `run_tests.py` 14/14, `pytest -q` 5/5, `compileall` y `pip check`
  correctos. No se ejecutaron llamadas live ni se desplegó.
- La memoria ahora separa hechos, experiencias y reflexiones; el memo trivial se
  descarta, las herramientas requieren pedido explícito y el panel ya no mezcla
  todo como “recuerdos”.

### 2026-09-24 — Cuentas y aislamiento

- Se añadieron usuarios, sesiones opacas server-side, cookies HttpOnly y CSRF.
- La primera cuenta copia el snapshot legacy de Kira; el original queda intacto.
- Memoria, diario, conversaciones, grabaciones y RAG se particionan por cuenta.
- El hardware/BLE y los clientes de proveedores siguen compartidos.
- Verificación: pruebas de AuthStore/TenantManager, `pytest -q` 7/7 y suite
  determinista 14/14.
