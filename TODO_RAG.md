# RAG serio para la memoria de Kira

> Objetivo: recuperar recuerdos por significado sin convertir la memoria en una caja negra.
> La fuente de verdad sigue siendo `memoria/*.jsonl`; el índice RAG es derivado y reconstruible.

## Decisiones

- Embeddings: **FastEmbed + `intfloat/multilingual-e5-small`**, local, 384 dimensiones.
- Queries: prefijo `query:`; documentos: prefijo `passage:`.
- Búsqueda: FTS5/BM25 + coseno vectorial + Reciprocal Rank Fusion.
- Idioma: multilingüe, con foco en español.
- Privacidad: no enviar recuerdos a Exa, Brave ni otro proveedor para embeddarlos.
- Brave queda para búsqueda web; el backend ya tiene Exa para `buscar_en_web`.
- Si FastEmbed/modelo no está disponible, se conserva el fallback lexical actual.
- Umbral vectorial calibrado inicialmente en `0.80`: E5 usa ranking relativo y
  sus scores absolutos no implican un corte universal en `0.84`.
- Cuando FTS no aporta coincidencia, el vector exige `0.84`; cuando FTS
  corrobora, se permite el ranking relativo de E5. Así no se inventan recuerdos
  para consultas sin evidencia.

## Memoria explícita: semántica, no regex

- Después de cada turno, Kira analiza la conversación como extractora de hechos
  durables y devuelve JSON con `text`, `tipo`, `confidence`, `evidence` e
  `importance`.
- El backend no decide el nombre por coincidencias: sólo acepta hechos con
  confianza de al menos `0.9` cuya evidencia sea una cita del mensaje del
  usuario. La validación de formato/evidencia es un guardrail; la decisión
  semántica sigue siendo del modelo.
- Si la IA no está segura, devuelve `facts: []` y no se modifica el JSONL.
- Los hechos de identidad se versionan con `supersedes` y se inyectan en un
  bloque prioritario; no se duplican en el bloque RAG general.
- La captura ocurre en background después de responder, tanto en chat normal como
  en SSE. El turno actual sigue estando en el historial, por lo que no se pierde
  la información durante esa misma charla.

## Memoria: admisión y capas

- Separó hechos semánticos (`memory_class=semantic`), experiencias/episodios
  (`episodic`) y reflexiones (`reflection`) en la superficie de memoria.
- El memo post-turno ya no se guarda por inercia: el modelo puede responder
  `NADA`, y debajo de `6/10` no entra como recuerdo. Antes de guardar, se
  deduplica contra memorias semánticas existentes.
- La herramienta `guardar_recuerdo` exige `explicit_user=true`; una frase
  fortuita o una reacción de Kira ya no se guarda por el mero hecho de sonar
  importante.
- Los hechos usan una `key` canónica (`persona:nombre:...`,
  `persona:nacimiento:...`) para que una corrección supersede la versión vieja
  en vez de crear otra copia.
- El panel de la web muestra hechos y experiencias en secciones separadas; no
  presenta cada observación interna como un dato sobre la persona.

La clasificación sigue EXA/Letta/MemGPT: lo durable se actualiza y consolida;
lo efímero queda en el historial; no se convierte cada interacción en una
regla permanente.

## Fases

### R0 — Fundaciones ✅

- [x] Provider de embeddings lazy y configurable.
- [x] Cache local del modelo, sin meterlo en `memoria/`.
- [x] Embeddings de prueba deterministas para tests.
- [x] Índice SQLite derivado con FTS5.
- [x] Vectores float32, versión y hash de modelo.
- [x] Reindexado idempotente desde JSONL.

### R1 — Recuperación

- [x] Búsqueda híbrida BM25 + coseno.
- [x] RRF con pesos y fallback lexical.
- [x] Filtros por personaje, estado, expiry y frescura.
- [x] Provenance/IDs recuperables.
- [x] Integración en `memoria_recuerdos_bloque`.
- [x] Integración en recuperación de reflexión.
- [ ] Golden queries y métricas recall@k/MRR.

### R2 — Memoria viva

- [x] Guardar una versión nueva sin destruir el JSONL.
- [x] `supersedes` para correcciones.
- [x] Olvido/soft-delete mediante tombstone.
- [x] Deduplicación exacta por hash para la herramienta de guardado.
- [x] Deduplicación semántica conservadora en `guardar_recuerdo` (umbral 0.97).
- [ ] Calibrar ese umbral con más casos golden.
- [x] Herramientas `buscar_recuerdos`, `actualizar_recuerdo` y `olvidar_recuerdo`.
- [x] Reconstrucción y auditoría del índice.
- [x] Hechos durables extraídos por la IA al cerrar cada turno, con confianza y
  evidencia literal del usuario; la extracción corre en background y no delega
  la decisión a un regex.
