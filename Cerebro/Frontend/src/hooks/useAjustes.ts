import { useCallback, useState } from 'react';
import { cargarAjustes, guardarAjustes, type AjustesLocales } from '../lib/ajustes';

/** Ajustes locales del usuario + persistencia transparente en localStorage. */
export function useAjustes(usuarioId: string) {
  const [ajustes, setAjustes] = useState<AjustesLocales>(() => cargarAjustes(usuarioId));

  const cambiar = useCallback(
    <K extends keyof AjustesLocales>(clave: K, valor: AjustesLocales[K]) => {
      setAjustes((actual) => {
        const siguiente = { ...actual, [clave]: valor };
        guardarAjustes(usuarioId, siguiente);
        return siguiente;
      });
    },
    [usuarioId]
  );

  return { ajustes, cambiar };
}
