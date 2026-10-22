// Utilidades de tiempo y autonomía de la placa.
import { fetchKira } from './http';
import type { EventoPlaca } from './tipos';

export function horaActual(): string {
  return new Date().toLocaleTimeString('es-AR', { hour: '2-digit', minute: '2-digit' });
}

// --------------------------------------------------------------------------
// AUTONOMIA: lo que la placa avisó por su cuenta (EVENTO:..., JUEGO:...)
// desde el número de secuencia 'desde' (-1 = solo saber en qué número va).
// --------------------------------------------------------------------------
export async function obtenerEventos(
  desde: number
): Promise<{ seq: number; eventos: EventoPlaca[] }> {
  const r = await fetchKira(`/api/eventos?desde=${desde}`);
  if (!r.ok) throw new Error(`eventos ${r.status}`);
  return r.json();
}
