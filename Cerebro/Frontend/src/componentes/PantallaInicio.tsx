import { ArrowRight, Menu, Plus } from 'lucide-react';
import KiraCara from './KiraCara';
import type { PersonajeInfo } from '../lib/api';
import type { ChatSesion } from '../lib/tipos';
import { horaLista, tituloSinEmoji } from '../lib/tipos';
import { saludoDelDia } from '../lib/ui-utils';

type Props = {
  cargandoPjs: boolean;
  personajes: PersonajeInfo[];
  ultimoChat: { pj: PersonajeInfo; sesion: ChatSesion } | null;
  sugerencias: string[];
  error: string;
  onAbrirSidebar: () => void;
  onAbrirChat: (pj: PersonajeInfo, sesionId: string) => void;
  onEmpezarNuevo: () => void;
  onSugerencia: (texto: string) => void;
};

/** Pantalla de bienvenida: la cara viva, "continuar" y sugerencias. */
export default function PantallaInicio({
  cargandoPjs,
  personajes,
  ultimoChat,
  sugerencias,
  error,
  onAbrirSidebar,
  onAbrirChat,
  onEmpezarNuevo,
  onSugerencia,
}: Props) {
  return (
    <div className="pantalla pantalla-inicio">
      <div className="pantalla-topbar">
        <button
          type="button"
          className="inicio-menu"
          onClick={onAbrirSidebar}
          aria-label="Abrir conversaciones"
        >
          <Menu size={18} strokeWidth={2} aria-hidden="true" />
          <span>Conversaciones</span>
        </button>
      </div>

      <header className="hero hero-kira">
        <KiraCara size={128} vivo mirarPuntero className="hero-cara" etiqueta="La cara de Kira" />
        <p className="hero-saludo">{saludoDelDia()}</p>
        <h1>Kira</h1>
        <p className="subtitulo">
          Una IA con voz y emociones que vive en una micro:bit. Hablale,
          escuchala y mirá cómo responde la carita.
        </p>

        {cargandoPjs ? (
          <div className="welcome-loading" role="status" aria-label="Preparando a Kira">
            <span />
            <span />
          </div>
        ) : (
          personajes[0] && (
            <>
              <div className="welcome-acciones">
                {ultimoChat ? (
                  <>
                    <button
                      type="button"
                      className="welcome-primary"
                      onClick={() => onAbrirChat(ultimoChat.pj, ultimoChat.sesion.id)}
                    >
                      <span>Continuar conversación</span>
                      <span className="welcome-primary-meta">
                        {tituloSinEmoji(ultimoChat.sesion.titulo)} · {horaLista(ultimoChat.sesion.fecha)}
                      </span>
                      <ArrowRight size={16} strokeWidth={2} aria-hidden="true" />
                    </button>
                    <button type="button" className="welcome-secondary" onClick={onEmpezarNuevo}>
                      <Plus size={15} strokeWidth={2} aria-hidden="true" />
                      Empezar de nuevo
                    </button>
                  </>
                ) : (
                  <button
                    type="button"
                    className="welcome-primary welcome-primary-simple"
                    onClick={onEmpezarNuevo}
                  >
                    Empezar a hablar
                    <ArrowRight size={16} strokeWidth={2} aria-hidden="true" />
                  </button>
                )}
              </div>

              <div className="sugerencias welcome-suggestions" aria-label="Sugerencias para empezar">
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
            </>
          )
        )}
      </header>

      {error && <p className="error" role="status">{error}</p>}
    </div>
  );
}
