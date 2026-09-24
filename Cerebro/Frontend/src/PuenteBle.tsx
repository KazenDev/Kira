/**
 * PuenteBle.tsx - EL CABLE INVISIBLE (modo feria) 🔌📡
 *
 * Sin PC en el stand, el navegador ES el cable entre el backend (VPS) y
 * la placa: toma los comandos del servidor (long-poll /api/ble/tx) y los
 * escribe por Web Bluetooth; las lineas que la placa responde vuelven al
 * servidor (/api/ble/rx).
 *
 * ARQUITECTURA RESISTENTE A FERIAS (patrones oficiales de Chrome):
 *  - TOKEN DE DUEÑO: un solo navegador puede ser el puente (el primero
 *    gana); el long-poll es el LATIDO que renueva el puesto (120s gracia).
 *  - AUTO-RECONEXIÓN BLE: si el enlace se cae, backoff exponencial
 *    (2s→4s→8s) y, si la placa sigue muerta, watchAdvertisements() espera
 *    a que la placa GRITE "estoy aquí" para reconectar sin colgarse.
 *  - GATT SIEMPRE FRESCO: tras cada reconexión se re-descubren servicios
 *    y características (los viejos quedan invalidados, dice el CAUTION).
 *  - MEMORIA: getDevices() al cargar → si ya habías emparejado la placa,
 *    se reconecta SOLA sin abrir el selector.
 *
 * Nordic UART estilo micro:bit (¡OJO, invertidos vs el NUS genérico!):
 *   6e400002 = TX (la PLACA transmite: aquí nos suscribimos)
 *   6e400003 = RX (la PLACA escucha: aquí le escribimos)
 *
 * --- 17-sep: por qué se "desconectaba solo a los ~10s" (arreglado) ---
 * El latido del puente (long-poll + ping) vivía DENTRO de efectos atados al
 * estado de la UI (`estado === 'conectado'`). Cualquier transición momentánea
 * (un toque al botón, un reintento, el selector abierto) desmontaba el efecto
 * y MATA el latido en silencio; el server liberaba el puente 60s después del
 * último latido y todo quedaba mudo sin un solo log. Además, perder el duelo
 * de pestañas o recibir un 409 DESCONECTABA el enlace BLE: en Chrome el enlace
 * es uno solo por placa y por navegador, así que la pestaña perdedora tumbaba
 * la conexión de la ganadora.
 * Ahora: los latidos NO dependen del estado, el puesto se pide ANTES de tocar
 * el GATT, nadie desconecta el enlace por temas de propiedad, y cada fallo se
 * reporta al server (`diag: ...`) porque en el cel no hay consola.
 */
import { useCallback, useEffect, useRef, useState } from 'react';
import { Bluetooth, BluetoothConnected, RefreshCw } from 'lucide-react';
import {
  bleDevolver,
  bleMarcarConectado,
  bleMarcarDesconectado,
  blePing,
  bleReportarError,
  bleReportarRx,
  bleTomarTx,
} from './api';
import { debeRearmarEnlace, msDeSilencio } from './enlace-ble';
// uuid con fallback: crypto.randomUUID() NO existe en http:// (contexto no
// seguro) y rompia el arranque de la app entera (el token del puente).
import { uuid } from './ui-utils';

const NUS_SERVICIO = '6e400001-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_ESCRIBIR = '6e400003-b5a3-f393-e0a9-e50e24dcca9e';
const NUS_LEER = '6e400002-b5a3-f393-e0a9-e50e24dcca9e';

// Cada cuánto late el puesto (el server lo libera a los 120s sin latido).
const LATIDO_MS = 10_000;
// RECLAMO del puesto: backoff exponencial (5s, 10s, 20s, 40s, 60s tope) y un
// tope de intentos. Antes era un fijo de 15s: si algo fallaba siempre (p. ej.
// getDevices no existe en Chrome Android), el cliente martillaba el server
// para siempre con un registro cada 15s.
const RECLAMO_BASE_MS = 5_000;
const RECLAMO_TOPE_MS = 60_000;
const RECLAMO_MAX_FALLOS = 6;
// ANTI-TORMENTA: nunca más de un registro en vuelo, y como mínimo esto entre
// dos registros. Un cliente viejo (PWA con bundle cacheado) podía pedir el
// puesto en loop: el server lo frena con 429, pero mejor no llegar.
const REGISTRO_MIN_MS = 4_000;

// QUIÉN SOY: identificador de ESTA copia de la app (id al azar + hash del
// bundle) que va en los diag. Con dos copias abiertas a la vez (la PWA
// instalada + una pestaña de Chrome) el log se vuelve ilegible — así se sabe
// de un vistazo cuál habla — y de paso delata si una corre JS viejo cacheado.
const YO = (() => {
  const m = import.meta.url.match(/index-([A-Za-z0-9_-]+)\.js/);
  return `${Math.random().toString(36).slice(2, 6)}@${m ? m[1] : 'dev'}`;
})();

