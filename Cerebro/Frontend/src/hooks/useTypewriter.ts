// Typewriter real: revela el texto del stream palabra por palabra, aunque el
// navegador entregue el SSE completo de golpe. Con voz activa no pasa de lo
// que Kira ya dijo (topeVoz).
import { useCallback, useEffect, useRef, useState } from 'react';
import type { Dispatch, MutableRefObject, SetStateAction } from 'react';
import { bocaDeLetra, kiraVivo } from '../lib/vivo';
import type { MensajeChat } from '../lib/api';

type Opciones = {
  setMensajes: Dispatch<SetStateAction<MensajeChat[]>>;
  /** Letras que ya se dijeron con la voz (Infinity = sin tope). */
  topeVoz: () => number;
  /** ¿Hay un clip de voz sonando ahora? (para animar la boca). */
  sincronizarBoca: () => boolean;
};

export function useTypewriter({ setMensajes, topeVoz, sincronizarBoca }: Opciones) {
  const [streamId, setStreamId] = useState<string | null>(null);
  const streamIdRef = useRef<string | null>(null);
  const ultimoTextoRef = useRef('');
  const streamObjetivoRef = useRef('');
  const streamReveladoRef = useRef(0);
  const streamTimerRef = useRef<number | null>(null);
  const streamFinRef = useRef<(() => void) | null>(null);

  const detener = useCallback(() => {
    if (streamTimerRef.current !== null) {
      window.clearInterval(streamTimerRef.current);
      streamTimerRef.current = null;
    }
    streamFinRef.current = null;
  }, []);

  const arrancar = useCallback(() => {
    if (streamTimerRef.current !== null) return;
    streamTimerRef.current = window.setInterval(() => {
      const objetivo = streamObjetivoRef.current;
      const revelado = streamReveladoRef.current;
      const idParcial = streamIdRef.current;
      if (!idParcial) {
        detener();
        return;
      }
      const tope = Math.min(objetivo.length, topeVoz());
      if (revelado >= tope && revelado < objetivo.length) {
        kiraVivo.boca *= 0.6; // pausa de la voz: la boca se cierra
        return; // esperando a la voz
      }
      if (revelado >= objetivo.length) {
        // alcanzó el texto actual: pausamos el timer (un próximo delta lo retoma)
        window.clearInterval(streamTimerRef.current!);
        streamTimerRef.current = null;
        const fin = streamFinRef.current;
        if (fin) {
          streamFinRef.current = null;
          fin();
        }
        return;
      }
      // velocidad adaptativa: un mensaje largo revela más rápido (máx ~4s)
      const vel = Math.max(1, Math.ceil(objetivo.length / 150));
      const siguiente = Math.min(revelado + vel, tope);
      streamReveladoRef.current = siguiente;
      if (sincronizarBoca()) {
        kiraVivo.boca = bocaDeLetra(objetivo[siguiente - 1]);
      }
      const texto = objetivo.slice(0, siguiente);
      setMensajes((m) =>
        m.map((msg) => (msg.id === idParcial ? { ...msg, contenido: texto } : msg))
      );
    }, 30);
  }, [detener, setMensajes, topeVoz, sincronizarBoca]);

  /** Arranca un mensaje parcial nuevo (o reutiliza el id al regenerar). */
  const iniciar = useCallback((id: string) => {
    streamIdRef.current = id;
    setStreamId(id);
  }, []);

  /** Prepara el buffer para un turno nuevo. */
  const preparar = useCallback(() => {
    detener();
    streamObjetivoRef.current = '';
    streamReveladoRef.current = 0;
  }, [detener]);

  const setObjetivo = useCallback((texto: string) => {
    streamObjetivoRef.current = texto;
  }, []);

  const setFin = useCallback((fn: (() => void) | null) => {
    streamFinRef.current = fn;
  }, []);

  /** El stream terminó: se suelta el parcial y deja de haber mensaje en curso. */
  const terminar = useCallback(() => {
    streamIdRef.current = null;
    setStreamId(null);
  }, []);

  const idEnCurso = useCallback(() => streamIdRef.current, []);
  const revelado = useCallback(() => streamReveladoRef.current, []);
  const largoObjetivo = useCallback(() => streamObjetivoRef.current.length, []);
  const hayFin = useCallback(() => streamFinRef.current !== null, []);

  useEffect(() => {
    return () => {
      if (streamTimerRef.current !== null) window.clearInterval(streamTimerRef.current);
    };
  }, []);

  return {
    streamId,
    idEnCurso,
    revelado,
    largoObjetivo,
    hayFin,
    ultimoTextoRef: ultimoTextoRef as MutableRefObject<string>,
    iniciar,
    preparar,
    setObjetivo,
    setFin,
    arrancar,
    detener,
    terminar,
  };
}
