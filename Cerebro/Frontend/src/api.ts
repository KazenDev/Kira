// Cliente de la API del Backend (FastAPI en http://127.0.0.1:8000)
// En desarrollo con `npm run dev`, el proxy de Vite reenvia /api -> :8000

export interface UsuarioKira {
  id: string;
  username: string;
  created_at: string;
}

export interface RespuestaAuth {
  ok: boolean;
  user: UsuarioKira;
  csrf_token?: string;
  legacy_imported?: boolean;
}

export interface EstadoAuth {
  ok: boolean;
  user: UsuarioKira | null;
  csrf_token?: string;
}

export interface PersonajeInfo {
  id: string;
  nombre: string;
  rol: string;
  descripcion?: string;
  emoji: string;
  color: string;
  saludo: string;
}

export interface MensajeIA {
  emotion: string;
  message: string;
  temp: number;
  tts_url: string | null;
  audio_url?: string | null;
  personaje: string;
}

// una fuente de la web que Kira consultó (para mostrarla verificable)
export interface FuenteWeb {
  titulo: string;
  url: string;
}

export interface MensajeChat {
  id: string;
  rol: 'usuario' | 'kira';
  contenido: string;
  emotion?: string;
  audio_url?: string | null;
  audio_urls?: string[] | null;
  fuentes?: FuenteWeb[] | null;
  /** FOTO que mandó el usuario (data URL JPEG reducido, ~200 KB). Se muestra
   * en su burbuja y se guarda con el chat: es lo que él mismo mandó. */
  imagen?: string | null;
  hora: string;
}

export interface EstadoMicrobit {
  microbit: {
    conectado: boolean;
    respondiendo: boolean;
    puerto: string | null;
    ultimo_comando: string;
    ultimo_ack: string | null;
    leds: string | null; // replica del display real (25 chars)
    ble_relay?: boolean; // modo feria: el navegador lleva los comandos
  };
}

const CSRF_COOKIE = 'kira_csrf';
const CSRF_HEADER = 'X-Kira-CSRF';
const METODOS_SEGUROS = new Set(['GET', 'HEAD', 'OPTIONS', 'TRACE']);

function csrfCookie(): string {
  const item = document.cookie.split('; ').find((parte) => parte.startsWith(`${CSRF_COOKIE}=`));
  return item ? decodeURIComponent(item.slice(CSRF_COOKIE.length + 1)) : '';
}

/** Fetch común: cookies de sesión + token CSRF para mutaciones. */
export function fetchKira(input: RequestInfo | URL, init: RequestInit = {}): Promise<Response> {
  const headers = new Headers(init.headers);
  const metodo = (init.method ?? 'GET').toUpperCase();
  if (!METODOS_SEGUROS.has(metodo)) {
    const token = csrfCookie();
    if (token) headers.set(CSRF_HEADER, token);
  }
  return fetch(input, {
    ...init,
    headers,
    credentials: init.credentials ?? 'same-origin',
  }).then((response) => {
    const url = typeof input === 'string' ? input : input instanceof URL ? input.pathname : input.url;
    if (response.status === 401 && !url.startsWith('/api/auth/')) {
      window.dispatchEvent(new Event('kira:sesion-expirada'));
    }
    return response;
  });
}

/** fetch CON TOPE DE TIEMPO. Sin esto, si la red se cuelga a mitad de una
 * request (el celu cambia de WiFi a datos, la pantalla se apaga, nginx
 * parpadea), el `await` no resuelve NUNCA y el bucle del puente se muere en
 * silencio sin reportar nada: los logs del VPS mostraban justo eso — los
 * latidos se cortaban de golpe y 61s despues el server liberaba el puente.
 * OJO: NO se usa en el stream del chat (ese es largo a propósito). */
async function conTope(url: string, ms: number, opciones: RequestInit = {}): Promise<Response> {
  const ctrl = new AbortController();
  const reloj = setTimeout(() => ctrl.abort(), ms);
  try {
    return await fetchKira(url, { ...opciones, signal: ctrl.signal });
  } finally {
    clearTimeout(reloj);
  }
}

// ------------------- relay BLE (modo feria) -------------------
// Sin serial en el servidor: este navegador se ofrece de cable entre
// el backend y la placa por Web Bluetooth (ver PuenteBle.tsx).
// Todos los llamados llevan el TOKEN de dueño del puente: el primero
// que registra gana; 409 = otro dispositivo lo tiene.

