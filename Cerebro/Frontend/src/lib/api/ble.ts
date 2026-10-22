// Relay BLE (modo feria): este navegador es el cable entre backend y placa.
// Todos los llamados llevan el TOKEN de dueño del puente: el primero que
// registra gana; 409 = otro dispositivo lo tiene.
import { conTope } from './http';

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
  } catch {
    /* da igual */
  }
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
  } catch {
    /* la proxima tanda lo lleva */
  }
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
  } catch {
    /* si tampoco llega, se pierden: ni modo */
  }
}

export async function bleReportarError(mensaje: string): Promise<void> {
  try {
    await conTope('/api/ble/error', 8000, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mensaje }),
    });
  } catch {
    /* ni modo */
  }
}
