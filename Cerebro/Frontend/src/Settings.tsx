// Settings.tsx - Ajustes de Kira: sonido, memoria/privacidad, chats,
// dispositivo y diagnóstico. Las acciones destructivas piden doble confirmación.
import { useEffect, useRef, useState } from 'react';
import type { ReactNode } from 'react';
import {
  Activity,
  Bluetooth,
  Brain,
  Check,
  Download,
  Eye,
  Info,
  LogOut,
  Mic2,
  ShieldCheck,
  Trash2,
  Volume2,
  VolumeX,
  X,
} from 'lucide-react';
import { AnimatePresence, motion } from 'motion/react';
import { T, EASE_OUT, SPRING_MODAL } from './motion';
import {
  obtenerMemoria,
  obtenerRagStatus,
  obtenerStatus,
  type EstadoMemoria,
  type EstadoMicrobit,
  type EstadoRag,
  type PreferenciasMemoria,
  type UsuarioKira,
} from './api';
import type { EstadoPuenteBle } from './PuenteBle';

interface Props {
  abierto: boolean;
  onCerrar: () => void;
  sonido: boolean;
  onCambiarSonido: (v: boolean) => void;
  voz: boolean;
  onCambiarVoz: (v: boolean) => void;
  volumenVoz: number;
  onCambiarVolumenVoz: (v: number) => void;
  preferenciasMemoria: PreferenciasMemoria;
  onCambiarPreferenciasMemoria: (v: PreferenciasMemoria) => void;
  totalChats: number;
  onBorrarTodos: () => void;
  onAbrirMemoria: () => void;
  onBorrarMemoria: () => Promise<void>;
  estadoMicrobit: EstadoMicrobit | null;
  bleEstado: EstadoPuenteBle;
  onAlternarBle: () => void;
  usuario: UsuarioKira;
  onCerrarSesion: () => Promise<void>;
}