export async function bleMarcarConectado(token: string): Promise<void> {
  const r = await conTope('/api/ble/conectar', 10000, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ token }),
  });
  if (r.status === 409) {
    const d = await r.json().catch(() => ({}));
    throw new Error(d.error ?? 'otro dispositivo tiene el puente');
  }
  if (!r.ok) throw new Error(`registro falló (${r.status})`);
}

export async function bleMarcarDesconectado(token: string): Promise<void> {
  try {
    await conTope('/api/ble/desconectar', 8000, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ token }),
    });
  } catch { /* da igual */ }
}

export async function bleTomarTx(token: string, espera = 0): Promise<string[]> {
  // el server responde a los `espera` segundos; el tope es el doble + 5s de
  // margen, para que un long-poll colgado no congele el bucle de comandos
  const r = await conTope(
    `/api/ble/tx?t=${encodeURIComponent(token)}&espera=${espera}`,
    (espera + 5) * 2000
  );
  if (r.status === 409) throw new Error('otro dispositivo tiene el puente');
  if (r.status === 410) throw new Error('puente expirado');
  if (!r.ok) throw new Error(`tx falló (${r.status})`);
  const d = await r.json();
  return d.lineas ?? [];
}

export async function blePing(token: string): Promise<void> {
  const r = await conTope(`/api/ble/ping?t=${encodeURIComponent(token)}`, 10000);
  if (r.status === 409) throw new Error('otro dispositivo tiene el puente');
  if (r.status === 410) throw new Error('puente expirado');
  if (!r.ok) throw new Error(`ping falló (${r.status})`);
}

export async function bleReportarRx(lineas: string[], token: string): Promise<void> {
  if (!lineas.length) return;
  try {
    await conTope(`/api/ble/rx?t=${encodeURIComponent(token)}`, 10000, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(lineas),
    });
  } catch { /* la proxima tanda lo lleva */ }
}

// El navegador NO pudo escribir comandos por BLE (GATT caído, timeout): los
// devuelve a la cola del server. Sin esto se perdían PARA SIEMPRE, porque
// /api/ble/tx los borra al entregarlos (así la placa se quedaba sin boca ni
// cara después de la primera respuesta).
export async function bleDevolver(lineas: string[], token: string): Promise<void> {
  if (!lineas.length) return;
  try {
    await conTope(`/api/ble/nack?t=${encodeURIComponent(token)}`, 10000, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(lineas),
    });
  } catch { /* si tampoco llega, se pierden: ni modo */ }
}

export async function bleReportarError(mensaje: string): Promise<void> {
  try {
    await conTope('/api/ble/error', 8000, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mensaje }),
    });
  } catch { /* ni modo */ }
}

// ------------------- superficie de memoria (transparencia) -------------------
// Lo que el personaje recuerda de su vida + su diario actual. La web lo
// muestra en el panel 🧠 para que cualquiera vea su mente (la feria).

export interface MemoriaVisible {
  /** ID interno de la fuente JSONL; permite olvidar una memoria puntual. */
  id?: string;
  texto: string;
  importancia: number;
  fecha: string;
  tipo: string;
  clase?: 'semantic' | 'episodic' | 'reflection' | string;
}

export interface EstadoMemoria {
  total_recuerdos: number;
  total_activos?: number;
  total_hechos?: number;
  total_experiencias?: number;
  total_reflexiones?: number;
  hechos?: MemoriaVisible[];
  experiencias?: MemoriaVisible[];
  reflexiones?: MemoriaVisible[];
  // `recuerdos` se conserva para clientes viejos; la UI nueva usa las
  // secciones curadas de arriba.
  recuerdos: MemoriaVisible[];
  diario: {
    yo_soy: string;
    opiniones: string[];
    gustos: string[];
    actualizado: string | null;
  };
}

/** Preferencias de memoria que viajan con cada turno al backend local. */
export interface PreferenciasMemoria {
  /** Interruptor maestro: sin esto no se crean recuerdos nuevos. */
  recordar: boolean;
  /** Extraer y conservar hechos explícitos de alta confianza. */
  hechos: boolean;
  /** Crear la experiencia post-charla (y habilitar reflexiones futuras). */
  experiencias: boolean;
  /** Consultar RAG/identidad al construir la respuesta. */
  usar: boolean;
}

