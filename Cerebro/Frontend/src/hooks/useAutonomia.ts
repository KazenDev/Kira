// Autonomía: Kira habla sola cuando la placa avisa algo o cuando hace rato
// que nadie le habla con la pantalla abierta.
import { useCallback, useEffect, useRef } from 'react';
import { obtenerEventos } from '../lib/api';

const PAUSA_MS = 20000; // entre dos intervenciones espontáneas
const SILENCIO_MS = 4 * 60000; // pantalla abierta y nadie habla
const VOLVIO_MS = 2 * 60000; // estuvo afuera de la pestaña al menos esto

type Opciones = {
  activa: boolean;
  /** Hay un turno en curso o la placa está sonando/grabando. */
  libre: () => boolean;
  /** Dispara un turno autónomo con el evento (SILENCIO, VOLVIO, ...). */
  hablarSola: (evento: string) => void;
};

export function useAutonomia({ activa, libre, hablarSola }: Opciones) {
  const ultimaActividadRef = useRef(Date.now());
  const ultimoProactivoRef = useRef(0);
  const seqEventosRef = useRef(-1);
  const ocultaDesdeRef = useRef<number | null>(null);
  const libreRef = useRef(libre);
  libreRef.current = libre;
  const hablarSolaRef = useRef(hablarSola);
  hablarSolaRef.current = hablarSola;

  const marcarActividad = useCallback(() => {
    ultimaActividadRef.current = Date.now();
  }, []);

  useEffect(() => {
    if (!activa) return;
    let vivo = true;

    const puede = () =>
      libreRef.current() &&
      Date.now() - ultimoProactivoRef.current > PAUSA_MS &&
      Date.now() - ultimaActividadRef.current > 6000;

    const hablar = (evento: string) => {
      if (!puede()) return false;
      ultimoProactivoRef.current = Date.now();
      hablarSolaRef.current(evento);
      return true;
    };

    // Un evento que llega mientras Kira habla o piensa NO se pierde: queda
    // esperando hasta 90 s (antes se tiraba, y el atardecer pasa una vez).
    // Los del mundo real y los juegos ganan contra un toque cualquiera.
    let pendiente: { linea: string; t: number } | null = null;
    const pesa = (l: string) =>
      l.startsWith('MUNDO:') || l.startsWith('JUEGO:') || l === 'EVENTO:TIMER' ? 2 : 1;

    const tick = async () => {
      if (document.hidden) return;
      try {
        const r = await obtenerEventos(seqEventosRef.current);
        if (!vivo) return;
        const primeraVez = seqEventosRef.current < 0;
        seqEventosRef.current = r.seq;
        // El último evento que valga la pena (los de antes de abrir la web no)
        // (los OIDO: son la persona hablándole a la placa: no son autonomía)
        const propios = r.eventos.filter((e) => !e.linea.startsWith('OIDO:'));
        const ultimo = primeraVez ? undefined : propios[propios.length - 1];
        if (ultimo && (!pendiente || pesa(ultimo.linea) >= pesa(pendiente.linea))) {
          pendiente = { linea: ultimo.linea, t: Date.now() };
        }
        if (pendiente && Date.now() - pendiente.t > 90000) pendiente = null;
        if (pendiente && hablar(pendiente.linea)) {
          pendiente = null;
          return;
        }
      } catch {
        // sin eventos (backend viejo, sin placa): la autonomía por silencio sigue
      }
      const quieto =
        Date.now() - Math.max(ultimaActividadRef.current, ultimoProactivoRef.current);
      if (quieto > SILENCIO_MS) hablar('SILENCIO');
    };

    const alCambiarVisibilidad = () => {
      if (document.hidden) {
        ocultaDesdeRef.current = Date.now();
        return;
      }
      const desde = ocultaDesdeRef.current;
      ocultaDesdeRef.current = null;
      if (desde && Date.now() - desde > VOLVIO_MS) hablar('VOLVIO');
    };

    const id = window.setInterval(() => void tick(), 1500);
    document.addEventListener('visibilitychange', alCambiarVisibilidad);
    return () => {
      vivo = false;
      window.clearInterval(id);
      document.removeEventListener('visibilitychange', alCambiarVisibilidad);
    };
  }, [activa]);

  return { marcarActividad };
}