- [x] Identidad recordada por evidencia semántica y bloque prioritario en los dos
  endpoints; si la IA no está segura, no se escribe nada.

### R3 — Operación

- [x] `rebuild_rag_index.py`.
- [x] `prepare_rag_model.py`.
- [x] `evaluate_rag.py` para `recall@k` y `MRR@k`.
- [ ] Métricas de latencia, cache y cobertura.
- [x] Documentación de modelo, cache y modo offline.
- [x] Tests live separados de tests deterministas.

## Criterios de seguridad funcional

- Las APIs de Kira se protegen con sesión de cookie para el nuevo sistema de
  cuentas; el hardware físico sigue siendo compartido.
- El primer registro copia el snapshot legacy de Kira a un directorio de
  usuario; el original queda intacto y las cuentas siguientes no lo ven.
- Los archivos actuales no se borran ni se reescriben: olvidar agrega
  tombstones y borrar memoria deja el diario intacto.
- Un índice faltante/corrupto se reconstruye desde JSONL.
- Un embedding viejo se puede invalidar por versión.
- Si el modelo falla, el chat sigue funcionando con fallback lexical.
- Los embeddings son locales; la extracción semántica de hechos es una tarea
  post-turno en background y sus fallos no bloquean ni rompen el chat.

## Fuentes consultadas

- Hugging Face model card `intfloat/multilingual-e5-small`:
  https://huggingface.co/intfloat/multilingual-e5-small
- Hugging Face model card `BAAI/bge-m3` (descartado por peso para CPU):
  https://huggingface.co/BAAI/bge-m3
- FastEmbed README/docs y catálogo de modelos:
  https://github.com/qdrant/fastembed
- Documentación de Brave Search API (web search, no embeddings):
  https://api-dashboard.search.brave.com/documentation
- SQLite FTS5:
  https://www.sqlite.org/fts5.html
- SQLite-vec/RRF patterns y Memento-Memory:
  https://github.com/glee4810/sqlite-vec
  https://github.com/Basiliskode/Memento-Memory
- Generative Agents (Park et al. 2023):
  https://arxiv.org/abs/2304.03442
- Letta memory architecture:
  https://docs.letta.com/agent-sdk/memory/index.md
- MemGPT memory management:
  https://memgpt.readme.io/docs/test_page

## Progreso

### 2026-09-23 — Primera implementación

- Se eligió FastEmbed + E5-small local, 384 dimensiones.
- Se verificó el modelo real y se dejó cacheado fuera de `memoria/` (~241 MB con
  tokenizer/ONNX).
- Se reconstruyeron 7 recuerdos existentes; el JSONL no se modificó.
- Se ajustó el filtro de similitud para no convertir scores bajos de E5 en
  recuerdos irrelevantes; las consultas sin evidencia ahora pueden devolver vacío.
- La calibración local bajó el corte vectorial de `0.84` a `0.80`: con el
  corpus real, la consulta de identidad quedó con fuente `vector` y el RAG no
  depende de una llamada externa para conservar sus resultados.
- `test_rag.py` cubre FTS5, vectores, RRF, supersede, tombstone y chat retrieval.
- `test_identity.py` cubre extracción semántica, evidencia, cambios de nombre,
  taxonomía de fecha/nombre y el bloque de identidad en
  `/api/chat` y `/api/chat/stream`.
- `test_herramientas.py` verifica que guardar requiere pedido explícito.
- `test_memory_policy.py` verifica que un memo trivial no se guarda y que una
  experiencia válida queda clasificada como `episodic`.
- El panel de memoria separa hechos, experiencias y reflexiones, permite olvidar
  una entrada puntual y Ajustes agrupa memoria/privacidad, chats, dispositivo y
  diagnóstico.
- Las preferencias de memoria viajan por turno: maestro, hechos, experiencias y
  uso del RAG; apagar no borra el histórico.
- La suite determinista quedó en 14/14 y pytest en 5/5.
- El bloque de chat conserva una lista local de recuerdos si falla el resumen
  opcional del proveedor; una consulta sin evidencia devuelve vacío y no
  inventa vecinos.
- Al abrir un chat nuevo se consideran también los recuerdos recientes de otra
  sesión; dentro de una charla con historial se evita duplicarlos.
- El índice real actual tiene 19 filas y 19 vectores; el archivo derivado se
  consultó desde `memoria/rag/index.sqlite3` (no desde el SQLite temporal de tests).
- Pendiente: calibrar el umbral semántico y crear un conjunto golden más
  grande antes de llamar al RAG "serial" completo.
