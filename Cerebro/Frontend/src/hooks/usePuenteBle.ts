// Controlador del puente BLE: orquesta las piezas de ./ble (token, liderazgo
// entre pestañas, wake lock, recepción, latidos) y la máquina de conexión.
import { useCallback, useEffect, useRef, useState } from 'react';
import {
  bleMarcarConectado,
  bleMarcarDesconectado,
  bleReportarError,
} from '../lib/api';
import { debeRearmarEnlace, msDeSilencio } from '../lib/enlace-ble';
import {
  NUS_ESCRIBIR,
  NUS_LEER,
  NUS_SERVICIO,
  RECLAMO_BASE_MS,
  RECLAMO_MAX_FALLOS,
  RECLAMO_TOPE_MS,
  REGISTRO_MIN_MS,
  backoff,
  conectarConTope,
  dormir,
  type Caracteristicas,
  type Estado,
  type EstadoPuenteBle,
} from '../lib/ble-protocolo';
import { useTokenPuente } from './ble/useTokenPuente';
import { useLiderPestanas } from './ble/useLiderPestanas';
import { useWakeLock } from './ble/useWakeLock';
import { useRecepcionPlaca } from './ble/useRecepcionPlaca';
import { useLazosPuente } from './ble/useLazosPuente';

type Opciones = {
  onEstado?: (estado: EstadoPuenteBle) => void;
};

/** Toda la máquina de estados del puente (conexión, liderazgo entre pestañas,
 *  latidos, reconexión y guardia del enlace). */
