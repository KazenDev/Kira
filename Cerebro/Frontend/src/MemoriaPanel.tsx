/**
 * MemoriaPanel.tsx — 🧠 SUPERFICIE DE MEMORIA (transparencia total).
 *
 * Muestra lo que el personaje recuerda de su vida: sus memorias recientes
 * con importancia, su diario (quién es hoy) y el contador. Es la prueba
 * viva de la personalidad emergente — ningún chatbot muestra esto.
 * Lee de GET /api/memoria/{personaje} (ver GUIA_DEPLOY_VPS.md).
 */
import { useEffect, useRef, useState } from 'react';
import { AnimatePresence, motion } from 'motion/react';
import { T, EASE_OUT } from './motion';
import { Brain, Star, Trash2, X } from 'lucide-react';
import { EstadoMemoria, type MemoriaVisible, obtenerMemoria, olvidarMemoria } from './api';
import { formatearFechaCorta } from './ui-utils';

interface Props {
  abierto: boolean;
  onCerrar: () => void;
  personajeId: string;
  nombre: string;
  color: string;
}

export default function MemoriaPanel({ abierto, onCerrar, personajeId, nombre, color }: Props) {
  const [estado, setEstado] = useState<EstadoMemoria | null>(null);
  const [error, setError] = useState('');
  const [olvidando, setOlvidando] = useState<string | null>(null);
  const returnFocusRef = useRef<HTMLElement | null>(null);
  const panelRef = useRef<HTMLDivElement | null>(null);
  const closeRef = useRef<HTMLButtonElement | null>(null);

  useEffect(() => {
    if (!abierto) {
      const objetivo = returnFocusRef.current;
      returnFocusRef.current = null;
      if (!objetivo) return;
      const rect = objetivo.getBoundingClientRect();
      const sigueVisible = Boolean(
        rect &&
          rect.right > 0 &&
          rect.left < window.innerWidth &&
          rect.bottom > 0 &&
          rect.top < window.innerHeight
      );
      if (sigueVisible && objetivo) objetivo.focus();
      else document.querySelector<HTMLButtonElement>('.chat-header .hamburguesa, .inicio-menu')?.focus();
      return;
    }
    if (!returnFocusRef.current?.isConnected) {
      returnFocusRef.current = document.activeElement as HTMLElement | null;
    }
    const focusTimer = window.setTimeout(() => closeRef.current?.focus(), 0);

    const mantenerFoco = (event: KeyboardEvent) => {
      if (event.key !== 'Tab') return;
      const focusables = panelRef.current?.querySelectorAll<HTMLElement>(
        'button:not([disabled]), input:not([disabled]), [href], [tabindex]:not([tabindex="-1"])'
      );
      if (!focusables?.length) return;
      const first = focusables[0];
      const last = focusables[focusables.length - 1];
      if (event.shiftKey && document.activeElement === first) {
        event.preventDefault();
        last.focus();
      } else if (!event.shiftKey && document.activeElement === last) {
        event.preventDefault();
        first.focus();
      }
    };

    document.addEventListener('keydown', mantenerFoco);
    return () => {
      window.clearTimeout(focusTimer);
      document.removeEventListener('keydown', mantenerFoco);
    };
  }, [abierto]);

  useEffect(() => {
    if (!abierto) return;
    setEstado(null);
    setError('');
    obtenerMemoria(personajeId)
      .then(setEstado)
      .catch((e) => setError(e instanceof Error ? e.message : String(e)));
  }, [abierto, personajeId]);

  const olvidar = async (record: { id?: string; texto: string }) => {
    const id = record.id;
    if (!id || olvidando) return;
    if (!window.confirm(`¿Olvidar este recuerdo?\n\n${record.texto.slice(0, 180)}`)) return;
    setOlvidando(id);
    setError('');
    try {
      await olvidarMemoria(personajeId, id, 'olvidado desde el panel de memoria');
      setEstado(await obtenerMemoria(personajeId, 20));
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setOlvidando(null);
    }
  };

  const renderMemoria = (record: MemoriaVisible, key: string) => (
    <div key={key} className="memoria-item memoria-item-con-boton">
      <div className="memoria-item-contenido">
        <p>{record.texto}</p>
        <span className="memoria-meta">
          {formatearFechaCorta(record.fecha)} · <Star size={11} aria-hidden="true" /> {record.importancia}/10
        </span>
      </div>
      {record.id && (
        <button
          type="button"
          className="memoria-olvidar"
          onClick={() => void olvidar(record)}
          disabled={olvidando === record.id}
          aria-label={`Olvidar: ${record.texto.slice(0, 100)}`}
          title="Olvidar este recuerdo"
        >
          <Trash2 size={15} aria-hidden="true" />
        </button>
      )}
    </div>
  );

  const hechos = estado?.hechos ?? estado?.recuerdos.filter((r) => r.clase === 'semantic') ?? [];
  const experiencias = estado?.experiencias ?? estado?.recuerdos.filter((r) => r.clase === 'episodic') ?? [];
  const reflexiones = estado?.reflexiones ?? estado?.recuerdos.filter((r) => r.clase === 'reflection') ?? [];
  const totalActivos = estado?.total_activos ?? estado?.total_recuerdos ?? 0;
  const totalHechos = estado?.total_hechos ?? hechos.length;
  const totalExperiencias = estado?.total_experiencias ?? experiencias.length;

  return (
    <AnimatePresence>
      {abierto && (
        <motion.div
          className="ajustes-fondo"
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          transition={{ duration: T.fast, ease: EASE_OUT }}
          onClick={onCerrar}
        >
          <motion.div
            ref={panelRef}
            className="ajustes-panel memoria-panel"
            style={{ '--color': color } as React.CSSProperties}
            initial={{ opacity: 0, y: 16, scale: 0.98 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: 16, scale: 0.98 }}
            transition={{ duration: T.fast, ease: EASE_OUT }}
            onClick={(e) => e.stopPropagation()}
            role="dialog"
            aria-modal="true"
            aria-label={`Lo que ${nombre} recuerda`}
          >
            <div className="ajustes-header">
              <h2>
                <Brain size={16} aria-hidden="true" /> Lo que {nombre} recuerda
              </h2>
              <button ref={closeRef} className="icon-btn" onClick={onCerrar} aria-label="Cerrar">
                <X size={18} />
              </button>
            </div>

            {error && <p className="error">{error}</p>}
            {/* skeleton CON FORMA del panel: la silueta de lo que viene
                (total + secciones + recuerdos) en vez de un texto flotando.
                role=status con label: al lector de pantalla se le ANUNCIA */}
            {!estado && !error && (
              <div
                className="memoria-sk"
                role="status"
                aria-label="leyendo su mente…"
              >
                <span className="sk sk-linea" style={{ width: '48%' }} aria-hidden="true" />
                <span className="sk sk-seccion" style={{ width: '34%' }} aria-hidden="true" />
                {[0, 1, 2].map((n) => (
                  <div className="sk-item" key={n} aria-hidden="true">
                    <span className="sk sk-linea" style={{ width: `${86 - n * 14}%` }} />
                    <span className="sk sk-linea chico" style={{ width: '36%' }} />
                  </div>
                ))}
              </div>
            )}

            {estado && (
              <>
                <p className="memoria-total">
                  {totalActivos} registro{totalActivos !== 1 ? 's' : ''} en su memoria
                  {totalHechos > 0 && (
                    <> · {totalHechos} hecho{totalHechos !== 1 ? 's' : ''}</>
                  )}
                  {totalExperiencias > 0 && (
                    <> · {totalExperiencias} experiencia{totalExperiencias !== 1 ? 's' : ''}</>
                  )}
                </p>
                {estado.diario.yo_soy && (
                  <div className="memoria-diario">
                    <span className="memoria-seccion">Quién es hoy (escrito por ella misma)</span>
                    <p>{estado.diario.yo_soy}</p>
                  </div>
                )}
                {estado.diario.opiniones.length > 0 && (
                  <div className="memoria-diario">
                    <span className="memoria-seccion">Sus opiniones</span>
                    {estado.diario.opiniones.slice(0, 5).map((op, i) => (
                      <p key={i} className="memoria-opinion">“{op}”</p>
                    ))}
                  </div>
                )}
                {hechos.length > 0 && (
                  <>
                    <span className="memoria-seccion">Hechos que recuerda de vos</span>
                    {hechos.map((r, i) => renderMemoria(r, `hecho-${r.id ?? i}`))}
                  </>
                )}
                {experiencias.length > 0 && (
                  <>
                    <span className="memoria-seccion">Experiencias que recuerda</span>
                    {experiencias.slice(0, 5).map((r, i) => renderMemoria(r, `experiencia-${r.id ?? i}`))}
                  </>
                )}
                {reflexiones.length > 0 && (
                  <>
                    <span className="memoria-seccion">Reflexiones</span>
                    {reflexiones.map((r, i) => renderMemoria(r, `reflexion-${r.id ?? i}`))}
                  </>
                )}
                {hechos.length === 0 && experiencias.length === 0 && reflexiones.length === 0 && (
                  <p className="memoria-vacia">Todavía no hay recuerdos. Hablale con Kira para empezar su historia.</p>
                )}
              </>
            )}
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
