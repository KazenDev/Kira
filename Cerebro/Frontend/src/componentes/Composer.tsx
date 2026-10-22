import { memo } from 'react';
import type { ChangeEvent, CSSProperties, RefObject } from 'react';
import { ArrowUp, Camera, Check, Mic, Square, X } from 'lucide-react';

const FACTORES_BARRAS = [0.5, 0.8, 1.1, 0.9, 1.2, 0.7, 1.0, 1.3, 0.9, 1.15, 0.7, 1.0, 1.2, 0.8, 0.95, 0.55];

function fmtReloj(segundos: number): string {
  const m = Math.floor(segundos / 60);
  const s = Math.floor(segundos % 60);
  return `${m}:${String(s).padStart(2, '0')}`;
}

type Props = {
  nombre: string;
  input: string;
  onInput: (valor: string) => void;
  foto: string | null;
  onQuitarFoto: () => void;
  procesandoFoto: boolean;
  respuestaActiva: boolean;
  textareaRef: RefObject<HTMLTextAreaElement>;
  fotoInputRef: RefObject<HTMLInputElement>;
  onElegirFoto: (evento: ChangeEvent<HTMLInputElement>) => void;
  onEnviar: () => void;
  onDetener: () => void;
  onEscuchar: () => void;
  escuchando: boolean;
  nivelEscucha: number;
  segEscucha: number;
  onCancelarEscucha: () => void;
  onEnviarEscucha: () => void;
  mbConectado: boolean;
  rutaCel: boolean;
};

/** Composer del chat: texto, foto, micrófono y la píldora de grabación. */
function ComposerInner({
  nombre,
  input,
  onInput,
  foto,
  onQuitarFoto,
  procesandoFoto,
  respuestaActiva,
  textareaRef,
  fotoInputRef,
  onElegirFoto,
  onEnviar,
  onDetener,
  onEscuchar,
  escuchando,
  nivelEscucha,
  segEscucha,
  onCancelarEscucha,
  onEnviarEscucha,
  mbConectado,
  rutaCel,
}: Props) {
  if (escuchando) {
    return (
      <footer className="input-bar">
        <div
          className="orbe-escucha"
          style={
            {
              '--nivel': nivelEscucha,
              '--progreso': `${Math.min(100, (segEscucha / 60) * 100)}%`,
            } as CSSProperties
          }
          role="group"
          aria-label="Kira está escuchando"
        >
          <button
            type="button"
            className="orbe-accion orbe-cancelar"
            onClick={(e) => {
              e.stopPropagation();
              onCancelarEscucha();
            }}
            title="Cancelar la grabación (Esc)"
            aria-label="Cancelar la grabación"
          >
            <X size={16} strokeWidth={2.4} aria-hidden="true" />
          </button>
          <span className="orbe-estado" aria-hidden="true">
            <span className="orbe-rec" />
            <span className="orbe-tiempo">{fmtReloj(segEscucha)}</span>
          </span>
          <div className="orbe-barras" aria-hidden="true">
            {FACTORES_BARRAS.map((f, i) => (
              <i
                key={i}
                style={{ '--f': f, animationDelay: `${i * 0.07}s` } as CSSProperties}
              />
            ))}
          </div>
          <span className="orbe-hint">
            {mbConectado ? 'hablá… te escucha' : 'hablá…'}
          </span>
          {!mbConectado && (
            <button
              type="button"
              className="orbe-accion orbe-enviar"
              onClick={(e) => {
                e.stopPropagation();
                onEnviarEscucha();
              }}
              title="Cortar y enviar"
              aria-label="Cortar la grabación y enviar"
            >
              <Check size={17} strokeWidth={2.6} aria-hidden="true" />
            </button>
          )}
          {rutaCel && <span className="orbe-progreso" aria-hidden="true" />}
        </div>
      </footer>
    );
  }

  return (
    <footer className="input-bar">
      <div className="composer">
        <input
          ref={fotoInputRef}
          className="foto-input"
          type="file"
          accept="image/*"
          capture="environment"
          onChange={onElegirFoto}
          aria-hidden="true"
          tabIndex={-1}
        />
        {foto && (
          <div className="foto-chip">
            <img src={foto} alt="Foto lista para enviar" />
            <span className="foto-chip-texto">foto lista para enviar</span>
            <button
              type="button"
              className="foto-chip-quitar"
              onClick={onQuitarFoto}
              title="Quitar la foto"
              aria-label="Quitar la foto"
            >
              <X size={14} strokeWidth={2.4} aria-hidden="true" />
            </button>
          </div>
        )}
        <textarea
          ref={textareaRef}
          value={input}
          onChange={(e) => onInput(e.target.value)}
          onKeyDown={(e) => {
            if (e.key === 'Enter' && !e.shiftKey) {
              e.preventDefault();
              onEnviar();
            }
          }}
          placeholder={`Escribile a ${nombre}...`}
          maxLength={300}
          rows={1}
          autoFocus
        />
        <div className="composer-acciones">
          <button
            type="button"
            className={'icon-btn' + (foto ? ' activo' : '')}
            onClick={() => fotoInputRef.current?.click()}
            disabled={respuestaActiva || procesandoFoto}
            title="Sacá una foto y mandásela: la mira de verdad"
            aria-label="Sacar una foto"
          >
            {procesandoFoto ? (
              <span className="foto-spinner" aria-hidden="true" />
            ) : (
              <Camera size={20} strokeWidth={2} aria-hidden="true" />
            )}
          </button>
          <button
            type="button"
            className="icon-btn mic-btn"
            onClick={onEscuchar}
            disabled={respuestaActiva}
            title="Hablale: el micro:bit escucha y Kira responde"
          >
            <Mic size={20} strokeWidth={2} aria-hidden="true" />
          </button>
          <span className="composer-espacio" aria-hidden="true" />
          <span
            className={'char-count' + (input.length > 250 ? ' visible' : '')}
            aria-hidden="true"
          >
            {300 - input.length}
          </span>
          {respuestaActiva ? (
            <button
              type="button"
              className="send-btn stop"
              onClick={onDetener}
              title="Cortar la respuesta (Esc)"
              aria-label="Cortar la respuesta"
            >
              <Square size={14} strokeWidth={2.6} fill="currentColor" aria-hidden="true" />
            </button>
          ) : (
            <button
              type="button"
              className="send-btn"
              onClick={onEnviar}
              disabled={!input.trim() && !foto}
              title="Enviar (Enter · Shift+Enter salto de línea)"
            >
              <ArrowUp size={20} strokeWidth={2.4} aria-hidden="true" />
            </button>
          )}
        </div>
      </div>
    </footer>
  );
}

// memo: durante el streaming el composer no cambia (solo el texto de Kira).
const Composer = memo(ComposerInner);
export default Composer;
