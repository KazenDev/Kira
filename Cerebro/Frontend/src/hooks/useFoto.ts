// Foto pendiente de enviar: cámara del celular, reducción y miniatura.
import { useCallback, useRef, useState } from 'react';
import type { ChangeEvent } from 'react';
import { fotoDesdeArchivo } from '../lib/foto';

export function useFoto(mostrarErrorTemporal: (mensaje: string) => void) {
  const [foto, setFoto] = useState<string | null>(null);
  const [procesandoFoto, setProcesandoFoto] = useState(false);
  const fotoInputRef = useRef<HTMLInputElement | null>(null);

  // El mismo input con `capture` abre la cámara trasera en el celular (y el
  // selector de archivos en la compu). La reducción vive en foto.ts.
  const elegirFoto = useCallback(
    async (e: ChangeEvent<HTMLInputElement>) => {
      const archivo = e.target.files?.[0];
      // limpiar el input: sin esto, volver a elegir LA MISMA foto no dispara
      e.target.value = '';
      if (!archivo) return;
      setProcesandoFoto(true);
      try {
        setFoto(await fotoDesdeArchivo(archivo));
      } catch {
        mostrarErrorTemporal('No se pudo leer la foto, probá otra vez');
      } finally {
        setProcesandoFoto(false);
      }
    },
    [mostrarErrorTemporal]
  );

  return { foto, setFoto, procesandoFoto, fotoInputRef, elegirFoto };
}
