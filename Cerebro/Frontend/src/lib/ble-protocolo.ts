// Protocolo y helpers del puente BLE (Web Bluetooth + Nordic UART del micro:bit).
// Sin React: se puede testear y reutilizar.

export const NUS_SERVICIO = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
export const NUS_ESCRIBIR = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';
export const NUS_LEER = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';

// Cada cuánto late el puesto (el server lo libera a los 120s sin latido).
export const LATIDO_MS = 10_000;
// RECLAMO del puesto: backoff exponencial (5s, 10s, 20s, 40s, 60s tope) y un
// tope de intentos. Antes era un fijo de 15s: si algo fallaba siempre (p. ej.
// getDevices no existe en Chrome Android), el cliente martillaba el server
// para siempre con un registro cada 15s.
export const RECLAMO_BASE_MS = 5_000;
export const RECLAMO_TOPE_MS = 60_000;
export const RECLAMO_MAX_FALLOS = 6;
// ANTI-TORMENTA: nunca más de un registro en vuelo, y como mínimo esto entre
// dos registros. Un cliente viejo (PWA con bundle cacheado) podía pedir el
// puesto en loop: el server lo frena con 429, pero mejor no llegar.
export const REGISTRO_MIN_MS = 4_000;

// QUIÉN SOY: identificador de ESTA copia de la app (id al azar + hash del
// bundle) que va en los diag. Con dos copias abiertas a la vez (la PWA
// instalada + una pestaña de Chrome) el log se vuelve ilegible — así se sabe
// de un vistazo cuál habla — y de paso delata si una corre JS viejo cacheado.
export const YO = (() => {
  const m = import.meta.url.match(/index-([A-Za-z0-9_-]+)\.js/);
  return `${Math.random().toString(36).slice(2, 6)}@${m ? m[1] : 'dev'}`;
})();

export type EstadoPuenteBle =
  | 'desconectado'
  | 'conectando'
  | 'conectado'
  | 'reconectando'
  | 'rechazado';

export type Estado = EstadoPuenteBle;

export interface Caracteristicas {
  escribir: BluetoothRemoteGATTCharacteristic;
  leer: BluetoothRemoteGATTCharacteristic;
}

/** Lo mínimo que usamos del WakeLockSentinel (tipado local para no depender
 * de que el TS del proyecto ya conozca la API). */
export interface LockPantalla {
  release(): Promise<void>;
  addEventListener?: (tipo: string, cb: () => void) => void;
}

/** gatt.connect() puede colgarse si la placa está apagada (issue #640
 * del spec): carrera con timeout para no quedar zombi. */
export async function conectarConTope(
  server: BluetoothRemoteGATTServer,
  topeMs = 20000
): Promise<BluetoothRemoteGATTServer> {
  return Promise.race<BluetoothRemoteGATTServer>([
    server.connect(),
    new Promise<BluetoothRemoteGATTServer>((_, reject) =>
      setTimeout(() => reject(new Error('timeout conectando GATT')), topeMs)
    ),
  ]);
}

/** writeValue CON TOPE. Tras reconectar, Chrome puede dejar el write colgado
 * PARA SIEMPRE sin tirar excepción ("the async call which is writing ... never
 * seems to return", Web Bluetooth en Android). Sin tope, el bucle de comandos
 * se muere mudo; con tope, lo contamos como fallo y el comando vuelve a la
 * cola (y la guardia del enlace se encarga de re-armar). */
export async function escribirConTope(
  ch: BluetoothRemoteGATTCharacteristic,
  datos: BufferSource,
  topeMs = 5000
): Promise<void> {
  return Promise.race<void>([
    ch.writeValue(datos),
    new Promise<void>((_, reject) =>
      setTimeout(() => reject(new Error('timeout escribiendo por BLE')), topeMs)
    ),
  ]);
}

/** ESCRITURA RÁPIDA: las ráfagas chicas van SIN respuesta (ATT write
 * command): no esperan el ida y vuelta de confirmación por cada paquete de
 * 20 bytes, que era la mayor parte del retraso de la boca. Es seguro hasta 60
 * bytes porque el buffer RX de la placa es de 96 (ver BLE_RX_BUF en
 * BleUart.cpp) y la placa lo vacía cada 10 ms. Lo que sea más grande, o si
 * el navegador/la placa no lo soportan, va por el camino de siempre (con
 * respuesta: control de flujo real). */
const RAFAGA_SIN_RESPUESTA = 60;
let sinRespuestaRoto = false;

export async function escribirLineas(
  ch: BluetoothRemoteGATTCharacteristic,
  bytes: Uint8Array
): Promise<void> {
  const puedeRapido =
    !sinRespuestaRoto &&
    bytes.length <= RAFAGA_SIN_RESPUESTA &&
    typeof ch.writeValueWithoutResponse === 'function' &&
    ch.properties?.writeWithoutResponse === true;
  if (puedeRapido) {
    try {
      for (let i = 0; i < bytes.length; i += 20) {
        await Promise.race<void>([
          ch.writeValueWithoutResponse(bytes.slice(i, i + 20)),
          new Promise<void>((_, reject) =>
            setTimeout(() => reject(new Error('timeout escribiendo por BLE')), 3000)
          ),
        ]);
      }
      return;
    } catch (e) {
      // NotSupported/NotAllowed: este enlace no lo banca -> nunca más lo
      // intenta y sigue con respuesta. Un timeout se propaga (re-encola).
      const nombre = e instanceof Error ? e.name : '';
      if (nombre !== 'NotSupportedError' && nombre !== 'NotAllowedError') throw e;
      sinRespuestaRoto = true;
    }
  }
  for (let i = 0; i < bytes.length; i += 20) {
    await escribirConTope(ch, bytes.slice(i, i + 20));
  }
}

/** Backoff exponencial oficial de Chrome (sample Automatic Reconnect). */
export function backoff(intento: number): number {
  return Math.min(2000 * Math.pow(2, intento), 8000);
}

export const dormir = (ms: number) => new Promise((r) => setTimeout(r, ms));
