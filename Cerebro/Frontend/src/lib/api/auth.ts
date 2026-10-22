// Auth: sesión, registro, login y logout.
import { fetchKira } from './http';
import type { EstadoAuth, RespuestaAuth } from './tipos';

export async function obtenerSesion(): Promise<EstadoAuth> {
  const r = await fetchKira('/api/auth/me');
  if (!r.ok) throw new Error(`No se pudo verificar la sesión (${r.status})`);
  return r.json();
}

export async function registrarUsuario(
  username: string,
  password: string
): Promise<RespuestaAuth> {
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