export const PREFERENCIAS_MEMORIA_POR_DEFECTO: PreferenciasMemoria = {
  recordar: true,
  hechos: true,
  experiencias: true,
  usar: true,
};

export interface EstadoRag {
  memories?: number;
  active?: number;
  vectors?: number;
  fts?: boolean;
  embedding_model?: string;
  embedding_dimension?: number;
  min_similarity?: number;
  vector_only_min_similarity?: number;
  db_path?: string;
}

export async function obtenerMemoria(personaje: string, limite = 8): Promise<EstadoMemoria> {
  const r = await fetchKira(`/api/memoria/${encodeURIComponent(personaje)}?limite=${limite}`);
  if (!r.ok) throw new Error(`No se pudo leer su memoria (${r.status})`);
  return r.json();
}

/** Olvida una memoria puntual; el backend conserva un tombstone auditable. */
export async function olvidarMemoria(
  personaje: string,
  id: string,
  motivo = 'olvidado desde el panel de memoria'
): Promise<void> {
  const r = await fetchKira(`/api/memoria/${encodeURIComponent(personaje)}/olvidar`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ id, motivo }),
  });
  if (!r.ok) {
    const detalle = await r.text().catch(() => '');
    throw new Error(`No se pudo olvidar la memoria (${r.status})${detalle ? `: ${detalle.slice(0, 160)}` : ''}`);
  }
}

/** Borra todos los recuerdos activos, sin tocar el diario de personalidad. */
export async function borrarMemoria(
  personaje: string,
  motivo = 'memoria borrada desde Ajustes'
): Promise<number> {
  const r = await fetchKira(`/api/memoria/${encodeURIComponent(personaje)}/borrar-todo`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ motivo }),
  });
  if (!r.ok) {
    const detalle = await r.text().catch(() => '');
    throw new Error(`No se pudo borrar la memoria (${r.status})${detalle ? `: ${detalle.slice(0, 160)}` : ''}`);
  }
  const datos = await r.json().catch(() => ({}));
  return Number(datos.borradas ?? 0);
}

export async function obtenerStatus(): Promise<EstadoMicrobit> {
  const r = await fetchKira('/api/status');
  if (!r.ok) throw new Error('No se pudo leer el estado');
  return r.json();
}

export async function obtenerRagStatus(): Promise<EstadoRag> {
  const r = await conTope('/api/rag/status', 10000);
  if (!r.ok) throw new Error(`No se pudo leer el diagnóstico RAG (${r.status})`);
  const datos = await r.json();
  return (datos.rag ?? {}) as EstadoRag;
}

export async function obtenerPersonajes(): Promise<PersonajeInfo[]> {
  const r = await fetchKira('/api/personajes');
  if (!r.ok) throw new Error('No se pudieron cargar los personajes');
  const data = await r.json();
  return data.personajes;
}

export async function obtenerSesion(): Promise<EstadoAuth> {
  const r = await fetchKira('/api/auth/me');
  if (!r.ok) throw new Error(`No se pudo verificar la sesión (${r.status})`);
  return r.json();
}

export async function registrarUsuario(username: string, password: string): Promise<RespuestaAuth> {
  const r = await fetchKira('/api/auth/register', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password }),
  });
  if (!r.ok) {
    const detalle = await r.json().catch(() => ({}));
    throw new Error(String(detalle.detail ?? 'No se pudo crear la cuenta'));
  }
  return r.json();
}

export async function iniciarSesion(username: string, password: string): Promise<RespuestaAuth> {
  const r = await fetchKira('/api/auth/login', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ username, password }),
  });
  if (!r.ok) {
    const detalle = await r.json().catch(() => ({}));
    throw new Error(String(detalle.detail ?? 'No se pudo iniciar sesión'));
  }
  return r.json();
}

export async function cerrarSesion(): Promise<void> {
  const r = await fetchKira('/api/auth/logout', { method: 'POST' });
  if (!r.ok && r.status !== 401) {
    throw new Error(`No se pudo cerrar la sesión (${r.status})`);
  }
}

