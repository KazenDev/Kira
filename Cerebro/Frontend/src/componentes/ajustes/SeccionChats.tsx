import { Check, Trash2 } from 'lucide-react';

type Props = {
  totalChats: number;
  confirmandoChats: boolean;
  onBorrarChats: () => void;
};

export function SeccionChats({ totalChats, confirmandoChats, onBorrarChats }: Props) {
  return (
    <section className="ajustes-seccion">
      <div className="ajustes-titulo">
        <Trash2 size={16} strokeWidth={2.2} aria-hidden="true" />
        <h3>Chats</h3>
      </div>
      <p className="ajustes-nota">
        Los chats se guardan en este navegador. Borrarlos no borra la memoria de Kira.
      </p>
      <button
        type="button"
        className={'ajustes-fila peligro' + (confirmandoChats ? ' confirmando' : '')}
        onClick={onBorrarChats}
        disabled={totalChats === 0}
      >
        <span className="ajustes-fila-ico">
          {confirmandoChats ? (
            <Check size={18} aria-hidden="true" />
          ) : (
            <Trash2 size={18} aria-hidden="true" />
          )}
        </span>
        <span className="ajustes-fila-texto">
          <strong>
            {confirmandoChats ? '¿Seguro? Toque de nuevo para borrar todo' : 'Borrar todos los chats'}
          </strong>
          <small>
            {totalChats === 0
              ? 'No hay conversaciones para borrar'
              : `${totalChats} conversación${totalChats !== 1 ? 'es' : ''} se eliminarán de este equipo`}
          </small>
        </span>
      </button>
    </section>
  );
}
