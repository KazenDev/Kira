// Títulos automáticos de los chats: se generan en segundo plano, UNA vez por
// sesión, y solo si el título sigue siendo el provisorio "Chat N".
import { useCallback, useRef } from 'react';
import type { Dispatch, SetStateAction } from 'react';
import { generarTitulo } from '../lib/api';
import type { ChatsPorPersonaje } from '../lib/almacenamiento';

export function useTitulosChat(
  setChats: Dispatch<SetStateAction<ChatsPorPersonaje>>
) {
  const titulosEnCursoRef = useRef<Set<string>>(new Set());

  const generarTituloChat = useCallback(
    (sesionId: string, pjId: string, primerMensaje: string, respuesta: string) => {
      if (titulosEnCursoRef.current.has(sesionId)) return;
      titulosEnCursoRef.current.add(sesionId);
      generarTitulo(primerMensaje, respuesta)
        .then((titulo) => {
          setChats((c) => ({
            ...c,
            [pjId]: (c[pjId] ?? []).map((s) =>
              s.id === sesionId && s.titulo.startsWith('Chat ') ? { ...s, titulo } : s
            ),
          }));
        })
        .catch(() => {
          // Si falla, el título por defecto se conserva; no afecta el chat.
        })
        .finally(() => {
          titulosEnCursoRef.current.delete(sesionId);
        });
    },
    [setChats]
  );

  return { generarTituloChat };
}
