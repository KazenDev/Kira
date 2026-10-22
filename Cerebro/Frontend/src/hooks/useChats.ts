import { useEffect, useRef, useState } from 'react';
import {
  cargarChats,
  guardarChats,
  type ChatsPorPersonaje,
} from '../lib/almacenamiento';
import { crearVolcadoDiferido, type VolcadoDiferido } from '../lib/volcado-diferido';

/**
 * Conversaciones locales + persistencia en localStorage.
 *
 * La escritura va DEBOUNCED: durante el streaming el último mensaje cambia
 * ~30 veces por segundo, y serializar todo el historial en cada tick bloquea
 * el hilo principal (se notaba como micro-tirones en celulares viejos).
 * Se vuelca al desmontar y al ocultar la pestaña, para no perder nada.
 */
export function useChats(usuarioId: string) {
  const [chats, setChats] = useState<ChatsPorPersonaje>(() => cargarChats(usuarioId));
  const volcadoRef = useRef<VolcadoDiferido<ChatsPorPersonaje> | null>(null);

  // Un volcado por usuario (no se recrea en cada cambio de chats).
  useEffect(() => {
    const volcado = crearVolcadoDiferido<ChatsPorPersonaje>(
      (valor) => guardarChats(usuarioId, valor),
      700
    );
    volcadoRef.current = volcado;
    return () => {
      // Nada de perder la última respuesta si desmontan la app.
      volcado.volcar();
      volcadoRef.current = null;
    };
  }, [usuarioId]);

  useEffect(() => {
    volcadoRef.current?.programar(chats);
  }, [chats]);

  // Tampoco si cierran la pestaña o cambian de app a mitad del streaming.
  useEffect(() => {
    const alOcultar = () => {
      if (document.visibilityState === 'hidden') volcadoRef.current?.volcar();
    };
    document.addEventListener('visibilitychange', alOcultar);
    return () => document.removeEventListener('visibilitychange', alOcultar);
  }, []);

  return { chats, setChats };
}
