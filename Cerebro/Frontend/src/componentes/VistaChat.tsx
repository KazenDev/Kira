// Vista completa de una conversación: encabezado, mensajes, botón de bajar,
// modo cara y composer. App solo decide qué personaje está abierto.
import { memo, useMemo } from 'react';
import type { CSSProperties, ChangeEvent, RefObject } from 'react';
import { ArrowDown, Brain, RefreshCw, Bluetooth, BluetoothConnected, Maximize2 } from 'lucide-react';
import { AnimatePresence, motion } from 'motion/react';
import type { MensajeChat, PersonajeInfo } from '../lib/api';
import { T, EASE_OUT } from '../lib/motion';
import ChatHeader from './ChatHeader';
import ListaMensajes from './ListaMensajes';
import Composer from './Composer';
import ModoCara from './ModoCara';

type Props = {
  elegido: PersonajeInfo;
  color: string;
  tituloConversacion: string;
  mensajes: MensajeChat[];
  mensajesRef: RefObject<HTMLElement>;
  autoScroll: boolean;
  scrolleo: boolean;

  // estados del motor
  cara: string;
  streamId: string | null;
  mostrarPuntos: boolean;
  ultimoKiraId?: string;
  respuestaActiva: boolean;
  hablando: boolean;
  escuchando: boolean;
  /** El audio ya se grabó y se está transcribiendo. */
  transcribiendo: boolean;
  nivelEscucha: number;
  segEscucha: number;
  labelTrace: string | null;
  ultimoTexto: string;

  // voz y sugerencias
  vozActiva: boolean;
  sugerencias: string[];
  saludoPantalla: string;
  hayMensajesUsuario: boolean;

  // micro:bit / BLE
  patronReal: string | null;
  mbEstado: string;
  mbTexto: string;
  mbTitulo: string;
  bleEstado: string;
  bleEtiqueta: string;
  bleOcupado: boolean;
  mbConectado: boolean;
  rutaCel: boolean;

  // modales/recarga
  recargando: boolean;
  modoCara: boolean;
  error: string;

  // composer
  input: string;
  foto: string | null;
  procesandoFoto: boolean;
  textareaRef: RefObject<HTMLTextAreaElement>;
  fotoInputRef: RefObject<HTMLInputElement>;

  // acciones
  onAbrirSidebar: () => void;
  onVolver: () => void;
  onInput: (valor: string) => void;
  onElegirFoto: (evento: ChangeEvent<HTMLInputElement>) => void;
  onQuitarFoto: () => void;
  onEnviar: () => void;
  onDetener: () => void;
  onEscuchar: () => void;
  onCancelarEscucha: () => void;
  onEnviarEscucha: () => void;
  onRegenerar: (id: string) => void;
  onReproducir: (urls: string[]) => void;
  onSugerenciaMensaje: (texto: string) => void;
  onReintentar: () => void;
  onScroll: () => void;
  onBajarAlFondo: () => void;
  onAlternarBle: () => void;
  onAbrirModoCara: () => void;
  onCerrarModoCara: () => void;
  onAbrirMemoria: () => void;
  onRecargar: () => void;
  onCosquillas: () => void;
};

