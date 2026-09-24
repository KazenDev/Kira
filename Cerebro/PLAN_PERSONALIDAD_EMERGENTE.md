# PLAN_PERSONALIDAD_EMERGENTE.md — Kira se cría sola 🐣🧬

> El propósito científico: Kira **no recibe** una personalidad escrita por
> nosotros — la **construye** viviendo: charlas → memorias → reflexiones →
> opiniones propias con evidencia citable.
>
> Basado en proyectos REALES (código leído, no papers resumidos):
> - **Generative Agents** — Park et al., Stanford 2023 (repo clonado en
>   `Cerebro/Referencias/generative_agents/`)
> - **MemGPT/Letta** — Packer et al., UC Berkeley (patrón de memoria
>   auto-editada con reemplazo exacto + snapshots)
> - **Moxie (Embodied)** — el caso negativo: personalidad guionizada
>   inconsistente y robot que MURIÓ con la empresa (lección: open + propio)

## 🎯 Hipótesis de feria (el experimento)

> *"¿Una IA que construye su personalidad por experiencia genera mayor
> percepción de vida y opiniones propias que una con personalidad
> instalada?"* — medible con la encuesta del stand.

## 🏗️ Arquitectura

```
┌─ kira.json — EL HOGAR (intocable por ella) ──────────────┐
│ valores base, público infantil, estilo opinionado,       │
│ contrato de emociones JSON + herramientas                 │
└───────────────────────────────────────────────────────────┘
┌─ memoria/kira.jsonl — LO VIVIDO (flujo de memoria) ──────┐
│ {id, fecha, tipo: observacion|pensamiento, texto,        │
│  importancia 1-10, expira: fecha}                        │
└───────────────────────────────────────────────────────────┘
┌─ diario_kira.json — LO QUE ELLA ES (solo ella lo edita) ┐
│ yo_soy: párrafo autorreescrito en cada reflexión         │
│ opiniones: [{texto, evidencia, fecha}]                   │
│ gustos: [] / personas_que_conozco: []                    │
│ (máx ~1-2KB, snapshots con fecha antes de cada edición)  │
└───────────────────────────────────────────────────────────┘
Cada turno: HOGAR + SU DIARIO + historial → habla como se crió.
```

## 📋 FASES

### FASE 1 — EL CUADERNO (memoria) `[primer paso: LA CHARLA + MEMO]` ✅ HECHA (04-sep)
- Implementada en kira_server.py (bloque PERSONALIDAD EMERGENTE) + deployada
- Hook: memo en background justo antes del evento "fin" del chat
- Tests: Cerebro/Backend/test_memoria.py (16/16 OK, incluida integración
  real — memos en primera persona, importancia 1-10, expiración 30 días,
  gatillo determinístico 15x10=150)
- ⚠ DEPLOY: rsync SIEMPRE con --exclude "memoria" (los recuerdos son
  datos de runtime, no código)
- Guardar cada charla terminada como memorias:
  - **MEMO post-charla** (prompt real de Stanford, en español):
    *"Anotá en UNA frase si hubo algo en la conversación que hayas
    encontrado interesante, desde TU perspectiva"*
  - **Importancia** (prompt real de Stanford):
    *"De 1 a 10, donde 1 es mundano (lavarse los dientes) y 10 es
    conmovedor (una ruptura), calificá para Kira: [memoria]"*
  - Cada memoria resta su importancia del CONTADOR DE REFLEXIÓN
- Formato: `memoria/kira.jsonl` (una línea por memoria)

### MEMORIA EXPLÍCITA SEMÁNTICA (2026-09-24)
- La captura de identidad y otros hechos durables ya no usa regex ni una
  decisión determinista del backend.
- Después de cada turno, un extractor LLM devuelve hechos JSON con confianza,
  importancia y una cita literal del usuario. El backend sólo aplica guardrails
  de forma; si no hay confianza (`>= 0.9`) o evidencia válida, no escribe.
- Los cambios de identidad se versionan con `supersedes` y entran en un bloque
  prioritario del system prompt, separado del RAG general.
- La tarea corre en background en `/api/chat` y `/api/chat/stream`; el JSONL
  sigue siendo la fuente de verdad y el índice RAG es derivado.
- El memo no se guarda por inercia: puede responder `NADA`, se descarta bajo
  `6/10` y se deduplica. La herramienta de guardar exige una orden explícita
  del usuario; las experiencias quedan separadas de los hechos durables.
- Tests: `test_identity.py` cubre extracción, evidencia, cambios de nombre,
  captura post-turno y ambos endpoints.

### FASE 2 — LA REFLEXIÓN (el momento de pensar) ✅ HECHA (04-sep)
- Implementada en kira_server.py + deployada. Tests: test_reflexion.py (23/23)
- Pesos REALES del repo de Stanford (retrieve.py:244): puntaje =
  0.5×novedad(0.995^i) + 3×relevancia(jaccard kw v1) + 2×importancia,
  todo normalizado min-max, top 30 por foco
