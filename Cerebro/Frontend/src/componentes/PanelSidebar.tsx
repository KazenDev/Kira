import { memo } from 'react';
// Barra lateral de conversaciones: el drawer, su fondo y el estado del micro.
import type { Dispatch, SetStateAction } from 'react';
import type { ChatsPorPersonaje } from '../lib/almacenamiento';
import type { PersonajeInfo } from '../lib/api';
import Sidebar from './Sidebar';

type Props = {
  personajes: PersonajeInfo[];
  chats: ChatsPorPersonaje;
  elegido: PersonajeInfo | null;
  chatActivo: string | null;
  mbEstado: string;
  mbTexto: string;
  mbTitulo: string;
  abierta: boolean;
  colapsada: boolean;
  setColapsada: Dispatch<SetStateAction<boolean>>;
  onAbrirChat: (pj: PersonajeInfo, sesionId: string) => void;
  onBorrarChat: (pjId: string, sesionId: string) => void;
  onNuevoChat: (pj: PersonajeInfo) => void;
  onSincronizar: () => void;
  onAbrirAjustes: () => void;
  onVolver: () => void;
  onCerrar: () => void;
};

/** Drawer de conversaciones + fondo oscuro para cerrarlo en móvil. */
function PanelSidebarInner({
  personajes,
  chats,
  elegido,
  chatActivo,
  mbEstado,
  mbTexto,
  mbTitulo,
  abierta,
  colapsada,
  setColapsada,
  onAbrirChat,
  onBorrarChat,
  onNuevoChat,
  onSincronizar,
  onAbrirAjustes,
  onVolver,
  onCerrar,
}: Props) {
  return (
    <>
      <aside
        aria-label="Conversaciones"
        aria-modal={abierta ? true : undefined}
        role={abierta ? 'dialog' : undefined}
        className={'sidebar' + (abierta ? ' abierta' : '') + (colapsada ? ' colapsada' : '')}
      >
        <Sidebar
          personajes={personajes}
          chats={chats}
          elegido={elegido}
          chatActivo={chatActivo}
          mbEstado={mbEstado}
          mbTexto={mbTexto}
          mbTitulo={mbTitulo}
          colapsada={colapsada}
          onToggleColapsada={() => setColapsada((v) => !v)}
          onAbrirChat={onAbrirChat}
          onBorrarChat={onBorrarChat}
          onNuevoChat={onNuevoChat}
          onSincronizar={onSincronizar}
          onAbrirAjustes={onAbrirAjustes}
          onVolver={onVolver}
          onCerrar={onCerrar}
        />
      </aside>
      {abierta && (
        <button
          type="button"
          className="sidebar-fondo"
          aria-label="Cerrar conversaciones"
          onClick={onCerrar}
        />
      )}
    </>
  );
}

// memo: no cambia mientras se revela el texto de un mensaje.
const PanelSidebar = memo(PanelSidebarInner);
export default PanelSidebar;
