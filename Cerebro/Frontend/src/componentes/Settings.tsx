import { memo } from 'react';
// Settings.tsx - Ajustes de Kira: sonido, memoria/privacidad, chats,
// dispositivo y diagnóstico. Las secciones viven en ./ajustes/.
import { useCallback, useEffect, useRef, useState } from 'react';
import { X } from 'lucide-react';
import { AnimatePresence, motion } from 'motion/react';
import { T, EASE_OUT, SPRING_MODAL } from '../lib/motion';
import {
  elegirProveedor,
  listarProveedores,
  obtenerMemoria,
  obtenerRagStatus,
  obtenerStatus,
  type EstadoMemoria,
  type EstadoMicrobit,
  type EstadoProveedores,
  type EstadoRag,
  type PreferenciasMemoria,
  type UsuarioKira,
} from '../lib/api';
import type { EstadoPuenteBle } from '../lib/ble-protocolo';
import { useFocoModal } from '../hooks/useFocoModal';
import { SeccionSonido } from './ajustes/SeccionSonido';
import { SeccionMemoria } from './ajustes/SeccionMemoria';
import { SeccionChats } from './ajustes/SeccionChats';
import { SeccionDispositivo } from './ajustes/SeccionDispositivo';
import { SeccionProveedores } from './ajustes/SeccionProveedores';
import { SeccionDiagnostico } from './ajustes/SeccionDiagnostico';
import { SeccionCuenta } from './ajustes/SeccionCuenta';

interface Props {
  abierto: boolean;
  onCerrar: () => void;
  sonido: boolean;
  onCambiarSonido: (v: boolean) => void;
  voz: boolean;
  onCambiarVoz: (v: boolean) => void;
  autonomia: boolean;
  onCambiarAutonomia: (v: boolean) => void;
  fondoVivo: boolean;
  onCambiarFondoVivo: (v: boolean) => void;
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

function SettingsInner({
  abierto,
  onCerrar,
  sonido,
  onCambiarSonido,
  voz,
  onCambiarVoz,
  autonomia,
  onCambiarAutonomia,
  fondoVivo,
  onCambiarFondoVivo,
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
  const [proveedores, setProveedores] = useState<EstadoProveedores | null>(null);
  const [cargandoProveedores, setCargandoProveedores] = useState(false);
  const [cambiandoProveedor, setCambiandoProveedor] = useState<string | null>(null);
  const [errorProveedor, setErrorProveedor] = useState('');
  const [estado, setEstado] = useState<EstadoMicrobit | null>(estadoMicrobit);
  const [cargandoDiagnostico, setCargandoDiagnostico] = useState(false);
  const [error, setError] = useState('');
  const confirmarChatsTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const confirmarMemoriaTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const { panelRef, closeRef } = useFocoModal(abierto);

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

  // Proveedores: se leen al abrir y cuando el usuario pide refrescar. No se
  // hace polling: el "enfriando" de un proveedor cambia solo cuando algo
  // falla, y para eso está el botón de refrescar.
  const cargarProveedores = useCallback(async () => {
    setCargandoProveedores(true);
    setErrorProveedor('');
    try {
      setProveedores(await listarProveedores());
    } catch (e) {
      setErrorProveedor(
        'No se pudo leer la lista de proveedores: ' +
          (e instanceof Error ? e.message : String(e))
      );
    } finally {
      setCargandoProveedores(false);
    }
  }, []);

  const cambiarProveedor = useCallback(
    async (nombre: string) => {
      setCambiandoProveedor(nombre || 'auto');
      setErrorProveedor('');
      try {
        await elegirProveedor(nombre);
        setProveedores(await listarProveedores());
      } catch (e) {
        setErrorProveedor(e instanceof Error ? e.message : String(e));
      } finally {
        setCambiandoProveedor(null);
      }
    },
    []
  );

  useEffect(() => {
    if (!abierto) return;
    void cargarProveedores();
  }, [abierto, cargarProveedores]);

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
              <button
                ref={closeRef}
                className="ajustes-x"
                onClick={onCerrar}
                title="Cerrar"
                aria-label="Cerrar ajustes"
              >
                <X size={18} strokeWidth={2.2} aria-hidden="true" />
              </button>
            </header>

            {error && (
              <p className="error ajustes-error" role="alert">
                {error}
              </p>
            )}

            <div className="ajustes-cuerpo">
              <SeccionSonido
                sonido={sonido}
                onCambiarSonido={onCambiarSonido}
                voz={voz}
                onCambiarVoz={onCambiarVoz}
                autonomia={autonomia}
                onCambiarAutonomia={onCambiarAutonomia}
                fondoVivo={fondoVivo}
                onCambiarFondoVivo={onCambiarFondoVivo}
                volumenVoz={volumenVoz}
                onCambiarVolumenVoz={onCambiarVolumenVoz}
              />
              <SeccionMemoria
                preferenciasMemoria={preferenciasMemoria}
                onCambiarPreferenciasMemoria={onCambiarPreferenciasMemoria}
                memoria={memoria}
                cargandoDiagnostico={cargandoDiagnostico}
                borrandoMemoria={borrandoMemoria}
                confirmandoMemoria={confirmandoMemoria}
                onAbrirMemoria={onAbrirMemoria}
                onExportarMemoria={() => void exportarMemoria()}
                onBorrarMemoria={() => void pedirBorrarMemoria()}
              />
              <SeccionChats
                totalChats={totalChats}
                confirmandoChats={confirmandoChats}
                onBorrarChats={pedirBorrarChats}
              />
              <SeccionProveedores
                estado={proveedores}
                cargando={cargandoProveedores}
                error={errorProveedor}
                cambiando={cambiandoProveedor}
                onRefrescar={() => void cargarProveedores()}
                onElegir={(nombre) => void cambiarProveedor(nombre)}
              />
              <SeccionDispositivo
                estado={estado}
                bleEstado={bleEstado}
                onAlternarBle={onAlternarBle}
              />
              <SeccionDiagnostico rag={rag} cargandoDiagnostico={cargandoDiagnostico} />
              <SeccionCuenta
                usuario={usuario}
                confirmandoSalir={confirmandoSalir}
                onCerrarSesion={() => void pedirCerrarSesion()}
              />
            </div>
          </motion.div>
        </motion.div>
      )}
    </AnimatePresence>
  );
}

// memo: el modal re-renderiza solo si cambia algo suyo, no en cada tick.
const Settings = memo(SettingsInner);
export default Settings;