export function usePuenteBle({ onEstado }: Opciones = {}) {
  const [estado, setEstado] = useState<Estado>('desconectado');
  const chars = useRef<Caracteristicas | null>(null);
  const dispositivo = useRef<BluetoothDevice | null>(null);
  // a qué dispositivo ya le colgamos el listener de 'gattserverdisconnected'
  // (sin esto, cada intento agregaba OTRO listener y una caída disparaba N
  // reconexiones a la vez)
  const deviceEscuchado = useRef<BluetoothDevice | null>(null);
  const cierreManual = useRef(false);
  const generacion = useRef(0); // invalida loops viejos tras reconectar

  // esDueno: EL SERVER nos aceptó como puente. Solo con esto tiene sentido
  // mandar comandos por aire (y es lo que mantiene vivo el latido).
  const esDueno = useRef(false);
  // reconectar() se define más abajo pero el aviso de caída del GATT se
  // cuelga antes: este ref mantiene la versión fresca sin dependencias raras.
  const reconectarRef = useRef<(miGen: number) => void>(() => {});
  // intentarPuente() se usa desde programarReclamo(), definido antes.
  const intentarPuenteRef = useRef<() => Promise<void>>(async () => {});
  const retirarseRef = useRef<(motivo: string) => void>(() => {});
  // quierePuente: este dispositivo quiere ser el cable (lo pone el botón o el
  // auto-connect al cargar). NO es lo mismo que "está conectado".
  const quierePuente = useRef(false);

  const token = useTokenPuente();
  const recepcion = useRecepcionPlaca({ token, esDueno });

  /** Reporte al server: en el cel no hay consola, asi que el log del VPS es
   * nuestro depurador. Los `diag:` son transiciones normales (ruido util). */
  const reportar = useCallback((mensaje: string) => {
    bleReportarError(mensaje);
  }, []);

  const wake = useWakeLock({ estado, esDueno, quierePuente, reportar });

  const lider = useLiderPestanas({
    token,
    quierePuente,
    esDueno,
    alPerderPrioridad: () => retirarseRef.current('otra pestaña tiene prioridad'),
  });

  // fallos de reclamo seguidos (para el backoff) y si ya nos rendimos
  const timerReclamo = useRef<ReturnType<typeof setTimeout> | null>(null);
  const fallosReclamo = useRef(0);
  // promesa de registro en vuelo (una sola a la vez) + hora del último
  const registrando = useRef<Promise<boolean> | null>(null);
  const ultimoRegistroMs = useRef(0);

  // GUARDIA DEL ENLACE: medimos el silencio con los comandos que mandamos y,
  // si la placa no contesta, re-armamos GATT + notificaciones (en Android el
  // CCCD lo maneja la app: otra pestaña puede haberlo apagado).
  const ultimaEscrituraMs = useRef(0);
  const ultimoRearmeMs = useRef(0);
  const rearmando = useRef(false);

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
      if (quierePuente.current && !esDueno.current) intentarPuenteRef.current();
    }, espera);
  }, []);

  /** Perder el puesto: se suelta el SERVER y se deja de mandar comandos.
   * NO se desconecta el enlace BLE (es compartido con la pestaña ganadora). */
  const perderPuesto = useCallback(
    (motivo: string) => {
      if (!esDueno.current) return;
      esDueno.current = false;
      bleMarcarDesconectado(token);
      setEstado('rechazado');
      reportar(`diag: solté el puesto (${motivo})`);
    },
    [token, reportar]
  );

  /** Retirarse por el duelo de pestañas: suelta el server y espera su turno. */
  const retirarse = useCallback(
    (motivo: string) => {
      cierreManual.current = false; // no es manual: se puede reintentar
      perderPuesto(`duelo: ${motivo}`);
      programarReclamo();
    },
    [perderPuesto, programarReclamo]
  );
  retirarseRef.current = retirarse;

  /** Cuelga el aviso de caída UNA sola vez por dispositivo (y por sesión). */
  const vigilarCaida = useCallback(
    (device: BluetoothDevice) => {
      if (deviceEscuchado.current === device) return;
      deviceEscuchado.current = device;
      const miGen = generacion.current;
      device.addEventListener('gattserverdisconnected', () => {
        if (cierreManual.current || miGen !== generacion.current) return;
        // si el enlace se cayó, dejamos de ser el cable hasta reconectar
        reportar('diag: la placa se desconectó (evento del navegador)');
        reconectarRef.current(miGen);
      });
    },
    [reportar]
  );

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
    recepcion.escucharPlaca(leer);
    // GATT fresco: el reloj de la guardia del enlace arranca de cero (vale
    // para el connect inicial, la reconexión y el re-arme).
    recepcion.reiniciarRelojes();
    ultimaEscrituraMs.current = 0;
    return { escribir, leer };
  }, [recepcion.escucharPlaca, recepcion.reiniciarRelojes]);

  // ---------- registro como dueño del puente en el server ----------
  const registrar = useCallback(async (): Promise<boolean> => {
    // ANTI-TORMENTA: una sola llamada en vuelo, y nunca dos registros seguidos
    // más rápido que REGISTRO_MIN_MS. Si ya somos dueños y venimos de
    // registrar, no hace falta volver a pedirlo.
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
      // reportar el motivo al server: sin esto el VPS no sabe POR QUÉ cayó
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
  // pateaba el enlace de la pestaña que sí era dueña (en Chrome hay un solo
  // enlace por placa por navegador).
  const intentarPuente = useCallback(async () => {
    if (!quierePuente.current) return;
    // si otra pestaña tiene prioridad (token menor y latiendo hace <4s), ni
    // intentar: sería patalear contra el dueño
    if (
      Date.now() - lider.ultimoPingAjeno.current < 4000 &&
      lider.tokenAjeno.current &&
      lider.tokenAjeno.current < token
    ) {
      programarReclamo();
      return;
    }
    try {
      // getDevices() NO EXISTE en Chrome Android (es de escritorio / detrás de
      // flag): sin él no hay forma de "recordar" la placa entre recargas. Si
      // falta, solo sirve la referencia que ya tengamos EN MEMORIA (esta
      // pestaña) — el patrón que recomienda Chrome: reconectar con el
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
  }, [
    registrar,
    programarReclamo,
    vigilarCaida,
    token,
    reportar,
    armarGATT,
    lider.ultimoPingAjeno,
    lider.tokenAjeno,
  ]);
  intentarPuenteRef.current = intentarPuente;

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

  // ---------- VOLVER A LA PESTAÑA: recuperar puesto y latido ----------
  // Con la pantalla apagada Chrome congela el JS: el server libera el puente
  // a los 120s. Al volver, re-registramos AL INSTANTE y re-armamos si hace
  // falta, en vez de esperar al reclamo de 15s. Los comandos que quedaron
  // pendientes los entrega el long-poll en cuanto volvemos (ya no se tiran).
  useEffect(() => {
    const alVolver = () => {
      if (document.visibilityState !== 'visible') return;
      if (!quierePuente.current) return;
      wake.pedir();
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
  }, [wake.pedir, intentarPuente, reportar]);

  // ---------- desconexión manual ----------
  const desconectar = useCallback(() => {
    quierePuente.current = false; // ya no quiero el puente: sin pings ni reclamos
    if (timerReclamo.current) clearTimeout(timerReclamo.current);
    cierreManual.current = true;
    generacion.current++;
    chars.current = null;
    esDueno.current = false;
    setEstado('desconectado');
    wake.soltar();
    bleMarcarDesconectado(token);
    try {
      dispositivo.current?.gatt?.disconnect();
    } catch {
      /* ya estaba */
    }
  }, [token, wake.soltar]);

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

  // ---------- GUARDIA DEL ENLACE: si la placa se queda MUDA, re-armar ------
  // El síntoma (17-sep): `>> SENSOR:TEMP` salía del server, llegaba al celu y
  // del otro lado NUNCA volvía el `TEMP:26.5`. Chrome no avisa de eso, así
  // que la salida es re-armar el enlace desde cero: servicios y
  // características FRESCOS (los viejos quedan invalidados) + startNotifications
  // otra vez (el CCCD puede haber quedado apagado por otra pestaña).
  const rearmarEnlace = useCallback(
    async (motivo: string) => {
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
        } catch {
          /* ya estaba suelto */
        }
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
    },
    [armarGATT, registrar, programarReclamo, reportar]
  );

  useEffect(() => {
    const t = setInterval(() => {
      if (!esDueno.current || !chars.current) return;
      const ahora = Date.now();
      if (
        debeRearmarEnlace({
          esDueno: esDueno.current,
          tieneEnlace: !!chars.current,
          comandosSinRespuesta: recepcion.comandosSinRespuesta.current,
          msDeSilencio: msDeSilencio(
            ahora,
            recepcion.ultimaRespuestaMs.current,
            ultimaEscrituraMs.current
          ),
          msDesdeUltimoRearme:
            ultimoRearmeMs.current === 0 ? Infinity : ahora - ultimoRearmeMs.current,
        })
      ) {
        rearmarEnlace(
          `la placa no contesta (${recepcion.comandosSinRespuesta.current} sin respuesta)`
        );
      }
    }, 2000);
    return () => clearInterval(t);
  }, [rearmarEnlace, recepcion.comandosSinRespuesta, recepcion.ultimaRespuestaMs]);

  useLazosPuente({
    token,
    esDueno,
    chars,
    dispositivo,
    ultimaEscrituraMs,
    comandosSinRespuesta: recepcion.comandosSinRespuesta,
    reportar,
    rearmar: (motivo) => void rearmarEnlace(motivo),
    registrar,
    perderPuesto,
    programarReclamo,
  });

  return { estado, conectar, desconectar };
}
