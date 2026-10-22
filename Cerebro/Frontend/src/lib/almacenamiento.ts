// Persistencia local de las conversaciones (una lista por personaje).
// localStorage puede estar lleno o bloqueado: en ese caso la app sigue
// funcionando en memoria y no rompe la charla.
import type { ChatSesion } from './tipos';

export type ChatsPorPersonaje = Record<string, ChatSesion[]>;

const STORAGE_CHATS = 'kira_chats_v2';

export const claveActual = (userId: string, personajeId: string) =>
  `kira_chat_activo_${userId}_${personajeId}`;

export function cargarChats(userId: string): ChatsPorPersonaje {
  try {
    const raw = localStorage.getItem(`${STORAGE_CHATS}_${userId}`);
    if (!raw) return {};
    const parsed = JSON.parse(raw);
    return parsed && typeof parsed === 'object' && !Array.isArray(parsed) ? parsed : {};
  } catch {
    return {};
  }
}

export function guardarChats(userId: string, chats: ChatsPorPersonaje): void {
  try {
    localStorage.setItem(`${STORAGE_CHATS}_${userId}`, JSON.stringify(chats));
  } catch {
    // Si localStorage está lleno/bloqueado, el chat sigue funcionando en memoria.
  }
}
