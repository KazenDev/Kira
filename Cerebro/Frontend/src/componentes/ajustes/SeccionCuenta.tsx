import { Info, LogOut } from 'lucide-react';
import type { UsuarioKira } from '../../lib/api';

type Props = {
  usuario: UsuarioKira;
  confirmandoSalir: boolean;
  onCerrarSesion: () => void;
};

export function SeccionCuenta({ usuario, confirmandoSalir, onCerrarSesion }: Props) {
  return (
    <>
      <section className="ajustes-seccion">
        <div className="ajustes-titulo">
          <LogOut size={16} strokeWidth={2.2} aria-hidden="true" />
          <h3>Cuenta</h3>
        </div>
        <div className="ajustes-estado-card">
          <div>
            <strong>{usuario.username}</strong>
            <small>Sesión activa · tus datos están separados</small>
          </div>
        </div>
        <button
          type="button"
          className={'ajustes-fila peligro' + (confirmandoSalir ? ' confirmando' : '')}
          onClick={onCerrarSesion}
        >
          <span className="ajustes-fila-ico">
            <LogOut size={18} aria-hidden="true" />
          </span>
          <span className="ajustes-fila-texto">
            <strong>{confirmandoSalir ? '¿Cerrar esta sesión?' : 'Cerrar sesión'}</strong>
            <small>La memoria de esta cuenta queda guardada</small>
          </span>
        </button>
      </section>

      <section className="ajustes-seccion">
        <div className="ajustes-titulo">
          <Info size={16} strokeWidth={2.2} aria-hidden="true" />
          <h3>Acerca de</h3>
        </div>
        <div className="ajustes-acerca">
          <p>
            <strong>Kira</strong> — una IA con voz y emociones que vive en una micro:bit.
          </p>
          <p>
            Su personalidad no está escrita de antemano: se construye con cada charla, recuerdo y
            opinión que Kira va formando.
          </p>
          <p className="ajustes-feria">Hecho para la feria de ciencias.</p>
        </div>
      </section>
    </>
  );
}
