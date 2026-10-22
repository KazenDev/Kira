import { Activity, Brain, Check, Download, Eye, ShieldCheck, Trash2 } from 'lucide-react';
import type { EstadoMemoria, PreferenciasMemoria } from '../../lib/api';
import { Interruptor } from './Interruptor';

type Props = {
  preferenciasMemoria: PreferenciasMemoria;
  onCambiarPreferenciasMemoria: (v: PreferenciasMemoria) => void;
  memoria: EstadoMemoria | null;
  cargandoDiagnostico: boolean;
  borrandoMemoria: boolean;
  confirmandoMemoria: boolean;
  onAbrirMemoria: () => void;
  onExportarMemoria: () => void;
  onBorrarMemoria: () => void;
};

export function SeccionMemoria({
  preferenciasMemoria,
  onCambiarPreferenciasMemoria,
  memoria,
  cargandoDiagnostico,
  borrandoMemoria,
  confirmandoMemoria,
  onAbrirMemoria,
  onExportarMemoria,
  onBorrarMemoria,
}: Props) {
  const totalActivos = memoria?.total_activos ?? memoria?.total_recuerdos ?? 0;
  const totalHechos = memoria?.total_hechos ?? 0;
  const totalExperiencias = memoria?.total_experiencias ?? 0;
  const totalReflexiones = memoria?.total_reflexiones ?? 0;
  const memoriaMaestra = preferenciasMemoria.recordar;

  return (
    <section className="ajustes-seccion">
      <div className="ajustes-titulo">
        <Brain size={16} strokeWidth={2.2} aria-hidden="true" />
        <h3>Memoria y privacidad</h3>
      </div>
      <p className="ajustes-nota">
        La memoria y el índice viven localmente; las conversaciones pueden pasar por los
        proveedores de IA/TTS configurados. Apagar un interruptor no borra lo que ya fue guardado.
      </p>
      <Interruptor
        activo={preferenciasMemoria.recordar}
        onClick={() =>
          onCambiarPreferenciasMemoria({
            ...preferenciasMemoria,
            recordar: !preferenciasMemoria.recordar,
          })
        }
        etiqueta="Recordar cosas"
        descripcion="Interruptor maestro para nuevas experiencias y hechos"
        icono={<Brain size={18} aria-hidden="true" />}
      />
      <Interruptor
        activo={preferenciasMemoria.hechos}
        disabled={!memoriaMaestra}
        onClick={() =>
          onCambiarPreferenciasMemoria({
            ...preferenciasMemoria,
            hechos: !preferenciasMemoria.hechos,
          })
        }
        etiqueta="Guardar hechos explícitos"
        descripcion="Nombre, preferencias y datos que Kira entiende con evidencia"
        icono={<ShieldCheck size={18} aria-hidden="true" />}
      />
      <Interruptor
        activo={preferenciasMemoria.experiencias}
        disabled={!memoriaMaestra}
        onClick={() =>
          onCambiarPreferenciasMemoria({
            ...preferenciasMemoria,
            experiencias: !preferenciasMemoria.experiencias,
          })
        }
        etiqueta="Guardar experiencias"
        descripcion="Una memoria breve por charla, sólo si supera el filtro de importancia"
        icono={<Activity size={18} aria-hidden="true" />}
      />
      <Interruptor
        activo={preferenciasMemoria.usar}
        disabled={!memoriaMaestra}
        onClick={() =>
          onCambiarPreferenciasMemoria({
            ...preferenciasMemoria,
            usar: !preferenciasMemoria.usar,
          })
        }
        etiqueta="Usar recuerdos al responder"
        descripcion="RAG local, identidad y búsqueda de recuerdos relevantes"
        icono={<Eye size={18} aria-hidden="true" />}
      />
      <div className="ajustes-acciones">
        <button type="button" className="ajustes-boton" onClick={onAbrirMemoria}>
          <Brain size={15} aria-hidden="true" />
          Ver memoria
        </button>
        <button
          type="button"
          className="ajustes-boton"
          onClick={onExportarMemoria}
          disabled={cargandoDiagnostico}
        >
          <Download size={15} aria-hidden="true" />
          Exportar
        </button>
      </div>
      <p className="ajustes-resumen">
        {totalActivos} activo{totalActivos !== 1 ? 's' : ''} · {totalHechos} hecho
        {totalHechos !== 1 ? 's' : ''} · {totalExperiencias} experiencia
        {totalExperiencias !== 1 ? 's' : ''} · {totalReflexiones} reflexión
        {totalReflexiones !== 1 ? 'es' : ''}
      </p>
      <button
        type="button"
        className={'ajustes-fila peligro' + (confirmandoMemoria ? ' confirmando' : '')}
        onClick={onBorrarMemoria}
        disabled={borrandoMemoria || totalActivos === 0}
      >
        <span className="ajustes-fila-ico">
          {borrandoMemoria ? (
            <Activity size={18} className="girando" aria-hidden="true" />
          ) : confirmandoMemoria ? (
            <Check size={18} aria-hidden="true" />
          ) : (
            <Trash2 size={18} aria-hidden="true" />
          )}
        </span>
        <span className="ajustes-fila-texto">
          <strong>
            {borrandoMemoria
              ? 'Borrando memoria…'
              : confirmandoMemoria
                ? '¿Seguro? Toque de nuevo para borrar los recuerdos'
                : 'Borrar todos los recuerdos'}
          </strong>
          <small>Deja de usarlos, pero conserva tombstones y el diario de personalidad</small>
        </span>
      </button>
    </section>
  );
}
