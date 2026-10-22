// Navegación y persistencia de conversaciones: abrir, crear, borrar, volver,
// títulos automáticos y derivados del sidebar/hero.
import { useCallback, useEffect, useMemo, useRef } from 'react';
import type { Dispatch, SetStateAction } from 'react';
import { postSilencioso, type MensajeChat, type PersonajeInfo } from '../lib/api';
import type { ChatsPorPersonaje } from '../lib/almacenamiento';
import { claveActual } from '../lib/almacenamiento';
import type { ChatSesion } from '../lib/tipos';
import { tituloSinEmoji } from '../lib/tipos';
import { uuid } from '../lib/ui-utils';
import { sonidoUI } from '../lib/sonidos';

type Opciones = {
  usuarioId: string;
  personajes: PersonajeInfo[];
  chats: ChatsPorPersonaje;
  setChats: Dispatch<SetStateAction<ChatsPorPersonaje>>;
  elegido: PersonajeInfo | null;
  setElegido: Dispatch<SetStateAction<PersonajeInfo | null>>;
  chatActivo: string | null;
  setChatActivo: Dispatch<SetStateAction<string | null>>;
  mensajes: MensajeChat[];
  setMensajes: Dispatch<SetStateAction<MensajeChat[]>>;
  /** Corta micrófono y respuesta en curso antes de cambiar de chat. */
  detenerConversacion: () => void;
  setEmotion: (emotion: string) => void;
  setMemoriaAbierta: Dispatch<SetStateAction<boolean>>;
  setAjustesAbiertos: Dispatch<SetStateAction<boolean>>;
  setError: Dispatch<SetStateAction<string>>;
  setHablando: (v: boolean) => void;
  cerrarSidebar: () => void;
};

