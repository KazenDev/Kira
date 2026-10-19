// Mensajes del chat en formato "turn": asistente sin burbuja y usuario en una
// superficie sutil. Las acciones secundarias viven en un único menú para que la
// conversación no se llene de iconos repetidos.
import { memo, useState } from 'react';

/** Un glifo por clase de origen, para que el ojo escanee la fila sin leerla.
 *  Texto plano a propósito: son 9 caracteres y meter 9 iconos SVG por el
 *  lucide para esto agranda el bundle por nada. */
const ICONO_RASTRO: Record<string, string> = {
  web: '🌐',
  sensor: '⚙',
  calculo: 'ƒ',
  memoria: '☰',
  estado: '◐',
  sistema: '◷',
  accion: '♪',
  criada: '✦',
  otro: '•',
};

/** "clima · pronóstico" o "lo sabía" (cuando no consultó nada). */
function resumenRastro(rastro: NonNullable<MensajeChat['rastro']>): string {
  const consultas = rastro.filter((r) => r.tipo !== 'criada');
  if (!consultas.length) return 'lo sabía';
  const nombres = consultas.slice(0, 2).map((r) => r.etiqueta.charAt(0).toLowerCase() + r.etiqueta.slice(1));
  return nombres.join(' · ') + (consultas.length > 2 ? ` +${consultas.length - 2}` : '');
}

import { motion } from 'motion/react';
import { SPRING_MSG } from './motion';
import {
  Check,
  Copy,
  ExternalLink,
  Eye,
  Globe,
  RefreshCw,
  Volume2,
} from 'lucide-react';
import { MensajeChat } from './api';
import KiraCara from './KiraCara';
import MensajeMarkdown from './MensajeMarkdown';

interface Props {
  m: MensajeChat;
  personajeId: string;
  color: string;
  parcial?: boolean;
  /** el ultimo mensaje de Kira: sus acciones se ven siempre (como ChatGPT) */
  ultimo?: boolean;
  onRegenerar?: (id: string) => void;
  onEscuchar?: () => void;
}

function BurbujaInner({
  m,
  personajeId,
  color,
  parcial = false,
  ultimo = false,
  onRegenerar,
  onEscuchar,
}: Props) {
  const [copiado, setCopiado] = useState(false);
  const [rastroAbierto, setRastroAbierto] = useState(false);

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
        setTimeout(() => setCopiado(false), 1600);
      } catch {
        /* clipboard no disponible */
      }
    };

    const hayAudio = Boolean(m.audio_url && onEscuchar);

    return (
      <div
        className={'turn' + (ultimo ? ' ultimo' : '')}
        style={{ '--color': color } as React.CSSProperties}
        title={`Mensaje de las ${m.hora}`}
      >
        <div className="turn-avatar">
          <KiraCara size={32} emocion={m.emotion ?? 'happy'} etiqueta="Kira" />
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
              {m.fuentes.slice(0, 4).map((f, i) => (
                <a
                  key={f.url + i}
                  className="fuente"
                  href={f.url}
                  target="_blank"
                  rel="noopener noreferrer"
                  title={f.url}
                >
                  <Globe size={12} strokeWidth={2} aria-hidden="true" />
                  <span className="fuente-titulo-texto">{f.titulo}</span>
                  <ExternalLink size={11} strokeWidth={2} aria-hidden="true" />
                </a>
              ))}
            </div>
          )}

          {/* DE DONDE SALIO ESTO: el rastro sale SIEMPRE (tambien cuando no
              consulto nada: "no consulto nada" es una respuesta, una lista
              vacia seria ambigua). Ver app/services/rastro.py. Va plegado en
              la misma fila de acciones para no gritar en cada mensaje. */}
          {rastroAbierto && m.rastro && m.rastro.length > 0 && (
            <ul className="rastro-lista" id={`rastro-${m.id}`}>
              {m.rastro.map((r, i) => (
                <li key={r.herramienta + i} className={`rastro-item rastro-${r.tipo}`}>
                  <span className="rastro-ic" aria-hidden="true">{ICONO_RASTRO[r.tipo] ?? '•'}</span>
                  <span className="rastro-txt">
                    <strong>{r.etiqueta}</strong>
                    {r.detalle ? <span className="rastro-detalle"> — {r.detalle}</span> : null}
                    {r.valor ? <span className="rastro-valor"> · {r.valor}</span> : null}
                  </span>
                </li>
              ))}
            </ul>
          )}

          <div className="turn-pie">
            {parcial ? (
              <span className="escribiendo-estado">
                <span className="onda-voz" aria-hidden="true"><i /><i /><i /></span>
                hablando…
              </span>
            ) : (
              <div className="msg-acciones" role="toolbar" aria-label="Acciones del mensaje">
                {hayAudio && (
                  <button type="button" className="msg-accion" onClick={onEscuchar}
                    title="Escuchar de nuevo" aria-label="Escuchar de nuevo">
                    <Volume2 size={15} strokeWidth={2} aria-hidden="true" />
                  </button>
                )}
                <button type="button" className="msg-accion" onClick={copiar}
                  title={copiado ? 'Copiado' : 'Copiar'} aria-label={copiado ? 'Copiado' : 'Copiar'}>
                  {copiado ? (
                    <Check size={15} strokeWidth={2.2} aria-hidden="true" />
                  ) : (
                    <Copy size={15} strokeWidth={2} aria-hidden="true" />
                  )}
                </button>
                {onRegenerar && (
                  <button type="button" className="msg-accion" onClick={() => onRegenerar(m.id)}
                    title="Otra respuesta" aria-label="Generar otra respuesta">
                    <RefreshCw size={15} strokeWidth={2} aria-hidden="true" />
                  </button>
                )}
                {m.rastro && m.rastro.length > 0 && (
                  <button
                    type="button"
                    className={'msg-rastro' + (rastroAbierto ? ' abierto' : '')}
                    onClick={() => setRastroAbierto((v) => !v)}
                    aria-expanded={rastroAbierto}
                    aria-controls={`rastro-${m.id}`}
                    title="De dónde salió esto"
                  >
                    <Eye size={13} strokeWidth={2} aria-hidden="true" />
                    {resumenRastro(m.rastro)}
                  </button>
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
