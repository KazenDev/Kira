// Chat: envío normal, streaming SSE, voz y títulos.
import { fetchKira } from './http';
import { PREFERENCIAS_MEMORIA_POR_DEFECTO, type PreferenciasMemoria } from './memoria';
import type {
  MensajeIA,
  ResultadoEscucha,
  ResultadoStream,
  TurnoHistorial,
} from './tipos';

export async function enviarMensaje(
  personaje: string,
  mensaje: string,
  // El historial lleva `rastro` a proposito: es lo que vuelve a Kira en el
  // siguiente turno para que sepa de donde salio lo que dijo antes. Sin esto
  // ella ve las fuentes en la burbuja pero no las tiene en la memoria.
  historial: TurnoHistorial[],
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

// Chat STREAMING real: consume /api/chat/stream (SSE) y llama onDelta con el
// texto parcial (palabra por palabra), onAudio por cada frase lista para
// hablar (TTS encadenado: la voz arranca sin esperar el texto completo),
// onFin cuando termina TODO, onError si la conexión se corta o la IA falla.
export async function enviarMensajeStream(
  personaje: string,
  mensaje: string,
  // El historial lleva `rastro` a proposito: es lo que vuelve a Kira en el
  // siguiente turno para que sepa de donde salio lo que dijo antes. Sin esto
  // ella ve las fuentes en la burbuja pero no las tiene en la memoria.
  historial: TurnoHistorial[],
  onDelta: (texto: string) => void,
  onFin: (r: ResultadoStream) => void,
  onError: (e: Error) => void,
  signal?: AbortSignal,
  // texto = lo que se VE de esa frase: la pantalla lo escribe mientras suena
  onAudio?: (url: string, orden: number, texto?: string) => void,
  sesion?: string | null,
  // FOTO del celular (data URL JPEG ya reducido): viaja en el turno nuevo y la
  // IA la VE (DeepSeek V4.1-Flash es multimodal nativo)
  imagen?: string | null,
  // tool trace: el backend avisa QUÉ herramienta está usando la IA (sensor,
  // web, cálculo...) para que la UI muestre un estado nombrado en vez de
  // puntos genéricos durante la espera
  onTool?: (nombre: string) => void,
  // privacidad/memoria se decide en el navegador y viaja en cada turno
  preferencias?: PreferenciasMemoria,
  // AUTONOMIA: un turno que nadie escribio (la placa avisó que la tocaron,
  // que la marearon... o hace rato que nadie habla). Ver /api/eventos.
  evento?: string | null
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
        evento: evento ?? null,
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
        const linea = evento.split('\n').find((l) => l.startsWith('data:'));
        if (!linea) continue;
        let datos: any = null;
        try {
          datos = JSON.parse(linea.slice(5).trim());
        } catch {
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
          if (onAudio) {
            onAudio(
              datos.url,
              Number(datos.orden ?? 0),
              typeof datos.texto === 'string' ? datos.texto : undefined
            );
          }
        } else if (datos.tipo === 'fin') {
          onFin({
            emotion: String(datos.emotion ?? 'neutral'),
            message: String(datos.message ?? ''),
            tts_url: datos.tts_url ?? null,
            tts_urls: Array.isArray(datos.tts_urls) ? datos.tts_urls : null,
            personaje: String(datos.personaje ?? personaje),
            fuentes: Array.isArray(datos.fuentes) ? datos.fuentes : null,
            rastro: Array.isArray(datos.rastro) ? datos.rastro : null,
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
    } catch {
      /* respuesta sin JSON */
    }
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
  onCancel?: () => void
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
