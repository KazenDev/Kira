import { Activity } from 'lucide-react';
import type { EstadoRag } from '../../lib/api';

type Props = {
  rag: EstadoRag | null;
  cargandoDiagnostico: boolean;
};

export function SeccionDiagnostico({ rag, cargandoDiagnostico }: Props) {
  return (
    <section className="ajustes-seccion">
      <div className="ajustes-titulo">
        <Activity size={16} strokeWidth={2.2} aria-hidden="true" />
        <h3>Diagnóstico</h3>
      </div>
      <div className="ajustes-diagnostico" aria-live="polite">
        {cargandoDiagnostico ? (
          <p className="ajustes-nota">Leyendo el estado local…</p>
        ) : (
          <>
            <div>
              <span>Backend</span>
              <strong className="ok-texto">Respondiendo</strong>
            </div>
            <div>
              <span>RAG</span>
              <strong>{rag?.fts ? 'FTS5 + vectores' : rag ? 'Índice disponible' : 'Sin diagnóstico'}</strong>
            </div>
            <div>
              <span>Embeddings</span>
              <strong>
                {rag?.vectors ?? 0} vectores · {rag?.embedding_dimension ?? 0} dimensiones
              </strong>
            </div>
            <div>
              <span>Modelo local</span>
              <strong>{rag?.embedding_model || 'No identificado'}</strong>
            </div>
            <div>
              <span>Índice local</span>
              <strong>{rag?.db_path || 'No disponible'}</strong>
            </div>
          </>
        )}
      </div>
      <p className="ajustes-nota ajustes-nota-chica">
        El índice es una ayuda derivada: los recuerdos permanecen en la fuente local aunque se
        reinicie.
      </p>
    </section>
  );
}
