// Proveedores de IA: listarlos con su estado y elegir cuál atiende.
// Se aplica en caliente en el backend (no hace falta reiniciar ni recargar).
import { fetchKira } from './http';

export interface ProveedorIA {
  nombre: string;
  modelo: string;
  base_url: string;
  /** Tiene clave configurada. Sin clave no puede atender nunca. */
  tiene_key: boolean;
  /** Puede atender ahora: tiene clave y no está enfriando. */
  disponible: boolean;
  /** Segundos que le quedan de "castigo" tras un 429/402. 0 = libre. */
  enfriando_s: number;
  /** Posición en la cadena automática (0 = primero). */
  orden: number;
  /** Es el elegido a mano en Ajustes. */
  elegido: boolean;
}

export interface EstadoProveedores {
  proveedores: ProveedorIA[];
  /** true = nadie elegido a mano; manda el orden de la config. */
  automatico: boolean;
  /** Quién atendería AHORA si se manda un mensaje. */
  atendiendo: string | null;
}

export async function listarProveedores(): Promise<EstadoProveedores> {
  const r = await fetchKira('/api/proveedores');
  if (!r.ok) throw new Error(`proveedores ${r.status}`);
  return r.json();
}

/** nombre vacío = volver al automático. */
export async function elegirProveedor(nombre: string): Promise<void> {
  const r = await fetchKira('/api/proveedores', {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ nombre }),
  });
  if (!r.ok) {
    const detalle = await r.json().catch(() => null);
    throw new Error(detalle?.detail ?? `no se pudo cambiar (${r.status})`);
  }
}