export async function enviarMensaje(
  personaje: string,
  mensaje: string,
  historial: { rol: string; contenido: string }[],
  imagen?: string | null,
  preferencias?: PreferenciasMemoria
): Promise<MensajeIA> {
  const r = await fetchKira('/api/chat', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({
      personaje,
      mensaje,
      historia: historial,
      imagen: imagen ?? null,
      memoria_config: preferencias ?? PREFERENCIAS_MEMORIA_POR_DEFECTO,
    }),
  });
  if (!r.ok) {
    const texto = await r.text();
    throw new Error(`El cerebro falló (${r.status}): ${texto.slice(0, 200)}`);
  }
  return r.json();
}

// Resultado final de un chat STREAMING
interface ResultadoStream {
  emotion: string;
  message: string;
  tts_url: string | null;
  tts_urls?: string[] | null; // TTS POR FRASES: la cola completa en orden
  personaje: string;
  fuentes?: FuenteWeb[] | null;
}

// Chat STREAMING real: consume /api/chat/stream (SSE) y llama onDelta con el
// texto parcial (palabra por palabra), onAudio por cada frase lista para
// hablar (TTS encadenado: la voz arranca sin esperar el texto completo),
// onFin cuando termina TODO, onError si la conexión se corta o la IA falla.
export async function enviarMensajeStream(
  personaje: string,
  mensaje: string,
  historial: { rol: string; contenido: string }[],
  onDelta: (texto: string) => void,
  onFin: (r: ResultadoStream) => void,
  onError: (e: Error) => void,
  signal?: AbortSignal,
  onAudio?: (url: string, orden: number) => void,
  sesion?: string | null,
  // FOTO del celular (data URL JPEG ya reducido): viaja en el turno nuevo y la
  // IA la VE (DeepSeek V4.1-Flash es multimodal nativo)
  imagen?: string | null,
  // tool trace: el backend avisa QUÉ herramienta está usando la IA (sensor,
  // web, cálculo...) para que la UI muestre un estado nombrado en vez de
  // puntos genéricos durante la espera
  onTool?: (nombre: string) => void,
  // privacidad/memoria se decide en el navegador y viaja en cada turno
  preferencias?: PreferenciasMemoria
): Promise<void> {
  try {
    const r = await fetchKira('/api/chat/stream', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        personaje,
        mensaje,
        historia: historial,
        sesion: sesion ?? null,
        imagen: imagen ?? null,
        memoria_config: preferencias ?? PREFERENCIAS_MEMORIA_POR_DEFECTO,
      }),
      signal,
    });
    if (!r.ok) {
      const texto = await r.text();
      throw new Error(`El cerebro falló (${r.status}): ${texto.slice(0, 200)}`);
    }
    if (!r.body) throw new Error('El stream está vacío');

    const lector = r.body.getReader();
    const decodificador = new TextDecoder();
    let buffer = '';

    for (;;) {
      const { done, value } = await lector.read();
      if (done) break;
      buffer += decodificador.decode(value, { stream: true });

      // separar eventos SSE (data: {...} separados por \n\n)
      let idx: number;
      while ((idx = buffer.indexOf('\n\n')) !== -1) {
        const evento = buffer.slice(0, idx);
        buffer = buffer.slice(idx + 2);
        const linea = evento
          .split('\n')
          .find((l) => l.startsWith('data:'));
        if (!linea) continue;
        let datos: any = null;
        try {
          datos = JSON.parse(linea.slice(5).trim());
        } catch (e) {
          // JSON parcial o corrupto: lo ignoramos, sigue el stream
          continue;
        }
        // ERROR REAL del backend: se propaga SIEMPRE (no se traga)
        if (datos.tipo === 'error') {
          throw new Error(String(datos.mensaje ?? 'Error del cerebro'));
        }
        if (datos.tipo === 'delta' && typeof datos.texto === 'string') {
          onDelta(datos.texto);
        } else if (datos.tipo === 'tool' && typeof datos.nombre === 'string') {
          // la IA llamó una herramienta: estado nombrado ("buscando en la web…")
          if (onTool) onTool(datos.nombre);
        } else if (datos.tipo === 'audio' && typeof datos.url === 'string') {
          // TTS POR FRASES: una frase ya tiene su audio listo — la cola
          // de reproducción lo toma en orden (puede llegar ANTES del fin)
          if (onAudio) onAudio(datos.url, Number(datos.orden ?? 0));
        } else if (datos.tipo === 'fin') {
          onFin({
            emotion: String(datos.emotion ?? 'neutral'),
            message: String(datos.message ?? ''),
            tts_url: datos.tts_url ?? null,
            tts_urls: Array.isArray(datos.tts_urls) ? datos.tts_urls : null,
            personaje: String(datos.personaje ?? personaje),
            fuentes: Array.isArray(datos.fuentes) ? datos.fuentes : null,
          });
          return;
        }
      }
    }
    // el stream terminó sin evento fin: error (se cortó la conexión)
    throw new Error('La conexión se cortó a mitad de la respuesta');
  } catch (e) {
    onError(e instanceof Error ? e : new Error(String(e)));
  }
}

