import { memo } from 'react';
import type { CSSProperties, RefObject } from 'react';
import { ArrowRight } from 'lucide-react';
import { AnimatePresence, motion } from 'motion/react';
import Burbuja from './Burbuja';
import KiraCara from './KiraCara';
import type { MensajeChat } from '../lib/api';
import { T, EASE_OUT } from '../lib/motion';

type Props = {
  mensajes: MensajeChat[];
  personajeId: string;
  nombre: string;
  color: string;
  streamId: string | null;
  mostrarPuntos: boolean;
  ultimoKiraId?: string;
  respuestaActiva: boolean;
  vozActiva: boolean;
  hablando: boolean;
  labelTrace: string | null;
  sugerencias: string[];
  saludoPantalla: string;
  hayMensajesUsuario: boolean;
  inputVacio: boolean;
  error: string;
  ultimoTexto: string;
  contenedorRef: RefObject<HTMLElement>;
  onScroll: () => void;
  onRegenerar: (id: string) => void;
  onEscuchar: (urls: string[]) => void;
  onSugerencia: (texto: string) => void;
  onReintentar: () => void;
};

/** Lista de mensajes + estados vacíos/indicadores/error del chat. */
function ListaMensajesInner({
  mensajes,
  personajeId,
  nombre,
  color,
  streamId,
  mostrarPuntos,
  ultimoKiraId,
  respuestaActiva,
  vozActiva,
  hablando,
  labelTrace,
  sugerencias,
  saludoPantalla,
  hayMensajesUsuario,
  inputVacio,
  error,
  ultimoTexto,
  contenedorRef,
  onScroll,
  onRegenerar,
  onEscuchar,
  onSugerencia,
  onReintentar,
}: Props) {
  return (
    <section
      className="mensajes"
      ref={contenedorRef}
      onScroll={onScroll}
      aria-busy={respuestaActiva}
    >
      {mensajes.map((m) => {
        // El parcial nace VACÍO: mientras tanto lo representa el indicador
        // de puntitos. Dibujarlo duplicaba los dos indicadores a la vez.
        if (mostrarPuntos && m.id === streamId) return null;
        return (
          <Burbuja
            key={m.id}
            m={m}
            personajeId={personajeId}
            color={color}
            parcial={streamId === m.id}
            ultimo={m.id === ultimoKiraId}
            onRegenerar={
              m.rol !== 'usuario' && !respuestaActiva ? onRegenerar : undefined
            }
            onEscuchar={
              m.rol !== 'usuario' && vozActiva && !respuestaActiva && !hablando
                ? onEscuchar
                : undefined
            }
          />
        );
      })}

      {/* chat vacío: la cara viva de Kira, su saludo y sugerencias */}
      {mensajes.length === 0 && !respuestaActiva && (
        <div className="chat-vacio">
          <KiraCara size={104} vivo mirarPuntero etiqueta="La cara de Kira" />
          <p className="chat-vacio-saludo">{saludoPantalla}</p>
        </div>
      )}
      {!hayMensajesUsuario && !respuestaActiva && inputVacio && (
        <div className="sugerencias">
          {sugerencias.map((s) => (
            <button
              key={s}
              type="button"
              className="sugerencia"
              onClick={() => onSugerencia(s)}
            >
              <span>{s}</span>
              <ArrowRight size={14} strokeWidth={2} aria-hidden="true" />
            </button>
          ))}
        </div>
      )}

      <AnimatePresence>
        {mostrarPuntos && (
          <motion.div
            className="typing"
            style={{ '--color': color } as CSSProperties}
            initial={{ opacity: 0, y: 4 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -4 }}
            transition={{ duration: T.fast, ease: EASE_OUT }}
            role="status"
            aria-live="polite"
          >
            <div className="typing-avatar">
              <KiraCara size={32} estado="piensa" />
            </div>
            <span className="typing-texto">
              <span className="typing-dots" aria-hidden="true">
                <span />
                <span />
                <span />
              </span>
              <motion.span
                key={labelTrace ?? 'pensando'}
                initial={{ opacity: 0, y: 2 }}
                animate={{ opacity: 1, y: 0 }}
                transition={{ duration: T.fast, ease: EASE_OUT }}
              >
                {labelTrace ? `${nombre} está ${labelTrace}` : `${nombre} está pensando…`}
              </motion.span>
            </span>
          </motion.div>
        )}
      </AnimatePresence>
      {error && (
        <p className="error" role="alert">
          {error}{' '}
          {ultimoTexto && !respuestaActiva && (
            <button className="error-reintentar" onClick={onReintentar}>
              reintentar
            </button>
          )}
        </p>
      )}
    </section>
  );
}

// memo: solo cambia cuando cambian los mensajes o los estados de la charla.
const ListaMensajes = memo(ListaMensajesInner);
export default ListaMensajes;