function Interruptor({
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

export default function Settings({
  abierto,
  onCerrar,
  sonido,
  onCambiarSonido,
  voz,
  onCambiarVoz,
  volumenVoz,
  onCambiarVolumenVoz,
  preferenciasMemoria,
  onCambiarPreferenciasMemoria,
  totalChats,
  onBorrarTodos,
  onAbrirMemoria,
  onBorrarMemoria,
  estadoMicrobit,
  bleEstado,
  onAlternarBle,
  usuario,
  onCerrarSesion,
}: Props) {
  const [confirmandoChats, setConfirmandoChats] = useState(false);
  const [confirmandoMemoria, setConfirmandoMemoria] = useState(false);
  const [confirmandoSalir, setConfirmandoSalir] = useState(false);
  const [borrandoMemoria, setBorrandoMemoria] = useState(false);
  const [memoria, setMemoria] = useState<EstadoMemoria | null>(null);
  const [rag, setRag] = useState<EstadoRag | null>(null);
  const [estado, setEstado] = useState<EstadoMicrobit | null>(estadoMicrobit);
  const [cargandoDiagnostico, setCargandoDiagnostico] = useState(false);
  const [error, setError] = useState('');
  const confirmarChatsTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const confirmarMemoriaTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const returnFocusRef = useRef<HTMLElement | null>(null);
  const panelRef = useRef<HTMLDivElement | null>(null);
  const closeRef = useRef<HTMLButtonElement | null>(null);

  useEffect(() => {
    if (!abierto) return;
    setConfirmandoChats(false);
    setConfirmandoMemoria(false);
    setConfirmandoSalir(false);
    setBorrandoMemoria(false);
    setError('');
    setMemoria(null);
    setRag(null);
    setEstado(estadoMicrobit);
    setCargandoDiagnostico(true);
    let vigente = true;

    // El diagnóstico se pide al abrir Ajustes, no en cada render: el panel
    // refleja el estado real sin convertir el modal en un polling constante.
    void Promise.allSettled([
      obtenerMemoria('kira', 20),
      obtenerRagStatus(),
      obtenerStatus(),
    ]).then(([memoriaResultado, ragResultado, estadoResultado]) => {
      if (!vigente) return;
      if (memoriaResultado.status === 'fulfilled') setMemoria(memoriaResultado.value);
      if (ragResultado.status === 'fulfilled') setRag(ragResultado.value);
      if (estadoResultado.status === 'fulfilled') setEstado(estadoResultado.value);
      const fallos = [memoriaResultado, ragResultado, estadoResultado].filter(
        (resultado) => resultado.status === 'rejected'
      );
      if (fallos.length === 3) {
        setError('No se pudo contactar al servidor para leer el diagnóstico.');
      }
      setCargandoDiagnostico(false);
    });

    return () => {
      vigente = false;
    };
    // estadoMicrobit se copia al abrir; no queremos reiniciar el fetch cuando
    // cambia el polling normal del App.
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [abierto]);

  useEffect(() => {
    if (estadoMicrobit) setEstado(estadoMicrobit);
  }, [estadoMicrobit]);

  useEffect(() => {
    setConfirmandoChats(false);
    setConfirmandoMemoria(false);
    setConfirmandoSalir(false);
    return () => {
      if (confirmarChatsTimerRef.current) clearTimeout(confirmarChatsTimerRef.current);
      if (confirmarMemoriaTimerRef.current) clearTimeout(confirmarMemoriaTimerRef.current);
      confirmarChatsTimerRef.current = null;
      confirmarMemoriaTimerRef.current = null;
    };
  }, [abierto]);

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
      const vuelveAlDrawerOculto =
        objetivo.closest('.sidebar') !== null && window.innerWidth <= 860;
      if (!vuelveAlDrawerOculto && sigueVisible && objetivo) objetivo.focus();
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

  const pedirBorrarChats = () => {
    if (!confirmandoChats) {
      setConfirmandoChats(true);
      confirmarChatsTimerRef.current = setTimeout(() => {
        confirmarChatsTimerRef.current = null;
        setConfirmandoChats(false);
      }, 3500);
      return;
    }
    if (confirmarChatsTimerRef.current) clearTimeout(confirmarChatsTimerRef.current);
    confirmarChatsTimerRef.current = null;
    setConfirmandoChats(false);
    onBorrarTodos();
  };

  const pedirBorrarMemoria = async () => {
    if (borrandoMemoria) return;
    if (!confirmandoMemoria) {
      setConfirmandoMemoria(true);
      confirmarMemoriaTimerRef.current = setTimeout(() => {
        confirmarMemoriaTimerRef.current = null;
        setConfirmandoMemoria(false);
      }, 4500);
      return;
    }
    if (confirmarMemoriaTimerRef.current) clearTimeout(confirmarMemoriaTimerRef.current);
    confirmarMemoriaTimerRef.current = null;
    setConfirmandoMemoria(false);
    setBorrandoMemoria(true);
    setError('');
    try {
      await onBorrarMemoria();
      setMemoria(await obtenerMemoria('kira', 20));
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    } finally {
      setBorrandoMemoria(false);
    }
  };

  const exportarMemoria = async () => {
    setError('');
    try {
      const datos = await obtenerMemoria('kira', 20);
      const blob = new Blob([JSON.stringify(datos, null, 2)], { type: 'application/json' });
      const url = URL.createObjectURL(blob);
      const enlace = document.createElement('a');
      enlace.href = url;
      enlace.download = 'kira-memoria.json';
      document.body.appendChild(enlace);
      enlace.click();
      enlace.remove();
      window.setTimeout(() => URL.revokeObjectURL(url), 1000);
    } catch (e) {
      setError(e instanceof Error ? e.message : String(e));
    }
  };

  const pedirCerrarSesion = async () => {
    if (!confirmandoSalir) {
      setConfirmandoSalir(true);
      window.setTimeout(() => setConfirmandoSalir(false), 3500);
      return;
    }
    setConfirmandoSalir(false);
    await onCerrarSesion();
  };

  const totalActivos = memoria?.total_activos ?? memoria?.total_recuerdos ?? 0;
  const totalHechos = memoria?.total_hechos ?? 0;
  const totalExperiencias = memoria?.total_experiencias ?? 0;
  const totalReflexiones = memoria?.total_reflexiones ?? 0;
  const microbit = estado?.microbit;
  const bleConectado = bleEstado === 'conectado' || microbit?.ble_relay === true;
  const bleOcupado = bleEstado === 'conectando' || bleEstado === 'reconectando';
  const memoriaMaestra = preferenciasMemoria.recordar;

  return (
    <AnimatePresence>
      {abierto && (
        <motion.div
          className="ajustes-fondo"
          onClick={onCerrar}
          initial={{ opacity: 0 }}
          animate={{ opacity: 1 }}
          exit={{ opacity: 0 }}
          transition={{ duration: T.fast, ease: EASE_OUT }}
        >
          <motion.div
            ref={panelRef}
            className="ajustes-panel ajustes-panel-completo"
            onClick={(e) => e.stopPropagation()}
            role="dialog"
            aria-modal="true"
            aria-label="Ajustes"
            initial={{ opacity: 0, scale: 0.96, y: 10 }}
            animate={{ opacity: 1, scale: 1, y: 0 }}
            exit={{ opacity: 0, scale: 0.96, y: 10 }}
            transition={SPRING_MODAL}
          >
            <header className="ajustes-header">
              <h2>Ajustes</h2>
              <button ref={closeRef} className="ajustes-x" onClick={onCerrar} title="Cerrar" aria-label="Cerrar ajustes">
                <X size={18} strokeWidth={2.2} aria-hidden="true" />
              </button>
            </header>

            {error && <p className="error ajustes-error" role="alert">{error}</p>}

            <div className="ajustes-cuerpo">
              {/* ---------- Sonido y voz ---------- */}
              <section className="ajustes-seccion">
                <div className="ajustes-titulo">
                  <Volume2 size={16} strokeWidth={2.2} aria-hidden="true" />
                  <h3>Sonido</h3>
                </div>
                <Interruptor
                  activo={sonido}
                  onClick={() => onCambiarSonido(!sonido)}
                  etiqueta="Sonidos de la app"
                  descripcion={
                    sonido
                      ? 'Pop suave al enviar, recibir y al cambiar de vista'
                      : 'Todo silencioso (ideal para la feria)'
                  }
                  icono={sonido ? <Volume2 size={18} aria-hidden="true" /> : <VolumeX size={18} aria-hidden="true" />}
                />
                <Interruptor
                  activo={voz}
                  onClick={() => onCambiarVoz(!voz)}
                  etiqueta="Voz de Kira"
                  descripcion={voz ? 'Reproducir la voz y sincronizar la boca de la micro:bit' : 'No reproducir audio; la placa queda muda'}
                  icono={<Mic2 size={18} aria-hidden="true" />}
                />
                <div className={'ajustes-slider' + (voz ? '' : ' deshabilitado')}>
                  <div className="ajustes-slider-cabecera">
                    <label htmlFor="volumen-voz">Volumen de la voz</label>
                    <output htmlFor="volumen-voz">{Math.round(volumenVoz * 100)}%</output>
                  </div>
                  <input
                    id="volumen-voz"
                    type="range"
                    min="0"
                    max="1"
                    step="0.05"
                    value={volumenVoz}
                    disabled={!voz}
                    onChange={(e) => onCambiarVolumenVoz(Number(e.target.value))}
                    aria-label="Volumen de la voz de Kira"
                  />
                </div>
              </section>

              {/* ---------- Memoria y privacidad ---------- */}
              <section className="ajustes-seccion">
                <div className="ajustes-titulo">
                  <Brain size={16} strokeWidth={2.2} aria-hidden="true" />
                  <h3>Memoria y privacidad</h3>
                </div>
                <p className="ajustes-nota">
                  La memoria y el índice viven localmente; las conversaciones pueden pasar por los proveedores de IA/TTS configurados.
                  Apagar un interruptor no borra lo que ya fue guardado.
                </p>
                <Interruptor
                  activo={preferenciasMemoria.recordar}
                  onClick={() => onCambiarPreferenciasMemoria({
                    ...preferenciasMemoria,
                    recordar: !preferenciasMemoria.recordar,
                  })}
                  etiqueta="Recordar cosas"
                  descripcion="Interruptor maestro para nuevas experiencias y hechos"
                  icono={<Brain size={18} aria-hidden="true" />}
                />
                <Interruptor
                  activo={preferenciasMemoria.hechos}
                  disabled={!memoriaMaestra}
                  onClick={() => onCambiarPreferenciasMemoria({
                    ...preferenciasMemoria,
                    hechos: !preferenciasMemoria.hechos,
                  })}
                  etiqueta="Guardar hechos explícitos"
                  descripcion="Nombre, preferencias y datos que Kira entiende con evidencia"
                  icono={<ShieldCheck size={18} aria-hidden="true" />}
                />
                <Interruptor
                  activo={preferenciasMemoria.experiencias}
                  disabled={!memoriaMaestra}
                  onClick={() => onCambiarPreferenciasMemoria({
                    ...preferenciasMemoria,
                    experiencias: !preferenciasMemoria.experiencias,
                  })}
                  etiqueta="Guardar experiencias"
                  descripcion="Una memoria breve por charla, sólo si supera el filtro de importancia"
                  icono={<Activity size={18} aria-hidden="true" />}
                />
                <Interruptor
                  activo={preferenciasMemoria.usar}
                  disabled={!memoriaMaestra}
                  onClick={() => onCambiarPreferenciasMemoria({
                    ...preferenciasMemoria,
                    usar: !preferenciasMemoria.usar,
                  })}
                  etiqueta="Usar recuerdos al responder"
                  descripcion="RAG local, identidad y búsqueda de recuerdos relevantes"
                  icono={<Eye size={18} aria-hidden="true" />}
                />
                <div className="ajustes-acciones">
                  <button type="button" className="ajustes-boton" onClick={onAbrirMemoria}>
                    <Brain size={15} aria-hidden="true" />
                    Ver memoria
                  </button>
                  <button type="button" className="ajustes-boton" onClick={() => void exportarMemoria()} disabled={cargandoDiagnostico}>
                    <Download size={15} aria-hidden="true" />
                    Exportar
                  </button>
                </div>
                <p className="ajustes-resumen">
                  {totalActivos} activo{totalActivos !== 1 ? 's' : ''} · {totalHechos} hecho{totalHechos !== 1 ? 's' : ''} · {totalExperiencias} experiencia{totalExperiencias !== 1 ? 's' : ''} · {totalReflexiones} reflexión{totalReflexiones !== 1 ? 'es' : ''}
                </p>
                <button
                  type="button"
                  className={'ajustes-fila peligro' + (confirmandoMemoria ? ' confirmando' : '')}
                  onClick={() => void pedirBorrarMemoria()}
                  disabled={borrandoMemoria || totalActivos === 0}
                >
                  <span className="ajustes-fila-ico">
                    {borrandoMemoria ? <Activity size={18} className="girando" aria-hidden="true" /> : confirmandoMemoria ? <Check size={18} aria-hidden="true" /> : <Trash2 size={18} aria-hidden="true" />}
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

              {/* ---------- Chats ---------- */}
              <section className="ajustes-seccion">
                <div className="ajustes-titulo">
                  <Trash2 size={16} strokeWidth={2.2} aria-hidden="true" />
                  <h3>Chats</h3>
                </div>
                <p className="ajustes-nota">
                  Los chats se guardan en este navegador. Borrarlos no borra la memoria de Kira.
                </p>
                <button
                  type="button"
                  className={'ajustes-fila peligro' + (confirmandoChats ? ' confirmando' : '')}
                  onClick={pedirBorrarChats}
                  disabled={totalChats === 0}
                >
                  <span className="ajustes-fila-ico">
                    {confirmandoChats ? <Check size={18} aria-hidden="true" /> : <Trash2 size={18} aria-hidden="true" />}
                  </span>
                  <span className="ajustes-fila-texto">
                    <strong>{confirmandoChats ? '¿Seguro? Toque de nuevo para borrar todo' : 'Borrar todos los chats'}</strong>
                    <small>
                      {totalChats === 0
                        ? 'No hay conversaciones para borrar'
                        : `${totalChats} conversación${totalChats !== 1 ? 'es' : ''} se eliminarán de este equipo`}
                    </small>
                  </span>
                </button>
              </section>

              {/* ---------- Dispositivo ---------- */}
              <section className="ajustes-seccion">
                <div className="ajustes-titulo">
                  <Bluetooth size={16} strokeWidth={2.2} aria-hidden="true" />
                  <h3>Dispositivo</h3>
                </div>
                <div className="ajustes-estado-card">
                  <div>
                    <strong>micro:bit</strong>
                    <small>
                      {microbit?.respondiendo
                        ? `En línea${microbit.puerto ? ` · ${microbit.puerto}` : ''}`
                        : microbit?.conectado
                          ? `Conectada por USB${microbit.puerto ? ` · ${microbit.puerto}` : ''}`
                          : bleConectado
                            ? 'Conectada por el puente Bluetooth del navegador'
                            : 'Sin conexión (ni USB ni puente Bluetooth)'}
                    </small>
                  </div>
                  <span className={'estado-punto ' + (microbit?.respondiendo || bleConectado ? 'ok' : 'off')} aria-label={microbit?.respondiendo || bleConectado ? 'conectado' : 'desconectado'} />
                </div>
                <button type="button" className="ajustes-boton" onClick={onAlternarBle} disabled={bleOcupado}>
                  <Bluetooth size={15} aria-hidden="true" />
                  {bleConectado ? 'Desconectar puente Bluetooth' : bleOcupado ? 'Conectando…' : 'Conectar micro:bit por Bluetooth'}
                </button>
                <p className="ajustes-nota ajustes-nota-chica">
                  El puente Bluetooth necesita un navegador compatible y, normalmente, HTTPS o localhost.
                </p>
              </section>

              {/* ---------- Diagnóstico ---------- */}
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
                      <div><span>Backend</span><strong className="ok-texto">Respondiendo</strong></div>
                      <div><span>RAG</span><strong>{rag?.fts ? 'FTS5 + vectores' : rag ? 'Índice disponible' : 'Sin diagnóstico'}</strong></div>
                      <div><span>Embeddings</span><strong>{rag?.vectors ?? 0} vectores · {rag?.embedding_dimension ?? 0} dimensiones</strong></div>
                      <div><span>Modelo local</span><strong>{rag?.embedding_model || 'No identificado'}</strong></div>
                      <div><span>Índice local</span><strong>{rag?.db_path || 'No disponible'}</strong></div>
                    </>
                  )}
                </div>
                <p className="ajustes-nota ajustes-nota-chica">
                  El índice es una ayuda derivada: los recuerdos permanecen en la fuente local aunque se reinicie.
                </p>
              </section>

              {/* ---------- Cuenta ---------- */}
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
                  onClick={() => void pedirCerrarSesion()}
                >
                  <span className="ajustes-fila-ico"><LogOut size={18} aria-hidden="true" /></span>
                  <span className="ajustes-fila-texto">
                    <strong>{confirmandoSalir ? '¿Cerrar esta sesión?' : 'Cerrar sesión'}</strong>
                    <small>La memoria de esta cuenta queda guardada</small>
                  </span>
                </button>
              </section>

              {/* ---------- Acerca de ---------- */}
              <section className="ajustes-seccion">
                <div className="ajustes-titulo">
                  <Info size={16} strokeWidth={2.2} aria-hidden="true" />
                  <h3>Acerca de</h3>
                </div>
                <div className="ajustes-acerca">
                  <p>
                    <strong>Kira</strong> — una IA con voz y emociones que vive en
                    una micro:bit.
                  </p>
                  <p>
                    Su personalidad no está escrita de antemano: se construye con
                    cada charla, recuerdo y opinión que Kira va formando.
                  </p>
                  <p className="ajustes-feria">Hecho para la feria de ciencias.</p>
                </div>
              </section>
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}