// Resultado de la ESCUCHA MANUAL (al pulsar A por segunda vez)
export interface ResultadoEscucha {
  transcripcion: string | null;
  archivo: string | null;
  duracion_s: number;
}

// VOZ MODO FERIA: manda el audio grabado con el MICROFONO DEL CELULAR al
// servidor, que lo pasa por AssemblyAI. La escucha por el micro:bit
// (ESCUCHAR + Silero) necesita el cable USB: por BLE el audio binario no
// viaja, asi que en el stand graba el telefono (que ademas suena mejor).
export async function transcribirDesdeCel(blob: Blob): Promise<string | null> {
  const fd = new FormData();
  fd.append('archivo', blob, 'voz.webm');
  const r = await fetchKira('/api/transcribir', { method: 'POST', body: fd });
  if (!r.ok) {
    let detalle = String(r.status);
    try {
      const e = await r.json();
      if (e?.error) detalle = e.error;
    } catch { /* respuesta sin JSON */ }
    throw new Error(`No se pudo transcribir: ${detalle}`);
  }
  const d = await r.json();
  return d.transcripcion ?? null;
}

// ESCUCHA MANUAL (SSE): el microfono queda abierto hasta A (enviar) o
// B (cancelar). onNivel se llama en vivo; onFin recibe la transcripcion y
// onCancel se llama cuando el firmware descarta la captura.
export async function escucharVoz(
  onNivel: (nivel: number) => void,
  onFin: (res: ResultadoEscucha) => void,
  onError: (e: Error) => void,
  onCancel?: () => void,
): Promise<void> {
  try {
    const r = await fetchKira('/api/escuchar', { method: 'POST' });
    if (!r.ok) throw new Error(`No se pudo escuchar (${r.status})`);
    if (!r.body) throw new Error('El stream está vacío');

    const lector = r.body.getReader();
    const decodificador = new TextDecoder();
    let buffer = '';

    for (;;) {
      const { done, value } = await lector.read();
      if (done) break;
      buffer += decodificador.decode(value, { stream: true });

      let idx: number;
      while ((idx = buffer.indexOf('\n\n')) !== -1) {
        const evento = buffer.slice(0, idx);
        buffer = buffer.slice(idx + 2);
        const linea = evento.split('\n').find((l) => l.startsWith('data:'));
        if (!linea) continue;
        let datos: any = null;
        try {
          datos = JSON.parse(linea.slice(5).trim());
        } catch {
          continue;
        }
        if (datos.tipo === 'error') {
          throw new Error(String(datos.mensaje ?? 'Error de la escucha'));
        }
        if (datos.tipo === 'nivel' && typeof datos.nivel === 'number') {
          onNivel(datos.nivel);
        } else if (datos.tipo === 'fin') {
          onFin({
            transcripcion: datos.transcripcion ?? null,
            archivo: datos.archivo ?? null,
            duracion_s: Number(datos.duracion_s ?? 0),
          });
          return;
        } else if (datos.tipo === 'cancelado') {
          onCancel?.();
          return;
        }
      }
    }
  } catch (e) {
    onError(e instanceof Error ? e : new Error(String(e)));
  }
}

export function horaActual(): string {
  return new Date().toLocaleTimeString('es-AR', { hour: '2-digit', minute: '2-digit' });
}

// genera el titulo del chat (emoji + titulo corto IA) con el primer intercambio
export async function generarTitulo(
  primerMensaje: string,
  primeraRespuesta: string
): Promise<string> {
  const r = await fetchKira('/api/titulo', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ primer_mensaje: primerMensaje, primera_respuesta: primeraRespuesta }),
  });
  if (!r.ok) throw new Error('No se pudo generar el titulo');
  const d = await r.json();
  return d.title ?? primerMensaje.slice(0, 30);
}
