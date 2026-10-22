// Personajes y estado del hardware (micro:bit / RAG).
import { conTope, fetchKira } from './http';
import type { EstadoMicrobit, EstadoRag, PersonajeInfo } from './tipos';

export async function obtenerPersonajes(): Promise<PersonajeInfo[]> {
  const r = await fetchKira('/api/personajes');
  if (!r.ok) throw new Error('No se pudieron cargar los personajes');
  const data = await r.json();
  return data.personajes;
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
