// Recepción de líneas de la placa: parsea el buffer de notificaciones, avisa
// los botones A/B al instante y manda los lotes al servidor.
import { useCallback, useEffect, useRef } from 'react';
import type { MutableRefObject } from 'react';
import { bleReportarRx } from '../../lib/api';

type Opciones = {
  token: string;
  esDueno: MutableRefObject<boolean>;
};

export function useRecepcionPlaca({ token, esDueno }: Opciones) {
  const buferEntrada = useRef('');
  const bandejaSalida = useRef<string[]>([]);
  // GUARDIA DEL ENLACE: relojes de "mandé y no contestó".
  const ultimaRespuestaMs = useRef(0);
  const comandosSinRespuesta = useRef(0);

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
          // BOTONES DE LA ESCUCHA: los atiende la app AL INSTANTE (graba con el
          // micrófono del celular: el audio de la placa no entra por aire)
          if (linea === 'TECLA:A' || linea === 'TECLA:B') {
            window.dispatchEvent(new CustomEvent('kira-tecla', { detail: linea.slice(6) }));
          }
          bandejaSalida.current.push(linea);
          // LA PLACA HABLÓ: el camino de vuelta está vivo (y el firmware nuevo
          // manda un HELLO al detectar el enlace, que también cae aquí).
          ultimaRespuestaMs.current = Date.now();
          comandosSinRespuesta.current = 0;
        }
      }
      if (buferEntrada.current.length > 512) buferEntrada.current = '';
    });
  }, []);

  // ---------- RX: placa -> server (lotes de 60ms con token) ----------
  useEffect(() => {
    const t = setInterval(() => {
      if (!bandejaSalida.current.length || !esDueno.current) return;
      const lote = bandejaSalida.current;
      bandejaSalida.current = [];
      bleReportarRx(lote, token);
    }, 60);
    return () => clearInterval(t);
  }, [token, esDueno]);

  /** GATT fresco: el reloj de la guardia del enlace arranca de cero. */
  const reiniciarRelojes = useCallback(() => {
    comandosSinRespuesta.current = 0;
    ultimaRespuestaMs.current = 0;
  }, []);

  return {
    escucharPlaca,
    ultimaRespuestaMs,
    comandosSinRespuesta,
    reiniciarRelojes,
  };
}
