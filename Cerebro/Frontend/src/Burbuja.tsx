// Mensajes del chat en formato "turn": asistente sin burbuja y usuario en una
// superficie sutil. Las acciones secundarias viven en un único menú para que la
// conversación no se llene de iconos repetidos.
import { memo, useEffect, useRef, useState } from 'react';
import { motion } from 'motion/react';
import { SPRING_MSG } from './motion';
import {
  Check,
  Copy,
  ExternalLink,
  Globe,
  MoreHorizontal,
  RefreshCw,
  Volume2,
} from 'lucide-react';
import { MensajeChat } from './api';
import Avatar from './Avatar';
import MensajeMarkdown from './MensajeMarkdown';

interface Props {
  m: MensajeChat;
  personajeId: string;
  color: string;
  parcial?: boolean;
  onRegenerar?: (id: string) => void;
  onEscuchar?: () => void;
}

function BurbujaInner({
  m,
  personajeId,
  color,
  parcial = false,
  onRegenerar,
  onEscuchar,
}: Props) {
  const [copiado, setCopiado] = useState(false);
  const [accionesAbiertas, setAccionesAbiertas] = useState(false);
  const accionesRef = useRef<HTMLDivElement | null>(null);
  const accionesButtonRef = useRef<HTMLButtonElement | null>(null);

  useEffect(() => {
    if (!accionesAbiertas) return;

    const cerrarFuera = (event: PointerEvent) => {
      if (!accionesRef.current?.contains(event.target as Node)) {
        setAccionesAbiertas(false);
      }
    };
    const cerrarEscape = (event: KeyboardEvent) => {
      if (event.key !== 'Escape') return;
      event.preventDefault();
      event.stopImmediatePropagation();
      setAccionesAbiertas(false);
      window.setTimeout(() => accionesButtonRef.current?.focus(), 0);
    };

    document.addEventListener('pointerdown', cerrarFuera);
    document.addEventListener('keydown', cerrarEscape);
    return () => {
      document.removeEventListener('pointerdown', cerrarFuera);
      document.removeEventListener('keydown', cerrarEscape);
    };
  }, [accionesAbiertas]);

  if (m.rol === 'usuario') {
    return (
      <motion.div
        className="user-msg"
        initial={{ opacity: 0, y: 6, scale: 0.97 }}
        animate={{ opacity: 1, y: 0, scale: 1 }}
        transition={SPRING_MSG}
      >
        <div className={'user-bubble' + (m.imagen ? ' con-foto' : '')}>
          {m.imagen && (
            <img
              className="user-foto"
              src={m.imagen}
              alt="Foto que enviaste"
              loading="lazy"
              decoding="async"
            />
          )}
          {m.contenido.trim() ? <MensajeMarkdown contenido={m.contenido} /> : null}
        </div>
      </motion.div>
    );
  }

  if (m.rol === personajeId) {
    const copiar = async () => {
      try {
        await navigator.clipboard.writeText(m.contenido);
        setCopiado(true);
        setAccionesAbiertas(false);
        setTimeout(() => setCopiado(false), 1600);
      } catch {
        /* clipboard no disponible */
      }
    };

    const hayAudio = Boolean(m.audio_url && onEscuchar);

    return (
      <div
        className="turn"
        style={{ '--color': color } as React.CSSProperties}
        title={`Mensaje de las ${m.hora}`}
      >
        <div className="turn-avatar" style={{ background: color }}>
          <Avatar color="#fff" size={18} />
        </div>
        <div className="turn-cuerpo">
          {parcial ? (
            <p className="parcial">
              {m.contenido}
              <span className="cursor" aria-hidden="true" />
            </p>
          ) : (
            <MensajeMarkdown contenido={m.contenido} />
          )}

          {!parcial && m.fuentes && m.fuentes.length > 0 && (
            <div className="fuentes">
              <span className="fuentes-titulo">
                <Globe size={12} strokeWidth={2} aria-hidden="true" />
                Fuentes
              </span>
              {m.fuentes.slice(0, 4).map((f, i) => (
                <a
                  key={f.url + i}
                  className="fuente"
                  href={f.url}
                  target="_blank"
                  rel="noopener noreferrer"
                  title={f.url}
                >
                  <span className="fuente-n">{i + 1}</span>
                  <span className="fuente-titulo-texto">{f.titulo}</span>
                  <ExternalLink size={12} strokeWidth={2} aria-hidden="true" />
                </a>
              ))}
            </div>
          )}

          <div className="turn-pie">
            {parcial ? (
              <span className="escribiendo-estado">escribiendo…</span>
            ) : (
              <div className="msg-acciones-wrap" ref={accionesRef}>
                <button
                  ref={accionesButtonRef}
                  type="button"
                  className="msg-accion msg-acciones-disparador"
                  onClick={() => setAccionesAbiertas((abierto) => !abierto)}
                  title="Acciones del mensaje"
                  aria-label="Acciones del mensaje"
                  aria-expanded={accionesAbiertas}
                  aria-haspopup="true"
                >
                  {copiado ? (
                    <Check size={14} strokeWidth={2} aria-hidden="true" />
                  ) : (
                    <MoreHorizontal size={15} strokeWidth={2} aria-hidden="true" />
                  )}
                </button>

                {accionesAbiertas && (
                  <div className="msg-acciones-menu" role="dialog" aria-label="Acciones del mensaje">
                    {hayAudio && (
                      <button
                        type="button"
                        className="msg-accion-menu-item"
                        onClick={() => {
                          setAccionesAbiertas(false);
                          onEscuchar?.();
                        }}
                      >
                        <Volume2 size={15} strokeWidth={2} aria-hidden="true" />
                        <span>Escuchar de nuevo</span>
                      </button>
                    )}
                    <button type="button" className="msg-accion-menu-item" onClick={copiar}>
                      <Copy size={15} strokeWidth={2} aria-hidden="true" />
                      <span>{copiado ? 'Copiado' : 'Copiar'}</span>
                    </button>
                    {onRegenerar && (
                      <button
                        type="button"
                        className="msg-accion-menu-item"
                        onClick={() => {
                          setAccionesAbiertas(false);
                          onRegenerar(m.id);
                        }}
                      >
                        <RefreshCw size={15} strokeWidth={2} aria-hidden="true" />
                        <span>Generar otra respuesta</span>
                      </button>
                    )}
                  </div>
                )}
              </div>
            )}
          </div>
        </div>
      </div>
    );
  }

  return null;
}

const Burbuja = memo(BurbujaInner);
export default Burbuja;
