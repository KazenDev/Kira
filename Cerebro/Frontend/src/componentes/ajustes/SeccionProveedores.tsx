import { Check, Cpu, RefreshCw, Sparkles } from 'lucide-react';
import type { EstadoProveedores, ProveedorIA } from '../../lib/api';

type Props = {
  estado: EstadoProveedores | null;
  cargando: boolean;
  error: string;
  cambiando: string | null;
  onRefrescar: () => void;
  onElegir: (nombre: string) => void;
};

/** Un proveedor no puede atender por tres motivos distintos; conviene decirlos
 *  distinto en vez de un genérico "no disponible". */
function motivo(p: ProveedorIA): { texto: string; tono: string } | null {
  if (!p.tiene_key) return { texto: 'sin clave', tono: 'apagado' };
  if (p.enfriando_s > 0) {
    return { texto: `enfriando ${Math.ceil(p.enfriando_s)} s`, tono: 'aviso' };
  }
  return null;
}

export function SeccionProveedores({
  estado,
  cargando,
  error,
  cambiando,
  onRefrescar,
  onElegir,
}: Props) {
  const lista = estado?.proveedores ?? [];

  return (
    <section className="ajustes-seccion">
      <div className="ajustes-titulo">
        <Cpu size={16} strokeWidth={2.2} aria-hidden="true" />
        <h3>Proveedores</h3>
        <button
          type="button"
          className="ajustes-mini-boton"
          onClick={onRefrescar}
          disabled={cargando}
          title="Volver a leer el estado"
          aria-label="Refrescar el estado de los proveedores"
        >
          <RefreshCw size={14} aria-hidden="true" className={cargando ? 'girando' : ''} />
        </button>
      </div>

      <p className="ajustes-nota">
        Quién piensa las respuestas. Si el que elegís falla o se queda sin saldo, Kira
        cae sola al siguiente de la lista: no se queda muda.
      </p>

      {error && <p className="ajustes-nota error">{error}</p>}

      <button
        type="button"
        className={'ajustes-fila' + (estado?.automatico ? ' activa' : '')}
        onClick={() => onElegir('')}
        disabled={cambiando !== null}
      >
        <span className="ajustes-fila-ico">
          {estado?.automatico ? (
            <Check size={18} aria-hidden="true" />
          ) : (
            <Sparkles size={18} aria-hidden="true" />
          )}
        </span>
        <span className="ajustes-fila-texto">
          <strong>Automático</strong>
          <small>
            El primero que pueda atender
            {estado?.automatico && estado.atendiendo ? ` · ahora: ${estado.atendiendo}` : ''}
          </small>
        </span>
      </button>

      {lista.map((p) => {
        const fallo = motivo(p);
        const ocupado = cambiando === p.nombre;
        return (
          <button
            key={p.nombre}
            type="button"
            className={'ajustes-fila' + (p.elegido ? ' activa' : '')}
            onClick={() => onElegir(p.nombre)}
            disabled={!p.tiene_key || cambiando !== null}
            title={p.tiene_key ? p.base_url : 'Falta la clave en el .env del backend'}
          >
            <span className="ajustes-fila-ico">
              {ocupado ? (
                <RefreshCw size={16} aria-hidden="true" className="girando" />
              ) : p.elegido ? (
                <Check size={18} aria-hidden="true" />
              ) : (
                <Cpu size={18} aria-hidden="true" />
              )}
            </span>
            <span className="ajustes-fila-texto">
              <strong>
                {p.nombre}
                {fallo && <em className={`proveedor-marca ${fallo.tono}`}> · {fallo.texto}</em>}
              </strong>
              <small>{p.modelo}</small>
            </span>
          </button>
        );
      })}

      {!cargando && lista.length === 0 && !error && (
        <p className="ajustes-nota">No hay proveedores configurados.</p>
      )}
    </section>
  );
}