export type EstadoPuenteBle = 'desconectado' | 'conectando' | 'conectado' | 'reconectando' | 'rechazado';
type Estado = EstadoPuenteBle;

interface Caracteristicas {
  escribir: BluetoothRemoteGATTCharacteristic;
  leer: BluetoothRemoteGATTCharacteristic;
}

/** Lo mínimo que usamos del WakeLockSentinel (tipado local para no depender
 * de que el TS del proyecto ya conozca la API). */
interface LockPantalla {
  release(): Promise<void>;
  addEventListener?: (tipo: string, cb: () => void) => void;
}

/** gatt.connect() puede colgarse si la placa está apagada (issue #640
 * del spec): carrera con timeout para no quedar zombi. */
async function conectarConTope(server: BluetoothRemoteGATTServer, topeMs = 20000) {
  return Promise.race<BluetoothRemoteGATTServer>([
    server.connect(),
    new Promise<BluetoothRemoteGATTServer>((_, reject) =>
      setTimeout(() => reject(new Error('timeout conectando GATT')), topeMs)
    ),
  ]);
}

/** writeValue CON TOPE. Tras reconectar, Chrome puede dejar el write colgado
 * PARA SIEMPRE sin tirar excepcion ("the async call which is writing ... never
 * seems to return", Web Bluetooth en Android). Sin tope, el bucle de comandos
 * se muere mudo; con tope, lo contamos como fallo y el comando vuelve a la
 * cola (y la guardia del enlace se encarga de re-armar). */
