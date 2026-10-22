// Tipos compartidos del contrato con el backend.

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

/** De dónde salió una cosa que dijo Kira. Cinco clases, y la última es la
 *  más importante: 'criada' significa que NO consultó nada y el número sale de
 *  su conocimiento de entrenamiento, que no tiene fuente. Mostrar eso es lo
 *  que hace auditable la diferencia entre "afirmó" y "consultó". */
export type TipoRastro =
  | 'web'
  | 'sensor'
  | 'calculo'
  | 'memoria'
  | 'estado'
  | 'sistema'
  | 'accion'
  | 'criada'
  | 'otro';

export interface RastroItem {
  tipo: TipoRastro;
  herramienta: string;
  etiqueta: string;
  detalle: string;
  valor: string;
  ok: boolean;
}

export interface MensajeChat {
  id: string;
  rol: 'usuario' | 'kira';
  contenido: string;
  emotion?: string;
  audio_url?: string | null;
  audio_urls?: string[] | null;
  fuentes?: FuenteWeb[] | null;
  /** De dónde salió lo que dijo en este turno. Se muestra plegado bajo el
   *  mensaje y además viaja de vuelta al backend en la historia, para que
   *  Kira sepa citar sus propias fuentes si le repreguntan. */
  rastro?: RastroItem[] | null;
  /** FOTO que mandó el usuario (data URL JPEG reducido, ~200 KB). Se muestra
   * en su burbuja y se guarda con el chat: es lo que él mismo mandó. */
  imagen?: string | null;
  hora: string;
}

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

/** Un turno del historial que viaja al backend. `rastro` es lo que le permite
 *  a Kira citar sus propias fuentes cuando le repreguntan de dónde lo sacó. */
export interface TurnoHistorial {
  rol: string;
  contenido: string;
  rastro?: RastroItem[] | null;
  emotion?: string | null;
}

// Resultado final de un chat STREAMING
export interface ResultadoStream {
  emotion: string;
  message: string;
  tts_url: string | null;
  tts_urls?: string[] | null; // TTS POR FRASES: la cola completa en orden
  personaje: string;
  fuentes?: FuenteWeb[] | null;
  rastro?: RastroItem[] | null;
}

// Resultado de la ESCUCHA MANUAL (al pulsar A por segunda vez)
export interface ResultadoEscucha {
  transcripcion: string | null;
  archivo: string | null;
  duracion_s: number;
}

export interface EventoPlaca {
  seq: number;
  linea: string;
  t: number;
}
