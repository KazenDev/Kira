import { useEffect, useId, useRef, useState, type ReactNode } from 'react';
import { ArrowLeft, Cpu, Menu, MoreHorizontal } from 'lucide-react';
import { AnimatePresence, motion } from 'motion/react';
import { T, EASE_OUT, SPRING_POP } from './motion';
import type { PersonajeInfo } from './api';
import Avatar from './Avatar';
import MicrobitVirtual from './MicrobitVirtual';

interface Props {
  elegido: PersonajeInfo;
  tituloConversacion: string;
  color: string;
  cara: string; // emotion | 'loading' | 'talk'
  patronReal: string | null;
  mbEstado: string;
  mbTexto: string;
  mbTitulo: string;
  scrolleo: boolean;
  onAbrirSidebar: () => void;
  onVolver: () => void;
  acciones?: ReactNode;
}

const NOMBRES_ESTADO: Record<string, string> = {
  happy: 'feliz',
  sad: 'triste',
  angry: 'molesto',
  surprised: 'sorprendido',
  neutral: 'tranquilo',
  loading: 'pensando',
  talk: 'hablando',
};

function nombreEstado(cara: string) {
  return NOMBRES_ESTADO[cara] ?? cara.split('_').join(' ');
}

export default function ChatHeader({
  elegido,
  tituloConversacion,
  color,
  cara,
  patronReal,
  mbEstado,
  mbTexto,
  mbTitulo,
  scrolleo,
  onAbrirSidebar,
  onVolver,
  acciones,
}: Props) {
  const [ledAbierto, setLedAbierto] = useState(false);
  const [menuAbierto, setMenuAbierto] = useState(false);
  const ledWrapRef = useRef<HTMLDivElement | null>(null);
  const menuWrapRef = useRef<HTMLDivElement | null>(null);
  const ledButtonRef = useRef<HTMLButtonElement | null>(null);
  const menuButtonRef = useRef<HTMLButtonElement | null>(null);
  const ledPopoverId = useId();
  const menuPopoverId = useId();

  const pensando = cara === 'loading';
  const hablando = cara === 'talk';
  const enActividad = pensando || hablando;

  // El título de la conversación manda; debajo queda el estado de Kira y de la
  // placa como información secundaria, sin robarle jerarquía.
  const estadoVisible = pensando
    ? `${elegido.nombre} está pensando…`
    : hablando
      ? `${elegido.nombre} está hablando…`
      : `${elegido.nombre} · ${mbTexto}`;

  const estadoTitle = [enActividad ? mbTexto : '', mbTitulo]
    .filter(Boolean)
    .join(' · ');

  useEffect(() => {
    setLedAbierto(false);
    setMenuAbierto(false);
  }, [elegido.id]);

  useEffect(() => {
    if (!ledAbierto && !menuAbierto) return;

    const cerrarFuera = (event: PointerEvent) => {
      const target = event.target as Node | null;
      if (target && ledAbierto && !ledWrapRef.current?.contains(target)) {
        setLedAbierto(false);
      }
      if (target && menuAbierto && !menuWrapRef.current?.contains(target)) {
        setMenuAbierto(false);
      }
    };

    const cerrarEscape = (event: KeyboardEvent) => {
      if (event.key === 'Tab' && menuAbierto) {
        const focusables = menuWrapRef.current?.querySelectorAll<HTMLElement>(
          'button:not([disabled]), [href], [tabindex]:not([tabindex="-1"])'
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
        return;
      }
      if (event.key !== 'Escape') return;
      event.preventDefault();
      event.stopImmediatePropagation();
      if (menuAbierto) {
        setMenuAbierto(false);
        window.setTimeout(() => menuButtonRef.current?.focus(), 0);
      } else if (ledAbierto) {
        setLedAbierto(false);
        window.setTimeout(() => ledButtonRef.current?.focus(), 0);
      }
    };

    document.addEventListener('pointerdown', cerrarFuera);
    document.addEventListener('keydown', cerrarEscape);
    return () => {
      document.removeEventListener('pointerdown', cerrarFuera);
      document.removeEventListener('keydown', cerrarEscape);
    };
  }, [ledAbierto, menuAbierto]);

  const abrirLed = () => {
    setMenuAbierto(false);
    setLedAbierto((abierto) => !abierto);
  };

  const abrirMenu = () => {
    setLedAbierto(false);
    setMenuAbierto((abierto) => !abierto);
  };

  useEffect(() => {
    if (!menuAbierto) return;
    const id = window.setTimeout(() => {
      menuWrapRef.current?.querySelector<HTMLButtonElement>('.header-more-menu button')?.focus();
    }, 0);
    return () => window.clearTimeout(id);
  }, [menuAbierto]);

  return (
    <header
      className={`chat-header${scrolleo ? ' scrolleo' : ''}`}
      data-state={cara}
      data-personaje={elegido.id}
    >
      <button
        type="button"
        className="volver hamburguesa"
        onClick={onAbrirSidebar}
        title="Ver conversaciones"
        aria-label="Abrir conversaciones"
      >
        <Menu size={19} strokeWidth={2} aria-hidden="true" />
      </button>

      <button
        type="button"
        className="volver volver-inicio"
        onClick={onVolver}
        title="Volver al inicio"
        aria-label="Volver al inicio de Kira"
      >
        <ArrowLeft size={19} strokeWidth={2} aria-hidden="true" />
      </button>

      <div
        className={`avatar${hablando ? ' hablando' : ''}`}
        style={{ background: color }}
        title={`${elegido.nombre} · ${elegido.rol}`}
        aria-hidden="true"
      >
        <Avatar color="#fff" size={22} />
      </div>

      <div className="chat-titulo">
        <h2 title={tituloConversacion}>{tituloConversacion}</h2>

        <AnimatePresence mode="wait" initial={false}>
          <motion.div
            key={estadoVisible}
            className={`mb-status chico ${mbEstado}`}
            title={estadoTitle || undefined}
            aria-live="polite"
            aria-atomic="true"
            initial={{ opacity: 0, y: 2 }}
            animate={{ opacity: 1, y: 0 }}
            exit={{ opacity: 0, y: -2 }}
            transition={{ duration: T.fast, ease: EASE_OUT }}
          >
            <span
              className="mb-dot"
              style={enActividad ? { background: color } : undefined}
              aria-hidden="true"
            />
            <span>{estadoVisible}</span>
          </motion.div>
        </AnimatePresence>
      </div>

      <div className="header-acciones" role="toolbar" aria-label="Acciones del chat">
        <div className="led-btn-wrap" ref={ledWrapRef}>
          <button
            ref={ledButtonRef}
            type="button"
            className="led-btn"
            onClick={abrirLed}
            title="Ver la carita del micro:bit"
            aria-label={`${ledAbierto ? 'Cerrar' : 'Abrir'} carita del micro:bit`}
            aria-expanded={ledAbierto}
            aria-controls={ledPopoverId}
            aria-haspopup="true"
          >
            <Cpu size={16} strokeWidth={2} aria-hidden="true" />
            <span>micro:bit</span>
            <span className={`led-estado ${mbEstado}`} aria-hidden="true" />
          </button>

          <AnimatePresence>
            {ledAbierto && (
              <motion.div
                id={ledPopoverId}
                className="led-popover"
                role="region"
                aria-label="Estado visual del micro:bit"
                initial={{ opacity: 0, scale: 0.96, y: -5 }}
                animate={{ opacity: 1, scale: 1, y: 0 }}
                exit={{ opacity: 0, scale: 0.97, y: -4 }}
                transition={SPRING_POP}
                style={{ transformOrigin: 'top right' }}
              >
                <MicrobitVirtual
                  emotion={cara}
                  color={color}
                  patronReal={patronReal}
                />
                <span className="emocion-label" aria-live="polite">
                  {patronReal ? 'micro:bit · en vivo' : `estado · ${nombreEstado(cara)}`}
                </span>
              </motion.div>
            )}
          </AnimatePresence>
        </div>

        <div className="header-more-wrap" ref={menuWrapRef}>
          <button
            ref={menuButtonRef}
            type="button"
            className="header-more-btn"
            onClick={abrirMenu}
            title="Más opciones"
            aria-label="Abrir más opciones"
            aria-expanded={menuAbierto}
            aria-controls={menuPopoverId}
            aria-haspopup="true"
          >
            <MoreHorizontal size={18} strokeWidth={2} aria-hidden="true" />
          </button>

          {/* El menú queda montado aunque esté oculto: PuenteBle mantiene vivo
              el enlace, Wake Lock y la reconexión durante toda la conversación. */}
          <motion.div
            id={menuPopoverId}
            className="header-more-menu"
            role="dialog"
            aria-label="Kira y dispositivo"
            aria-hidden={!menuAbierto}
            initial={false}
            animate={{
              opacity: menuAbierto ? 1 : 0,
              scale: menuAbierto ? 1 : 0.98,
              y: menuAbierto ? 0 : -4,
            }}
            transition={SPRING_POP}
            style={{
              display: menuAbierto ? 'block' : 'none',
              transformOrigin: 'top right',
            }}
            onClick={(event) => {
              const boton = (event.target as HTMLElement).closest('button');
              if (boton && !boton.classList.contains('puente-ble')) {
                setMenuAbierto(false);
              }
            }}
          >
            <span className="header-menu-label">Kira y dispositivo</span>
            <div className="header-menu-items">{acciones}</div>
          </motion.div>
        </div>
      </div>
    </header>
  );
}