async function escribirConTope(
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

/** Backoff exponencial oficial de Chrome (sample Automatic Reconnect). */
function backoff(intento: number) {
  return Math.min(2000 * Math.pow(2, intento), 8000);
}

const dormir = (ms: number) => new Promise((r) => setTimeout(r, ms));

interface Props {
  compacto?: boolean; // sólo icono
  oculto?: boolean; // controlador persistente sin botón propio
  onEstado?: (estado: EstadoPuenteBle) => void;
}

export default function PuenteBle({ compacto = false, oculto = false, onEstado }: Props) {
  const [estado, setEstado] = useState<Estado>('desconectado');
  const chars = useRef<Caracteristicas | null>(null);
  const dispositivo = useRef<BluetoothDevice | null>(null);
  const buferEntrada = useRef('');
  const bandejaSalida = useRef<string[]>([]);
  // a qué dispositivo ya le colgamos el listener de 'gattserverdisconnected'
  // (sin esto, cada intento agregaba OTRO listener y una caída disparaba N
  // reconexiones a la vez)
  const deviceEscuchado = useRef<BluetoothDevice | null>(null);
  const cierreManual = useRef(false);
  const generacion = useRef(0); // invalida loops viejos tras reconectar
  const estadoRef = useRef<Estado>('desconectado');

  // esDueno: EL SERVER nos aceptó como puente. Solo con esto tiene sentido
  // mandar comandos por aire (y es lo que mantiene vivo el latido).
  const esDueno = useRef(false);
  // reconectar() se define más abajo pero el aviso de caída del GATT se
  // cuelga antes: este ref mantiene la versión fresca sin dependencias raras.
  const reconectarRef = useRef<(miGen: number) => void>(() => {});
  // rearmarEnlace() vive mas abajo (necesita armarGATT/registrar); el bucle de
  // comandos lo llama por este ref para no depender del orden del archivo.
  const rearmarRef = useRef<(motivo: string) => void>(() => {});
  // quierePuente: este dispositivo quiere ser el cable (lo pone el botón o el
  // auto-connect al cargar). NO es lo mismo que "está conectado".
  const quierePuente = useRef(false);

  /** Reporte al server: en el cel no hay consola, asi que el log del VPS es
   * nuestro depurador. Los `diag:` son transiciones normales (ruido util). */
  const reportar = useCallback((mensaje: string) => {
    bleReportarError(mensaje);
  }, []);

  // ---------- WAKE LOCK ----------
  // Mientras el puente está activo, la pantalla del cel NO se bloquea. Sin
  // esto Chrome congela el JS con la pantalla apagada (los comandos se
  // acumulan y llegan en ráfaga al despertar: la placa hace "la película
  // loca"). Se pide: al conectar, al volver a la pestaña, y cada vez que el
  // sistema lo suelta (evento 'release': pasa al cambiar de app).
  const wakeLock = useRef<LockPantalla | null>(null);
  const pedirWakeLock = useCallback(async () => {
    if (wakeLock.current) return;
    try {
      const nav = navigator as Navigator & {
        wakeLock?: { request(t: string): Promise<LockPantalla> };
      };
      if (!nav.wakeLock) return; // navegador sin soporte: no es un error
      const lock = await nav.wakeLock.request('screen');
      wakeLock.current = lock;
      lock.addEventListener?.('release', () => {
        wakeLock.current = null;
        // el sistema lo soltó (cambio de app, batería baja): lo recuperamos
        // cuando la página vuelva a estar visible
        if (document.visibilityState === 'visible' && (esDueno.current || quierePuente.current)) {
          setTimeout(() => pedirWakeLock(), 500);
        }
      });
    } catch (e) {
      // OJO: si esto falla, la pantalla se apaga → Chrome congela el JS →
      // adiós latido. Antes era un catch mudo: ahora queda en el log.
      reportar(`wake-lock falló: ${e instanceof Error ? e.message : String(e)}`);
    }
  }, [reportar]);
  const soltarWakeLock = useCallback(() => {
    try {
      wakeLock.current?.release();
    } catch { /* ya estaba suelto */ }
    wakeLock.current = null;
  }, []);
  useEffect(() => {
    if (estado === 'conectado' || estado === 'reconectando' || quierePuente.current) pedirWakeLock();
    if (estado === 'desconectado' || estado === 'rechazado') soltarWakeLock();
  }, [estado, pedirWakeLock, soltarWakeLock]);

  // TOKEN de dueño: por PESTAÑA (dos tabs = dos tokens, uno gana)
  const [token] = useState(() => {
    let t = sessionStorage.getItem('puente-token');
    if (!t) {
      t = uuid();
      sessionStorage.setItem('puente-token', t);
    }
    return t;
  });

  // ---------- LIDER ÚNICO ENTRE PESTAÑAS (un solo puente) ----------
  // Dos tabs = dos tokens = comandos al vacío para el perdedor. Canal
  // local: quien quiere el puente anuncia su token cada 2s; gana el MENOR
  // (determinista). El perdedor se retira del SERVER (no del enlace BLE:
  // el enlace es uno solo por navegador y desconectarlo rompía al ganador).
  const canalLider = useRef<BroadcastChannel | null>(null);
  const ultimoPingAjeno = useRef(0);
  const tokenAjeno = useRef('');
  const timerReclamo = useRef<ReturnType<typeof setTimeout> | null>(null);

  // fallos de reclamo seguidos (para el backoff) y si ya nos rendimos
  const fallosReclamo = useRef(0);
  // promesa de registro en vuelo (una sola a la vez) + hora del último
  const registrando = useRef<Promise<boolean> | null>(null);
  const ultimoRegistroMs = useRef(0);

  // GUARDIA DEL ENLACE (17-sep tarde): el log del server mostro 28 comandos
  // mandados y UNA sola respuesta. El enlace de VUELTA estaba mudo y la app no
  // se enteraba (los writeValue resuelven igual y Chrome no avisa: el unico
  // evento prometido es gattserverdisconnected). Medimos el silencio con los
  // comandos que mandamos y, si la placa no contesta, re-armamos GATT +
  // notificaciones (en Android el CCCD lo maneja la app: otra pestaña puede
  // haberlo apagado).
  const ultimaRespuestaMs = useRef(0);     // ultima linea que llego de la placa
  const ultimaEscrituraMs = useRef(0);     // ultimo comando que le mandamos
  const comandosSinRespuesta = useRef(0);  // escritos desde esa respuesta
  const ultimoRearmeMs = useRef(0);        // cooldown del re-arme
  const rearmando = useRef(false);         // un re-arme a la vez

  const programarReclamo = useCallback(() => {
    if (timerReclamo.current) clearTimeout(timerReclamo.current);
    const fallos = ++fallosReclamo.current;
    if (fallos > RECLAMO_MAX_FALLOS) {
      // nos rendimos con el reintento automático: esperamos al usuario (el
      // botón) o a que vuelva a la pestaña. Antes esto era un loop eterno.
      return;
    }
    const espera = Math.min(RECLAMO_BASE_MS * Math.pow(2, fallos - 1), RECLAMO_TOPE_MS);
    timerReclamo.current = setTimeout(() => {
      timerReclamo.current = null;
      if (quierePuente.current && !esDueno.current) intentarPuente();
    }, espera);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  /** Perder el puesto: se suelta el SERVER y se deja de mandar comandos.
   * NO se desconecta el enlace BLE (es compartido con la pestaña ganadora). */
  const perderPuesto = useCallback((motivo: string) => {
    if (!esDueno.current) return;
    esDueno.current = false;
    bleMarcarDesconectado(token);
    setEstado('rechazado');
    reportar(`diag: solté el puesto (${motivo})`);
  }, [token, reportar]);

  /** Retirarse por el duelo de pestañas: suelta el server y espera su turno. */
  const retirarse = useCallback((motivo: string) => {
    cierreManual.current = false; // no es manual: se puede reintentar
    perderPuesto(`duelo: ${motivo}`);
    programarReclamo();
  }, [perderPuesto, programarReclamo]);

  useEffect(() => {
    let canal: BroadcastChannel | null = null;
    try {
      canal = new BroadcastChannel('kira-puente');
    } catch { canal = null; }
    canalLider.current = canal;
    if (!canal) return () => {};
    canal.onmessage = (ev: MessageEvent) => {
      const otro = String(ev.data?.token ?? '');
      if (!otro || otro === token) return;
      ultimoPingAjeno.current = Date.now();
      tokenAjeno.current = otro;
      // duelo: el token MENOR gana; si pierdo y TENGO el puesto, lo suelto
      if (otro < token && esDueno.current) {
        retirarse('otra pestaña tiene prioridad');
      }
    };
    return () => {
      canal.close();
      canalLider.current = null;
    };
  }, [token, retirarse]);

  // ping de presencia mientras se QUIERE el puente (no solo teniéndolo:
  // así el duelo converge aunque el server me haya dado 409)
  useEffect(() => {
    const t = setInterval(() => {
      if (quierePuente.current) {
        try {
          canalLider.current?.postMessage({ token });
        } catch { /* canal caído: ni modo */ }
      }
    }, 2000);
    return () => clearInterval(t);
  }, [token]);

  /** Cuelga el aviso de caída UNA sola vez por dispositivo (y por sesión). */
  const vigilarCaida = useCallback((device: BluetoothDevice) => {
    if (deviceEscuchado.current === device) return;
    deviceEscuchado.current = device;
    const miGen = generacion.current;
    device.addEventListener('gattserverdisconnected', () => {
      if (cierreManual.current || miGen !== generacion.current) return;
      // si el enlace se cayó, dejamos de ser el cable hasta reconectar
      reportar('diag: la placa se desconectó (evento del navegador)');
      reconectarRef.current(miGen);
    });
  }, [reportar]);

  // ---------- suscripción a las notificaciones de la placa ----------
  const escucharPlaca = useCallback((leer: BluetoothRemoteGATTCharacteristic) => {
    leer.addEventListener('characteristicvaluechanged', (ev: Event) => {
      const valor = (ev.target as BluetoothRemoteGATTCharacteristic).value;
      if (!valor) return;
      buferEntrada.current += new TextDecoder().decode(valor);
      let idx: number;
      while ((idx = buferEntrada.current.indexOf('\n')) >= 0) {
        const linea = buferEntrada.current.slice(0, idx).trim();
        buferEntrada.current = buferEntrada.current.slice(idx + 1);
        if (linea) {
          bandejaSalida.current.push(linea);
          // LA PLACA HABLO: el camino de vuelta esta vivo (y el firmware nuevo
          // manda un HELLO al detectar el enlace, que tambien cae aqui).
          ultimaRespuestaMs.current = Date.now();
          comandosSinRespuesta.current = 0;
        }
      }
      if (buferEntrada.current.length > 512) buferEntrada.current = '';
    });
  }, []);

  // ---------- secuencia GATT COMPLETA (siempre fresca) ----------
  const armarGATT = useCallback(async (): Promise<Caracteristicas> => {
    const device = dispositivo.current!;
    const server = await conectarConTope(device.gatt!);
    // CAUTION de Chrome: tras cada reconexión los atributos GATT quedan
    // invalidados -> re-descubrir TODO, siempre.
    const servicio = await server.getPrimaryService(NUS_SERVICIO);
    const escribir = await servicio.getCharacteristic(NUS_ESCRIBIR);
    const leer = await servicio.getCharacteristic(NUS_LEER);
    await leer.startNotifications();
    escucharPlaca(leer);
    // GATT fresco: el reloj de la guardia del enlace arranca de cero (vale
    // para el connect inicial, la reconexion y el re-arme).
    comandosSinRespuesta.current = 0;
    ultimaRespuestaMs.current = 0;
    ultimaEscrituraMs.current = 0;
    return { escribir, leer };
  }, [escucharPlaca]);

  // ---------- registro como dueño del puente en el server ----------
  const registrar = useCallback(async (): Promise<boolean> => {
    // ANTI-TORMENTA (bug del 17-sep: el celu pidió el puesto 834 veces en 10
    // minutos): una sola llamada en vuelo, y nunca dos registros seguidos más
    // rápido que REGISTRO_MIN_MS. Si ya somos dueños y venimos de registrar,
    // no hace falta volver a pedirlo.
    if (registrando.current) return registrando.current;
    if (esDueno.current && Date.now() - ultimoRegistroMs.current < REGISTRO_MIN_MS) return true;
    const vuelo = (async () => {
      try {
        await bleMarcarConectado(token);
        esDueno.current = true;
        ultimoRegistroMs.current = Date.now();
        fallosReclamo.current = 0; // salió bien: el backoff arranca de cero
        return true;
      } catch (e) {
        // 409 = otro dispositivo tiene el puente. OJO: NO desconectamos el
        // enlace BLE acá (es compartido: hacerlo tumbaba al dueño real).
        esDueno.current = false;
        setEstado('rechazado');
        reportar(`registro: ${e instanceof Error ? e.message : String(e)}`);
        return false;
      } finally {
        registrando.current = null;
      }
    })();
    registrando.current = vuelo;
    return vuelo;
  }, [token, reportar]);

  // ---------- AUTO-RECONEXIÓN (backoff + watchAdvertisements) ----------
  const reconectar = useCallback(
    async (miGen: number) => {
      setEstado('reconectando');
      // reportar el motivo al server: sin esto el VPS no sabe POR QUE cayo
      reportar(`diag: BLE caído, reconectando (gen ${miGen})`);
      // ronda 1: backoff exponencial (2s, 4s, 8s)
      for (let intento = 0; intento < 3 && miGen === generacion.current; intento++) {
        if (intento > 0) await dormir(backoff(intento - 1));
        if (miGen !== generacion.current) return;
        try {
          chars.current = await armarGATT();
          if (await registrar()) {
            setEstado('conectado');
            reportar('diag: reconectado al toque');
            return;
          }
          return; // rechazado: otro es el dueño (y no tocamos el enlace)
        } catch {
          /* seguimos intentando */
        }
      }
      if (miGen !== generacion.current) return;
      // ronda 2: esperar a que la PLACA grite "estoy aquí" (máx 60s)
      // y de yapa un reintento lento periódico si todo falla
      try {
        const device = dispositivo.current!;
        await new Promise<void>((resolver) => {
          const alAnuncio = () => {
            device.removeEventListener('advertisementreceived', alAnuncio);
            device.watchAdvertisements?.().catch(() => {});
            resolver();
          };
          device.addEventListener('advertisementreceived', alAnuncio);
          device.watchAdvertisements?.().catch(() => resolver());
          setTimeout(resolver, 60000); // tope: reintentar igual
        });
        if (miGen !== generacion.current) return;
        chars.current = await armarGATT();
        if (await registrar()) {
          setEstado('conectado');
          reportar('diag: reconectado tras el anuncio de la placa');
        }
      } catch {
        // ni modo: el reclamo periódico lo reintenta en silencio
        setEstado('desconectado');
        programarReclamo();
      }
    },
    [armarGATT, registrar, programarReclamo, reportar]
  );
  // el aviso de caída del GATT (colgado antes) siempre llama a la última
  // versión de reconectar()
  useEffect(() => {
    reconectarRef.current = reconectar;
  }, [reconectar]);

  // ---------- intento SILENCIOSO (sin selector: usa el permiso ya dado) ----
  // Lo usan el auto-connect al cargar, el reclamo periódico y la reconexión.
  // OJO: pide el PUESTO antes de tocar el GATT. Conectar sin ser el dueño
  // pateaba el enlace de la pestaña que si era dueña (en Chrome hay un solo
  // enlace por placa por navegador).
  const intentarPuente = useCallback(async () => {
    if (!quierePuente.current) return;
    // si otra pestaña tiene prioridad (token menor y latiendo hace <4s), ni
    // intentar: sería patalear contra el dueño
    if (Date.now() - ultimoPingAjeno.current < 4000 && tokenAjeno.current && tokenAjeno.current < token) {
      programarReclamo();
      return;
    }
    try {
      // getDevices() NO EXISTE en Chrome Android (es de escritorio / detras de
      // flag): sin el no hay forma de "recordar" la placa entre recargas. Si
      // falta, solo sirve la referencia que ya tengamos EN MEMORIA (esta
      // pestaña) — el patron que recomienda Chrome: reconectar con el
      // BluetoothDevice cacheado, sin volver a abrir el selector.
      let placa: BluetoothDevice | null = dispositivo.current;
      const hayGetDevices = typeof navigator.bluetooth?.getDevices === 'function';
      if (!placa && hayGetDevices) {
        const conocidos = await navigator.bluetooth.getDevices();
        placa = conocidos.find((d) => (d.name ?? '').startsWith('BBC micro:bit')) ?? null;
      }
      if (!placa) {
        setEstado('desconectado');
        // Sin placa recordada NO hay reintento que sirva: en Android hay que
        // abrir el selector con un toque del usuario. Antes esto programaba un
        // reclamo cada 15s PARA SIEMPRE (la tormenta de getDevices del celu).
        return;
      }
      // 1) ¿el puesto es nuestro? Si no, no tocamos el GATT ajeno.
      if (!(await registrar())) {
        programarReclamo();
        return;
      }
      // 2) recién ahora el enlace
      dispositivo.current = placa;
      setEstado('conectando');
      const miGen = ++generacion.current;
      vigilarCaida(placa);
      chars.current = await armarGATT();
      if (miGen !== generacion.current) return;
      setEstado('conectado');
    } catch (e) {
      // antes era mudo: por eso "se desconectaba" sin ninguna pista
      reportar(`diag: intento silencioso falló (${e instanceof Error ? e.message : String(e)})`);
      setEstado('desconectado');
      programarReclamo();
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [registrar, programarReclamo, vigilarCaida, token, reportar]);

  // ---------- conexión manual (el botón: abre el selector) ----------
  const conectar = useCallback(async () => {
    if (estado === 'conectando' || estado === 'reconectando') return;
    quierePuente.current = true;
    setEstado('conectando');
    cierreManual.current = false;
    fallosReclamo.current = 0; // el usuario pidió: sin backoff, intento ya
    generacion.current++;
    const miGen = generacion.current;
    try {
      // El selector va PRIMERO: requestDevice() necesita el gesto del usuario
      // (un await largo antes puede consumir la activación transitoria).
      const device = await navigator.bluetooth.requestDevice({
        filters: [{ namePrefix: 'BBC micro:bit' }],
        optionalServices: [NUS_SERVICIO],
      });
      dispositivo.current = device;
      vigilarCaida(device); // ¡resucitá solo!
      chars.current = await armarGATT();
      if (miGen !== generacion.current) return;
      if (await registrar()) setEstado('conectado');
      else {
        // otro es el dueño: NO desconectamos (rompería su enlace), solo
        // dejamos de mandar y esperamos nuestro turno
        programarReclamo();
      }
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      reportar(`conexion: ${msg}`);
      setEstado('desconectado');
      programarReclamo();
    }
  }, [estado, armarGATT, registrar, vigilarCaida, programarReclamo, reportar]);

  // ---------- MEMORIA: al cargar, placa ya emparejada -> reconexión muda ----
  useEffect(() => {
    quierePuente.current = true; // esta pestaña quiere el puente al abrir
    intentarPuente();
    return () => {
      quierePuente.current = false;
      if (timerReclamo.current) clearTimeout(timerReclamo.current);
    };
    // solo al montar: la magia de la reconexión silenciosa
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, []);

  // el estado ya se refleja en estadoRef (declarado con los demás refs)
  useEffect(() => {
    estadoRef.current = estado;
  }, [estado]);

  // ---------- VOLVER A LA PESTAÑA: recuperar puesto y latido ----------
  // Con la pantalla apagada Chrome congela el JS: el server libera el puente
  // a los 120s. Al volver, re-registramos AL INSTANTE y re-armamos si hace
  // falta, en vez de esperar al reclamo de 15s. Los comandos que quedaron
  // pendientes los entrega el long-poll en cuanto volvemos (ya no se tiran).
  useEffect(() => {
    const alVolver = () => {
      if (document.visibilityState !== 'visible') return;
      if (!quierePuente.current) return;
      pedirWakeLock();
      if (!esDueno.current) {
        reportar('diag: volví a la pestaña, retomo el puente');
        fallosReclamo.current = 0; // el usuario volvió: que el backoff arranque de cero
        intentarPuente();
      } else if (!chars.current) {
        reportar('diag: dueño sin enlace al volver, re-armo GATT');
        intentarPuente();
      }
    };
    document.addEventListener('visibilitychange', alVolver);
    return () => document.removeEventListener('visibilitychange', alVolver);
  }, [pedirWakeLock, intentarPuente, reportar]);

  // ---------- desconexión manual ----------
  const desconectar = useCallback(() => {
    quierePuente.current = false; // ya no quiero el puente: sin pings ni reclamos
    if (timerReclamo.current) clearTimeout(timerReclamo.current);
    cierreManual.current = true;
    generacion.current++;
    chars.current = null;
    esDueno.current = false;
    setEstado('desconectado');
    soltarWakeLock();
    bleMarcarDesconectado(token);
    try {
      dispositivo.current?.gatt?.disconnect();
    } catch { /* ya estaba */ }
  }, [token, soltarWakeLock]);

  // El botón visible puede vivir en un menú que se desmonta, pero este
  // controlador debe permanecer en App durante toda la sesión. Por eso la UI
  // dispara un evento local en vez de volver a montar el puente.
  useEffect(() => {
    const alternar = () => {
      if (estado === 'conectado') desconectar();
      else if (estado === 'conectando' || estado === 'reconectando') return;
      else void conectar();
    };
    window.addEventListener('kira-ble-toggle', alternar);
    return () => window.removeEventListener('kira-ble-toggle', alternar);
  }, [conectar, desconectar, estado]);

  useEffect(() => {
    onEstado?.(estado);
  }, [estado, onEstado]);

  // ---------- TX: server -> placa (long-poll con token) ----------
  // El latido NO depende del estado de la UI: mientras seamos dueños, este
  // loop late. Antes vivía en un efecto atado a `estado` y cualquier
  // transición momentánea lo mataba en silencio (el server soltaba el puente
  // 60s después y todo quedaba mudo: el bug de "se desconecta a los ~10s").
  useEffect(() => {
    let vivo = true;
    const ciclo = async () => {
      let ultimoLatidoTx = 0;
      let vueltas = 0;
      while (vivo) {
        vueltas++;
        // LATIDO DE TRAZABILIDAD: sin esto, si este bucle deja de pedir
        // comandos (o se atasca en un await), desde el VPS solo se ve "el
        // puente está VIVO" y el SENSOR:TEMP muere en la cola sin una sola
        // pista. Pasó el 17-sep: 0 /api/ble/tx durante minutos y ningún
        // aviso. Cada 15s esta copia cuenta en qué estado está.
        if (Date.now() - ultimoLatidoTx > 15000) {
          ultimoLatidoTx = Date.now();
          reportar(`diag: tx vivo [${YO}] dueno=${esDueno.current ? 1 : 0} ` +
                   `enlace=${chars.current ? 1 : 0} gatt=${dispositivo.current?.gatt?.connected ? 1 : 0} ` +
                   `vueltas=${vueltas}`);
        }
        if (!esDueno.current) {
          await dormir(250);
          continue;
        }
        const ch = chars.current;
        const gatt = dispositivo.current?.gatt;
        // ¿EL ENLACE SIGUE VIVO DE VERDAD? Chrome puede NO avisar de la caida
        // (`gattserverdisconnected` no siempre llega: esta documentado que
        // falta si la placa se va de rango o se resetea). Si el GATT dice que
        // no estamos conectados, seguir escribiendo seria mandar comandos al
        // VACIO para siempre: la placa queda sin boca, sin cara y sin ACKs y
        // nadie se entera (bug del 17-sep). Aca se re-arma al toque.
        if (!ch || !gatt || !gatt.connected) {
          if (esDueno.current && dispositivo.current) {
            chars.current = null;
            rearmarRef.current('el GATT se cayo sin avisar');
          }
          await dormir(1000);
          continue;
        }
        try {
          const lineas = await bleTomarTx(token, 5);
          if (!vivo) break;
          if (lineas.length && chars.current) {
            try {
              const bytes = new TextEncoder().encode(lineas.join(''));
              for (let i = 0; i < bytes.length; i += 20) {
                await escribirConTope(chars.current.escribir, bytes.slice(i, i + 20));
              }
              // Ya hay comandos "esperando respuesta": eso es lo que mide la
              // guardia del enlace para decidir si el camino de vuelta murio.
              ultimaEscrituraMs.current = Date.now();
              comandosSinRespuesta.current += lineas.length;
              // Trazabilidad: sin esto no se puede distinguir "la app NO
              // escribio" de "escribio y la placa no contesta" (hoy no se
              // podia saber, y por eso el bug se escondio tanto).
              reportar(`diag: escribi ${bytes.length}B '${lineas.join('').trim()}'`);
            } catch (e) {
              // SI LA ESCRITURA FALLA, EL COMANDO NO SE PUEDE PERDER: el
              // server ya lo había borrado de su cola, así que lo devolvemos
              // (sin esto la boca/cara quedaban mudas tras la primera
              // respuesta: el bug que reportó el usuario).
              const detalle = e instanceof Error ? e.message : String(e);
              reportar(`diag: no pude escribir "${lineas.join('').trim()}" (${detalle}) -> devuelvo a la cola`);
              await bleDevolver(lineas, token);
              await dormir(300);
            }
          }
        } catch (e) {
          if (!vivo) break;
          const msg = e instanceof Error ? e.message : String(e);
          if (msg.includes('otro dispositivo')) {
            // nos robaron el puente: soltamos el server pero NO el enlace
            // (el enlace es del navegador entero y el dueño nuevo lo usa)
            perderPuesto('tx: otro dispositivo lo tomó');
            programarReclamo();
            await dormir(2000);
            continue;
          }
          if (msg.includes('expirado')) {
            // auto-cura: re-registrar y seguir como si nada (y DECIRLO: antes
            // esto era mudo y el bucle podía quedarse girando sin reportar)
            reportar(`diag: el server dio por expirado el puente [${YO}] -> re-registro`);
            if (!(await registrar())) {
              programarReclamo();
              await dormir(2000);
              continue;
            }
            continue;
          }
          // error de red o GATT: seguir intentando, sin matar el latido
          await dormir(500);
        }
      }
    };
    ciclo();
    return () => {
      vivo = false;
    };
  }, [token, registrar, perderPuesto, programarReclamo]);

  // ---------- RX: placa -> server (lotes de 60ms con token) ----------
  useEffect(() => {
    const t = setInterval(() => {
      if (!bandejaSalida.current.length || !esDueno.current) return;
      const lote = bandejaSalida.current;
      bandejaSalida.current = [];
      bleReportarRx(lote, token);
    }, 60);
    return () => clearInterval(t);
  }, [token]);

  // ---------- LATIDO EXTRA ----------
  // El long-poll ya late, pero este ping liviano cubre los huecos: mientras
  // reconectamos BLE (sin enlace) o si el long-poll se pausó, el puesto sigue
  // vivo. NO depende del estado de la UI: solo de "quiero ser el puente".
  useEffect(() => {
    let vivo = true;
    const latir = async () => {
      if (!vivo || !esDueno.current) return;
      try {
        await blePing(token);
      } catch (e) {
        if (!vivo) return;
        const msg = e instanceof Error ? e.message : String(e);
        if (msg.includes('expirado')) {
          const ok = await registrar();
          if (!ok) programarReclamo();
        } else if (msg.includes('otro dispositivo')) {
          perderPuesto('ping: otro dispositivo lo tomó');
          programarReclamo();
        }
      }
    };
    const t = setInterval(latir, LATIDO_MS);
    return () => {
      vivo = false;
      clearInterval(t);
    };
  }, [token, registrar, perderPuesto, programarReclamo]);

  // ---------- GUARDIA DEL ENLACE: si la placa se queda MUDA, re-armar ------
  // El sintoma (17-sep): `>> SENSOR:TEMP` salia del server, llegaba al celu y
  // del otro lado NUNCA volvia el `TEMP:26.5`. Chrome no avisa de eso, asi
  // que la salida es re-armar el enlace desde cero: servicios y
  // caracteristicas FRESCOS (los viejos quedan invalidados) + startNotifications
  // otra vez (el CCCD puede haber quedado apagado por otra pestaña).
  const rearmarEnlace = useCallback(async (motivo: string) => {
    if (rearmando.current || !dispositivo.current) return;
    rearmando.current = true;
    ultimoRearmeMs.current = Date.now();
    reportar(`diag: re-armo el enlace (${motivo})`);
    try {
      generacion.current++;
      const miGen = generacion.current;
      chars.current = null;
      try {
        dispositivo.current.gatt?.disconnect();
      } catch { /* ya estaba suelto */ }
      await dormir(250);
      if (miGen !== generacion.current) return;
      chars.current = await armarGATT();
      if (await registrar()) {
        setEstado('conectado');
        reportar('diag: enlace re-armado y notificaciones re-suscritas');
      } else {
        programarReclamo();
      }
    } catch (e) {
      const msg = e instanceof Error ? e.message : String(e);
      reportar(`diag: no pude re-armar el enlace (${msg})`);
      setEstado('desconectado');
      programarReclamo();
    } finally {
      rearmando.current = false;
    }
  }, [armarGATT, registrar, programarReclamo, reportar]);

  useEffect(() => {
    rearmarRef.current = rearmarEnlace;
  }, [rearmarEnlace]);

  useEffect(() => {
    const t = setInterval(() => {
      if (!esDueno.current || !chars.current) return;
      const ahora = Date.now();
      if (
        debeRearmarEnlace({
          esDueno: esDueno.current,
          tieneEnlace: !!chars.current,
          comandosSinRespuesta: comandosSinRespuesta.current,
          msDeSilencio: msDeSilencio(
            ahora,
            ultimaRespuestaMs.current,
            ultimaEscrituraMs.current
          ),
          msDesdeUltimoRearme:
            ultimoRearmeMs.current === 0 ? Infinity : ahora - ultimoRearmeMs.current,
        })
      ) {
        rearmarEnlace(`la placa no contesta (${comandosSinRespuesta.current} sin respuesta)`);
      }
    }, 2000);
    return () => clearInterval(t);
  }, [rearmarEnlace]);

  // ---------- la cara del botón ----------
  const caras: Record<Estado, { icono: typeof Bluetooth; texto: string; clase: string }> = {
    desconectado: { icono: Bluetooth, texto: 'conectar placa', clase: '' },
    conectando: { icono: RefreshCw, texto: 'conectando...', clase: '' },
    conectado: { icono: BluetoothConnected, texto: 'placa por BLE', clase: 'conectado' },
    reconectando: { icono: RefreshCw, texto: 'reconectando...', clase: 'reconectando' },
    rechazado: { icono: Bluetooth, texto: 'puente ocupado', clase: 'rechazado' },
  };
  const cara = caras[estado];
  const Icono = cara.icono;

  if (oculto) return null;

  // MODO COMPACTO: solo el icono, con el estado en el color + tooltip
  // (vive en el header del chat, sin ocupar la zona de mensajes)
  if (compacto) {
    return (
      <button
        className={`icon-btn puente-icono ${cara.clase}`}
        onClick={estado === 'conectado' ? desconectar : conectar}
        disabled={estado === 'conectando' || estado === 'reconectando'}
        title={
          estado === 'conectado'
            ? 'Placa conectada por Bluetooth. Toca para desconectar.'
            : estado === 'rechazado'
            ? 'Otro dispositivo tiene el puente. Toca para intentar de nuevo.'
            : estado === 'reconectando' || estado === 'conectando'
            ? 'Conectando la placa…'
            : 'Conectar el micro:bit por Bluetooth'
        }
        aria-label="Puente Bluetooth con el micro:bit"
      >
        {estado === 'reconectando' || estado === 'conectando' ? (
          <Icono size={17} className="girando" />
        ) : (
          <Icono size={17} />
        )}
      </button>
    );
  }

  return (
    <button
      className={`puente-ble ${cara.clase}`}
      onClick={estado === 'conectado' ? desconectar : conectar}
      disabled={estado === 'conectando' || estado === 'reconectando'}
      title={
        estado === 'conectado'
          ? 'La placa obedece por Bluetooth. Toca para desconectar.'
          : estado === 'rechazado'
          ? 'Otro dispositivo ya es el puente. Toca para intentar de nuevo.'
          : 'Conecta el micro:bit por Bluetooth: la IA lo controla a través de este teléfono.'
      }
    >
      {estado === 'reconectando' || estado === 'conectando' ? (
        <Icono size={15} className="girando" />
      ) : (
        <Icono size={15} />
      )}
      <span>{cara.texto}</span>
    </button>
  );
}
