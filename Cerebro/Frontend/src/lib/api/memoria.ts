// Memoria viva: lo que Kira recuerda, su diario y las preferencias de
// privacidad que viajan con cada turno.
import { fetchKira } from './http';

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
    throw new Error(
      `No se pudo olvidar la memoria (${r.status})${detalle ? `: ${detalle.slice(0, 160)}` : ''}`
    );
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
    throw new Error(
      `No se pudo borrar la memoria (${r.status})${detalle ? `: ${detalle.slice(0, 160)}` : ''}`
    );
  }
  const datos = await r.json().catch(() => ({}));
  return Number(datos.borradas ?? 0);
}
