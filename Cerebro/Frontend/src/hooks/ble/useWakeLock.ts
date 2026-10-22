// Mantiene la pantalla encendida mientras el puente está activo. Sin esto,
// Chrome congela el JS con la pantalla apagada y los comandos llegan en
// ráfaga al despertar (la placa hace "la película loca").
import { useCallback, useEffect, useRef } from 'react';
import type { MutableRefObject } from 'react';
import type { Estado, LockPantalla } from '../../lib/ble-protocolo';

type Opciones = {
  estado: Estado;
  esDueno: MutableRefObject<boolean>;
  quierePuente: MutableRefObject<boolean>;
  reportar: (mensaje: string) => void;
};

export function useWakeLock({ estado, esDueno, quierePuente, reportar }: Opciones) {
  const wakeLock = useRef<LockPantalla | null>(null);

  const pedir = useCallback(async () => {
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
          setTimeout(() => pedir(), 500);
        }
      });
    } catch (e) {
      // OJO: si esto falla, la pantalla se apaga → Chrome congela el JS →
      // adiós latido. Antes era un catch mudo: ahora queda en el log.
      reportar(`wake-lock falló: ${e instanceof Error ? e.message : String(e)}`);
    }
  }, [esDueno, quierePuente, reportar]);

  const soltar = useCallback(() => {
    try {
      wakeLock.current?.release();
    } catch {
      /* ya estaba suelto */
    }
    wakeLock.current = null;
  }, []);

  useEffect(() => {
    if (estado === 'conectado' || estado === 'reconectando' || quierePuente.current) void pedir();
    if (estado === 'desconectado' || estado === 'rechazado') soltar();
  }, [estado, pedir, soltar, quierePuente]);

  return { pedir, soltar };
}
