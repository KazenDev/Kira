import type { ReactNode } from 'react';

export function Interruptor({
  activo,
  onClick,
  etiqueta,
  descripcion,
  icono,
  disabled = false,
}: {
  activo: boolean;
  onClick: () => void;
  etiqueta: string;
  descripcion: string;
  icono: ReactNode;
  disabled?: boolean;
}) {
  return (
    <button
      type="button"
      className={'ajustes-fila' + (activo ? ' activa' : '') + (disabled ? ' subtoggle' : '')}
      onClick={onClick}
      role="switch"
      aria-checked={activo}
      aria-label={etiqueta}
      disabled={disabled}
    >
      <span className="ajustes-fila-ico">{icono}</span>
      <span className="ajustes-fila-texto">
        <strong>{etiqueta}</strong>
        <small>{descripcion}</small>
      </span>
      <span className={'toggle' + (activo ? ' on' : '')} aria-hidden="true">
        <span className="toggle-bola" />
      </span>
    </button>
  );
}