function VistaChatInner({
  elegido,
  color,
  tituloConversacion,
  mensajes,
  mensajesRef,
  autoScroll,
  scrolleo,
  cara,
  streamId,
  mostrarPuntos,
  ultimoKiraId,
  respuestaActiva,
  hablando,
  escuchando,
  transcribiendo,
  nivelEscucha,
  segEscucha,
  labelTrace,
  ultimoTexto,
  vozActiva,
  sugerencias,
  saludoPantalla,
  hayMensajesUsuario,
  patronReal,
  mbEstado,
  mbTexto,
  mbTitulo,
  bleEstado,
  bleEtiqueta,
  bleOcupado,
  mbConectado,
  rutaCel,
  recargando,
  modoCara,
  error,
  input,
  foto,
  procesandoFoto,
  textareaRef,
  fotoInputRef,
  onAbrirSidebar,
  onVolver,
  onInput,
  onElegirFoto,
  onQuitarFoto,
  onEnviar,
  onDetener,
  onEscuchar,
  onCancelarEscucha,
  onEnviarEscucha,
  onRegenerar,
  onReproducir,
  onSugerenciaMensaje,
  onReintentar,
  onScroll,
  onBajarAlFondo,
  onAlternarBle,
  onAbrirModoCara,
  onCerrarModoCara,
  onAbrirMemoria,
  onRecargar,
  onCosquillas,
}: Props) {
  // useMemo: `acciones` es JSX; si se recrea en cada render, rompe el memo de
  // ChatHeader aunque su contenido sea idéntico.
  const acciones = useMemo(
    () => (
      <>
        <button
          type="button"
          className="header-menu-item"
          onClick={onAlternarBle}
          disabled={bleOcupado}
        >
          {bleEstado === 'conectado' ? (
            <BluetoothConnected size={16} strokeWidth={2} aria-hidden="true" />
          ) : (
            <Bluetooth size={16} strokeWidth={2} aria-hidden="true" />
          )}
          <span>{bleEtiqueta}</span>
        </button>
        <button type="button" className="header-menu-item" onClick={onAbrirModoCara}>
          <Maximize2 size={16} strokeWidth={2} aria-hidden="true" />
          <span>Modo cara (pantalla completa)</span>
        </button>
        <button type="button" className="header-menu-item" onClick={onAbrirMemoria}>
          <Brain size={16} strokeWidth={2} aria-hidden="true" />
          <span>Recuerdos de Kira</span>
        </button>
        <button
          type="button"
          className="header-menu-item"
          onClick={onRecargar}
          disabled={recargando}
        >
          <RefreshCw
            size={16}
            strokeWidth={2}
            className={recargando ? 'girando' : undefined}
            aria-hidden="true"
          />
          <span>{recargando ? 'Buscando actualización…' : 'Buscar actualización'}</span>
        </button>
      </>
    ),
    [
      bleEstado,
      bleEtiqueta,
      bleOcupado,
      onAlternarBle,
      onAbrirModoCara,
      onAbrirMemoria,
      onRecargar,
      recargando,
    ]
  );

  return (
    <div className="chat" style={{ '--color': color } as CSSProperties}>
      <ChatHeader
        elegido={elegido}
        tituloConversacion={tituloConversacion}
        color={color}
        cara={cara}
        patronReal={patronReal}
        mbEstado={mbEstado}
        mbTexto={mbTexto}
        mbTitulo={mbTitulo}
        scrolleo={scrolleo}
        onAbrirSidebar={onAbrirSidebar}
        onVolver={onVolver}
        acciones={acciones}
      />

      <ListaMensajes
        mensajes={mensajes}
        personajeId={elegido.id}
        nombre={elegido.nombre}
        color={color}
        streamId={streamId}
        mostrarPuntos={mostrarPuntos}
        ultimoKiraId={ultimoKiraId}
        respuestaActiva={respuestaActiva}
        vozActiva={vozActiva}
        hablando={hablando}
        labelTrace={labelTrace}
        sugerencias={sugerencias}
        saludoPantalla={saludoPantalla}
        hayMensajesUsuario={hayMensajesUsuario}
        inputVacio={!input.trim()}
        error={error}
        ultimoTexto={ultimoTexto}
        contenedorRef={mensajesRef}
        onScroll={onScroll}
        onRegenerar={onRegenerar}
        onEscuchar={onReproducir}
        onSugerencia={onSugerenciaMensaje}
        onReintentar={onReintentar}
      />

      {/* botón flotante: volver abajo cuando scrolleaste para arriba */}
      <AnimatePresence>
        {!autoScroll && (
          <motion.button
            type="button"
            className="bajar-btn"
            onClick={onBajarAlFondo}
            initial={{ opacity: 0, y: 8, scale: 0.9 }}
            animate={{ opacity: 1, y: 0, scale: 1 }}
            exit={{ opacity: 0, y: 8, scale: 0.9 }}
            transition={{ duration: T.fast, ease: EASE_OUT }}
            title="Bajar al último mensaje"
            aria-label="Bajar al último mensaje"
          >
            <ArrowDown size={17} strokeWidth={2.2} aria-hidden="true" />
          </motion.button>
        )}
      </AnimatePresence>

      {modoCara && (
        <ModoCara
          subtitulo={
            ultimoKiraId ? (mensajes.find((m) => m.id === ultimoKiraId)?.contenido ?? '') : ''
          }
          estadoTexto={
            escuchando
              ? 'te escucho…'
              : hablando
                ? 'hablando…'
                : mostrarPuntos
                  ? (labelTrace ?? 'pensando…')
                  : 'tocale la cara o hablale'
          }
          escuchando={escuchando}
          puedeHablar={!respuestaActiva}
          onHablar={onEscuchar}
          onCosquillas={onCosquillas}
          onCerrar={onCerrarModoCara}
        />
      )}

      <Composer
        nombre={elegido.nombre}
        input={input}
        onInput={onInput}
        foto={foto}
        onQuitarFoto={onQuitarFoto}
        procesandoFoto={procesandoFoto}
        respuestaActiva={respuestaActiva}
        textareaRef={textareaRef}
        fotoInputRef={fotoInputRef}
        onElegirFoto={onElegirFoto}
        onEnviar={onEnviar}
        onDetener={onDetener}
        onEscuchar={onEscuchar}
        escuchando={escuchando}
        transcribiendo={transcribiendo}
        nivelEscucha={nivelEscucha}
        segEscucha={segEscucha}
        onCancelarEscucha={onCancelarEscucha}
        onEnviarEscucha={onEnviarEscucha}
        mbConectado={mbConectado}
        rutaCel={rutaCel}
      />
    </div>
  );
}

// memo: App re-renderiza en cada tick; la vista solo debe hacerlo si sus
// props cambian de verdad.
const VistaChat = memo(VistaChatInner);
export default VistaChat;