- 3 focos → 3 insights c/u CON evidencia citada por id → tipo="pensamiento"
- Guardrails: conclusión sin evidencia citada = DESCARTADA; candado
  anti-reflexiones simultáneas ("reflexionando"); mínimo 6 memorias
- Importancia de conclusiones en lote (1 llamada por foco)
- Dispara sola: al cruzar el umbral, el memo lanza la reflexión en background
- Detalle fino pendiente (v2): refrescar "último acceso" de recuerdos
  recuperados (Stanford lo hace; con nuestro volumen es marginal)

### MEMORIA EN CHARLA (converse.py de Stanford) ✅ HECHA (04-sep)
- Tests: test_recuerdos.py (13/13). El caso real del usuario reproducido:
  chat A (tomboys) → chat B pregunta → la memoria influye en la respuesta
- En cada mensaje: recuperación LOCAL (gratis) top-8 por novedad+
  relevancia+importancia, foco = mensaje + últimas charlas
- Frescos (<15 min) excluidos: ya viven en el historial del prompt
- Con ≥3 recuerdos: LA LLAMADA DE STANFORD (summarize_ideas) los comprime
  en una frase natural que CONSERVA términos concretos (prompt ajustado
  tras detectar eufemización: "tomboys" → "ese estilo")
- Sin recuerdos → bloque vacío → costo cero (hoy no se dispara nunca)
- Inyectado en ambos endpoints junto al diario: "# LO QUE TE VIENE A LA
  MENTE (recuerdos de tu vida, no de esta charla)"

### FASE 3 — EL DIARIO (la personalidad que ella escribe) ✅ HECHA (04-sep)
- Implementada + deployada. Tests: test_diario.py (24/24) — el guardrail
  de evidencia RECHAZO una opinion sin sustento en un test real 🔒
- Tras cada reflexión: diario_fusionar() reescribe SU diario
  (yo_soy ≤400 + opiniones con evidencia + gustos + personas) vía JSON
  mode con reintento anti-truncado y defensa anti-```json```
- Guardrails duros (diario_aplicar): anti-wipe (no se vacía de golpe),
  topes (20 opiniones/10 gustos/10 personas), evidencia con ids válidos
  o NO entra, snapshots datados de cada versión anterior
  (memoria/diario_snapshots/ — línea de tiempo del carácter pa' el poster)
- **INYECCIÓN AL CHAT YA ACTIVA**: ambos endpoints (/api/chat y
  /api/chat/stream) suman al system prompt el bloque "TU DIARIO" —
  desde la primera reflexión ella habla desde SU historia
- El diario vive en memoria/ (excluido del rsync: es dato de vida, no código)

### FASE 4 — LA VOZ OPINIONADA (reformatear el hogar) ✅ HECHA (04-sep)
- Personalidad escrita por humanos JUBILADA (backup en
  Personaje/original_escrito/). Tests: test_hogar.py (36/36)
- Nuevo system prompt de ambos: Rol (solo hechos) → "Tu carácter: LO
  ESCRIBÍS VOS" (desde TU DIARIO; si está vacío = recién nacida, sin
  personalidad prestada) → "Tu voz: OPINIÓN PROPIA" (tomá postura,
  PROHIBIDO "en mi opinión"/"como IA no puedo opinar"/diplomacia de
  robot) → "Hogar intocable" (bondad base, público infantil, vale más
  que su propio diario) → el resto del contrato INTACTO (emociones,
  herramientas, TTS, JSON)
- Temperamento de cuna mínimo (lo único innato): Kira "sensible" /
  Kiro "inquieto" — los gemelos divergen desde el día 1
- rol de display: "en cría 🌱" ambos
- Integración real: responde JSON válido Y con postura propia
  ("yo soy del equipo de los gatos"), cero diplomacia de robot

### FASE 5 — LA CRÍA (las semanas antes a la feria)
- Ustedes charlan normal → ella forma carácter con evidencia real
- A la feria llega con meses de vida, opiniones citables y su historia

## 🧾 Prompts reales extraídos del código de Stanford
(archivo: `Referencias/generative_agents/reverie/backend_server/persona/prompt_template/v2/`)
- `generate_focal_pt_v1.txt` — puntos focales
- `insight_and_evidence_v1.txt` — conclusiones con evidencia
- `memo_on_convo_v1.txt` — el memo post-charla
- `poignancy_event_v1.txt` — escala de importancia 1-10
- Gatillo: `cognitive_modules/reflect.py:135` (contador de importancia)

## 🔍 PENDIENTE INVESTIGAR
- [ ] MCP (Model Context Protocol): ¿servidor de memoria MCP para que la
  IA edite su diario como herramienta? (investigación en curso)
- [ ] Umbral fino: 150 de Stanford ¿o escala menor al principio (chats
  cortos)? → arrancar con 150, ajustar con datos reales