export function useNavegacionChats({
  usuarioId,
  personajes,
  chats,
  setChats,
  elegido,
  setElegido,
  chatActivo,
  setChatActivo,
  mensajes,
  setMensajes,
  detenerConversacion,
  setEmotion,
  setMemoriaAbierta,
  setAjustesAbiertos,
  setError,
  setHablando,
  cerrarSidebar,
}: Opciones) {
  // sincronizar los mensajes actuales con la sesión activa (se guardan en chats)
  useEffect(() => {
    if (!elegido || !chatActivo) return;
    setChats((c) => ({
      ...c,
      [elegido.id]: (c[elegido.id] ?? []).map((s) => {
        if (s.id !== chatActivo) return s;
        // el orden del sidebar solo cambia cuando llega un mensaje NUEVO,
        // no cuando simplemente se abre un chat
        const ultimoNuevo = mensajes[mensajes.length - 1]?.id;
        const ultimoViejo = s.mensajes[s.mensajes.length - 1]?.id;
        const huboNuevo = ultimoNuevo !== ultimoViejo;
        return {
          ...s,
          mensajes,
          fecha: huboNuevo ? new Date().toISOString() : s.fecha,
        };
      }),
    }));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mensajes, chatActivo]);

  const nuevoChatImpl = (pj: PersonajeInfo) => {
    sonidoUI('cambio');
    detenerConversacion();
    setMemoriaAbierta(false);
    setElegido(pj);
    cerrarSidebar();
    // Solo cuentan los títulos automáticos "Chat N"; un título como "RTX 5090"
    // no debe convertir el siguiente chat en "Chat 5091".
    const numeros = (chats[pj.id] ?? [])
      .map((c) => c.titulo.match(/^Chat\s+(\d+)$/i)?.[1])
      .filter((n): n is string => Boolean(n))
      .map(Number);
    const sesion: ChatSesion = {
      id: uuid(),
      titulo: `Chat ${(numeros.length ? Math.max(...numeros) : 0) + 1}`,
      fecha: new Date().toISOString(),
      // sin saludo guardado: el chat nuevo muestra la cara viva y un
      // saludo de pantalla (antes era un texto fijo de asistente)
      mensajes: [],
    };
    setChats((c) => ({ ...c, [pj.id]: [...(c[pj.id] ?? []), sesion] }));
    setChatActivo(sesion.id);
    setMensajes(sesion.mensajes);
    setEmotion('happy');
    localStorage.setItem(claveActual(usuarioId, pj.id), sesion.id);
  };

  const abrirChatImpl = (pj: PersonajeInfo, sesionId: string) => {
    sonidoUI('cambio'); // micro-tono al cambiar de conversación (opcional)
    detenerConversacion();

    const lista = chats[pj.id] ?? [];
    const sesion = lista.find((c) => c.id === sesionId) ?? lista[lista.length - 1];
    if (!sesion) {
      nuevoChatImpl(pj);
      return;
    }

    setElegido(pj);
    setChatActivo(sesion.id);
    cerrarSidebar();
    setMensajes(sesion.mensajes);

    const ultimo = [...sesion.mensajes].reverse().find((m) => m.rol === pj.id);
    setEmotion(ultimo?.emotion ?? 'happy');
    localStorage.setItem(claveActual(usuarioId, pj.id), sesion.id);
  };

  const borrarChatImpl = (pjId: string, sesionId: string) => {
    const esActivo = chatActivo === sesionId && elegido?.id === pjId;
    if (esActivo) detenerConversacion();

    const encontrado = personajes.find((p) => p.id === pjId) ?? elegido;
    if (!encontrado) return;
    const pj: PersonajeInfo = encontrado;
    const lista = (chats[pjId] ?? []).filter((c) => c.id !== sesionId);
    setChats((c) => ({
      ...c,
      [pjId]: (c[pjId] ?? []).filter((c) => c.id !== sesionId),
    }));
    if (chatActivo === sesionId && elegido?.id === pjId) {
      if (lista.length > 0) {
        abrirChatImpl(pj, lista[lista.length - 1].id);
      } else {
        // si no quedan chats de este personaje, abrir el último de otro (o la bienvenida)
        const otro = personajes.find((p) => p.id !== pjId);
        const otraLista = otro ? (chats[otro.id] ?? []) : [];
        if (otro && otraLista.length > 0) {
          abrirChatImpl(otro, otraLista[otraLista.length - 1].id);
        } else {
          setElegido(null);
          setChatActivo(null);
          setMensajes([]);
        }
      }
    }
    postSilencioso('/api/stop');
  };

  // volver = cerrar esta conversación: se olvida cuál era el chat "activo"
  // del personaje, así al volver a entrar arranca uno NUEVO en vez de
  // retomar el que estabas mirando (ni el último de la lista).
  const volverImpl = () => {
    detenerConversacion();
    if (elegido) localStorage.removeItem(claveActual(usuarioId, elegido.id));
    setElegido(null);
    setChatActivo(null);
    setMensajes([]);
    setError('');
    setHablando(false);
    setMemoriaAbierta(false);
    postSilencioso('/api/stop');
  };

  // borra TODAS las conversaciones (desde Ajustes, con doble confirmación)
  const borrarTodosLosChatsImpl = () => {
    detenerConversacion();
    setChats({});
    setElegido(null);
    setChatActivo(null);
    setMensajes([]);
    setError('');
    setMemoriaAbierta(false);
    setAjustesAbiertos(false);
    personajes.forEach((pj) => localStorage.removeItem(claveActual(usuarioId, pj.id)));
    postSilencioso('/api/stop');
  };

  const totalChats = useMemo(
    () => Object.values(chats).reduce((total, lista) => total + lista.length, 0),
    [chats]
  );

  // el chat más reciente de cualquier personaje: el "continuar donde
  // dejaste" del hero (patrón moderno: retomar > empezar de cero)
  const ultimoChat = useMemo(() => {
    const todos = personajes.flatMap((pj) =>
      (chats[pj.id] ?? []).map((sesion) => ({ pj, sesion }))
    );
    todos.sort((a, b) => new Date(b.sesion.fecha).getTime() - new Date(a.sesion.fecha).getTime());
    return todos[0] ?? null;
  }, [chats, personajes]);

  const sesionActual = useMemo(
    () =>
      elegido && chatActivo
        ? (chats[elegido.id] ?? []).find((sesion) => sesion.id === chatActivo) ?? null
        : null,
    [chats, chatActivo, elegido]
  );

  const tituloConversacion = sesionActual
    ? tituloSinEmoji(sesionActual.titulo)
    : `Conversación con ${elegido?.nombre ?? 'Kira'}`;

  // Wrappers de identidad ESTABLE: las implementaciones se recrean en cada
  // render (dependen de chats/mensajes), pero los componentes memoizados no
  // deben re-renderizarse por eso. Los refs guardan la versión viva.
  const abrirChatRef = useRef(abrirChatImpl);
  abrirChatRef.current = abrirChatImpl;
  const nuevoChatRef = useRef(nuevoChatImpl);
  nuevoChatRef.current = nuevoChatImpl;
  const borrarChatRef = useRef(borrarChatImpl);
  borrarChatRef.current = borrarChatImpl;
  const volverRef = useRef(volverImpl);
  volverRef.current = volverImpl;
  const borrarTodosRef = useRef(borrarTodosLosChatsImpl);
  borrarTodosRef.current = borrarTodosLosChatsImpl;

  const abrirChat = useCallback(
    (pj: PersonajeInfo, sesionId: string) => abrirChatRef.current(pj, sesionId),
    []
  );
  const nuevoChat = useCallback((pj: PersonajeInfo) => nuevoChatRef.current(pj), []);
  const borrarChat = useCallback(
    (pjId: string, sesionId: string) => borrarChatRef.current(pjId, sesionId),
    []
  );
  const volver = useCallback(() => volverRef.current(), []);
  const borrarTodosLosChats = useCallback(() => borrarTodosRef.current(), []);

  return {
    abrirChat,
    nuevoChat,
    borrarChat,
    borrarTodosLosChats,
    volver,
    totalChats,
    ultimoChat,
    tituloConversacion,
  };
}
