// Líder único entre pestañas: un solo puente. Dos tabs = dos tokens =
// comandos al vacío para el perdedor. Quien quiere el puente anuncia su token
// cada 2s; gana el MENOR (determinista). El perdedor se retira del SERVER
// (no del enlace BLE: el enlace es uno solo por navegador y desconectarlo
// rompía al ganador).
import { useEffect, useRef } from 'react';
import type { MutableRefObject } from 'react';

type Opciones = {
  token: string;
  quierePuente: MutableRefObject<boolean>;
  esDueno: MutableRefObject<boolean>;
  /** Otra pestaña con token menor nos ganó: soltar el puesto y reintentar. */
  alPerderPrioridad: () => void;
};

export function useLiderPestanas({
  token,
  quierePuente,
  esDueno,
  alPerderPrioridad,
}: Opciones) {
  const canalLider = useRef<BroadcastChannel | null>(null);
  const ultimoPingAjeno = useRef(0);
  const tokenAjeno = useRef('');
  const alPerderRef = useRef(alPerderPrioridad);
  alPerderRef.current = alPerderPrioridad;

  useEffect(() => {
    let canal: BroadcastChannel | null = null;
    try {
      canal = new BroadcastChannel('kira-puente');
    } catch {
      canal = null;
    }
    canalLider.current = canal;
    if (!canal) return () => {};
    canal.onmessage = (ev: MessageEvent) => {
      const otro = String(ev.data?.token ?? '');
      if (!otro || otro === token) return;
      ultimoPingAjeno.current = Date.now();
      tokenAjeno.current = otro;
      // duelo: el token MENOR gana; si pierdo y TENGO el puesto, lo suelto
      if (otro < token && esDueno.current) {
        alPerderRef.current();
      }
    };
    return () => {
      canal.close();
      canalLider.current = null;
    };
  }, [token, esDueno]);

  // ping de presencia mientras se QUIERE el puente (no solo teniéndolo:
  // así el duelo converge aunque el server me haya dado 409)
  useEffect(() => {
    const t = setInterval(() => {
      if (quierePuente.current) {
        try {
          canalLider.current?.postMessage({ token });
        } catch {
          /* canal caído: ni modo */
        }
      }
    }, 2000);
    return () => clearInterval(t);
  }, [token, quierePuente]);

  return { ultimoPingAjeno, tokenAjeno };
}
