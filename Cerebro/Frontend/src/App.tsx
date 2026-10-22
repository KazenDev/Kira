import { useCallback, useEffect, useRef, useState } from 'react';
import {
  fetchKira,
  postSilencioso,
  borrarMemoria,
  obtenerPersonajes,
  type PreferenciasMemoria,
  type UsuarioKira,
  PersonajeInfo,
  MensajeChat,
} from './lib/api';
import FondoVivo from './componentes/FondoVivo';
import { kiraVivo } from './lib/vivo';
import PanelSidebar from './componentes/PanelSidebar';
import PuenteBle from './componentes/PuenteBle';
import PantallaInicio from './componentes/PantallaInicio';
import VistaChat from './componentes/VistaChat';
import MemoriaPanel from './componentes/MemoriaPanel';
import { recargarApp } from './lib/recarga';
import { indicadorEscribiendo, labelHerramienta, elegirSugerencias, SALUDOS_KIRA } from './lib/ui-utils';
import Settings from './componentes/Settings';
import { sonidoActivo, setSonidoActivo, sonidoUI } from './lib/sonidos';
import { useAjustes } from './hooks/useAjustes';
import { useChats } from './hooks/useChats';
import { useMicrobit } from './hooks/useMicrobit';
import { useScrollChat } from './hooks/useScrollChat';
import { useMotorConversacion } from './hooks/useMotorConversacion';
import { useTextareaAutoResize } from './hooks/useTextareaAutoResize';
import { useSidebar } from './hooks/useSidebar';
import { useFoto } from './hooks/useFoto';
import { useNavegacionChats } from './hooks/useNavegacionChats';
import { useTitulosChat } from './hooks/useTitulosChat';

type AppProps = {
  usuario: UsuarioKira;
  onCerrarSesion: () => Promise<void>;
};

export default function App({ usuario, onCerrarSesion }: AppProps) {
  const [personajes, setPersonajes] = useState<PersonajeInfo[]>([]);
  const [elegido, setElegido] = useState<PersonajeInfo | null>(null);
  const { chats, setChats } = useChats(usuario.id);
  const [chatActivo, setChatActivo] = useState<string | null>(null);
  const [mensajes, setMensajes] = useState<MensajeChat[]>([]);
  const [input, setInput] = useState('');
  // el botón ⟳ del encabezado: pide versión nueva al SW y recarga la app
  const [recargando, setRecargando] = useState(false);
  const [modoCara, setModoCara] = useState(false); // Kira a pantalla completa (feria)
  // el markdown se baja en un rato libre (ver Burbuja: carga diferida)
  useEffect(() => {
    const precargar = () => void import('./componentes/MensajeMarkdown');
    const ric = (window as unknown as { requestIdleCallback?: (f: () => void) => number }).requestIdleCallback;
    if (ric) ric(precargar);
    else window.setTimeout(precargar, 1500);
  }, []);
  // cada visita: 3 sugerencias distintas y un saludo de pantalla distinto
  const [sugerencias] = useState(() => elegirSugerencias());
  const [saludoPantalla] = useState(
    () => SALUDOS_KIRA[Math.floor(Math.random() * SALUDOS_KIRA.length)]
  );
  const [error, setError] = useState('');
  const errorTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const mostrarErrorTemporal = useCallback((mensaje: string, ms = 3500) => {
    if (errorTimerRef.current) clearTimeout(errorTimerRef.current);
    setError(mensaje);
    errorTimerRef.current = setTimeout(() => {
      setError('');
      errorTimerRef.current = null;
    }, ms);
  }, []);

  const {
    status,
    bleEstado,
    setBleEstado,
    conectadoRef: mbConectadoRef,
    patronReal,
    mbEstado,
    mbTexto,
    mbTitulo,
    bleOcupado,
    bleEtiqueta,
  } = useMicrobit();
  const [memoriaAbierta, setMemoriaAbierta] = useState(false); // panel 🧠 de memoria viva
  const [ajustesAbiertos, setAjustesAbiertos] = useState(false); // modal de ajustes
  const {
    sidebarAbierta,
    sidebarColapsada,
    setSidebarColapsada,
    abrirSidebar,
    cerrarSidebar,
    sidebarReturnFocusRef,
  } = useSidebar();
  const { foto, setFoto, procesandoFoto, fotoInputRef, elegirFoto } =
    useFoto(mostrarErrorTemporal);
  const { ajustes: ajustesLocales, cambiar: cambiarAjuste } = useAjustes(usuario.id);
  const [sonido, setSonido] = useState<boolean>(() => sonidoActivo());

  // El motor y la navegación se necesitan mutuamente: este ref permite que la
  // navegación corte la conversación sin depender del orden de los hooks.
  const detenerConversacionRef = useRef<() => void>(() => {});
  const setEmotionRef = useRef<(emotion: string) => void>(() => {});
  const setHablandoRef = useRef<(v: boolean) => void>(() => {});
  const { generarTituloChat } = useTitulosChat(setChats);
  const nav = useNavegacionChats({
    usuarioId: usuario.id,
    personajes,
    chats,
    setChats,
    elegido,
    setElegido,
    chatActivo,
    setChatActivo,
    mensajes,
    setMensajes,
    detenerConversacion: () => detenerConversacionRef.current(),
    setEmotion: (emotion) => setEmotionRef.current(emotion),
    setMemoriaAbierta,
    setAjustesAbiertos,
    setError,
    setHablando: (v) => setHablandoRef.current(v),
    cerrarSidebar,
  });
  const {
    abrirChat,
    nuevoChat,
    borrarChat,
    borrarTodosLosChats,
    volver,
    totalChats,
    ultimoChat,
    tituloConversacion,
  } = nav;

  const sincronizar = async () => {
    try {
      const r = await fetchKira('/api/sync', { method: 'POST' });
      if (!r.ok) throw new Error(`HTTP ${r.status}`);
      const d = await r.json();
      mostrarErrorTemporal(
        d.comando ? `micro:bit sincronizado → ${d.comando}` : 'micro:bit sincronizado',
        2500
      );
    } catch {
      mostrarErrorTemporal('No se pudo sincronizar (¿el server está corriendo?)', 4500);
    }
  };

  // ---------- SCROLL INTELIGENTE + TEXTAREA QUE CRECE SOLO ----------
  const { mensajesRef, autoScroll, setAutoScroll, scrolleo, revisarScroll, bajarAlFondo } =
    useScrollChat();
  const { textareaRef } = useTextareaAutoResize(input);

  // cargar personajes
  const [cargandoPjs, setCargandoPjs] = useState(true);
  useEffect(() => {
    obtenerPersonajes()
      .then(setPersonajes)
      .catch((e) => setError(String(e.message || e)))
      .finally(() => setCargandoPjs(false));
  }, []);

  // Escape cierra la capa superior sin desaparecer la conversación.
  useEffect(() => {
    if (!ajustesAbiertos && !memoriaAbierta) return;
    const cerrarModal = (event: KeyboardEvent) => {
      if (event.key !== 'Escape') return;
      event.preventDefault();
      if (ajustesAbiertos) {
        sonidoUI('cerrar');
        setAjustesAbiertos(false);
      } else {
        sonidoUI('cerrar');
        setMemoriaAbierta(false);
      }
    };
    document.addEventListener('keydown', cerrarModal);
    return () => document.removeEventListener('keydown', cerrarModal);
  }, [ajustesAbiertos, memoriaAbierta]);

  const {
    escribiendo,
    streamPendiente,
    streamId,
    herramienta,
    emotion,
    setEmotion,
    hablando,
    setHablando,
    escuchando,
    transcribiendo,
    nivelEscucha,
    segEscucha,
    rutaCelRef,
    enviar,
    detenerRespuesta,
    reproducirMensaje,
    escucharHabla,
    cancelarEscucha,
    cortarEscucha,
    enviarEscuchaCel,
    ultimoTextoRef,
  } = useMotorConversacion({
    elegido,
    chatActivo,
    mensajes,
    setMensajes,
    input,
    setInput,
    foto,
    setFoto,
    ajustes: ajustesLocales,
    mbConectadoRef,
    textareaRef,
    setAutoScroll,
    setError,
    mostrarErrorTemporal,
    generarTituloChat,
  });

  detenerConversacionRef.current = () => {
    cortarEscucha();
    detenerRespuesta();
  };
  setEmotionRef.current = setEmotion;
  setHablandoRef.current = setHablando;

  // seguir la conversación solo si el usuario está cerca del fondo.
  useEffect(() => {
    if (!autoScroll) return;
    const raf = requestAnimationFrame(() => bajarAlFondo(false));
    return () => cancelAnimationFrame(raf);
  }, [mensajes, escribiendo, streamPendiente, autoScroll, bajarAlFondo]);

  // Escape corta la respuesta en curso (atajo estándar de los chats IA)
  useEffect(() => {
    if ((!escribiendo && !streamPendiente) || ajustesAbiertos || memoriaAbierta || sidebarAbierta) return;
    const conEscape = (e: KeyboardEvent) => {
      if (e.key !== 'Escape') return;
      const menuAbierto = document.querySelector<HTMLElement>('.header-more-menu');
      if (
        document.querySelector('.msg-acciones-menu') ||
        (menuAbierto && getComputedStyle(menuAbierto).display !== 'none')
      ) {
        return;
      }
      detenerRespuesta();
    };
    document.addEventListener('keydown', conEscape);
    return () => document.removeEventListener('keydown', conEscape);
  }, [
    escribiendo,
    streamPendiente,
    ajustesAbiertos,
    memoriaAbierta,
    sidebarAbierta,
    detenerRespuesta,
  ]);

  const cambiarSonido = (v: boolean) => {
    setSonido(v);
    setSonidoActivo(v);
  };

  const cambiarVoz = (v: boolean) => cambiarAjuste('voz', v);
  const cambiarAutonomia = (v: boolean) => cambiarAjuste('autonomia', v);
  const cambiarFondoVivo = (v: boolean) => cambiarAjuste('fondoVivo', v);
  const cambiarVolumenVoz = (v: number) =>
    cambiarAjuste('volumenVoz', Math.max(0, Math.min(1, v)));
  const cambiarPreferenciasMemoria = (memoria: PreferenciasMemoria) =>
    cambiarAjuste('memoria', { ...memoria });

  const abrirMemoriaDesdeAjustes = () => {
    sonidoUI('abrir');
    setAjustesAbiertos(false);
    setMemoriaAbierta(true);
  };

  const borrarMemoriaDesdeAjustes = async () => {
    const personaje = elegido?.id ?? 'kira';
    const cantidad = await borrarMemoria(personaje, 'memoria borrada desde Ajustes');
    mostrarErrorTemporal(
      cantidad === 1 ? 'Se olvidó 1 recuerdo' : `Se olvidaron ${cantidad} recuerdos`,
      3000
    );
  };

  const alternarBleDesdeAjustes = () => {
    window.dispatchEvent(new Event('kira-ble-toggle'));
  };

  // la cara del micro:bit virtual: loading mientras piensa, talk al hablar, sino la emoción
  const cara = hablando ? 'talk' : streamPendiente ? 'loading' : emotion;

  // RECARGAR LA APP (⟳ del encabezado): reemplaza al "deslizá para recargar"
  // del celular — ese gesto se perdió al bloquear el scroll del documento
  // (y en la PWA instalada nunca existió, no hay barra del navegador).
  const recargar = useCallback(async () => {
    setRecargando(true);
    try {
      await recargarApp();
    } finally {
      // si el reload no llegó a dispararse, que el botón no quede girando para siempre
      setTimeout(() => setRecargando(false), 1500);
    }
  }, []);

  // Handlers ESTABLES: `enviar` y `escucharHabla` se recrean en cada render
  // (dependen de `mensajes`, que cambia 30 fps durante el streaming). Si se
  // pasaran directo, CUALQUIER memo de abajo se rompe y el historial entero se
  // re-pinta en cada tick. Los refs guardan la versión viva y el handler queda
  // con identidad fija.
  const enviarVivoRef = useRef(enviar);
  enviarVivoRef.current = enviar;
  const escucharVivoRef = useRef(escucharHabla);
  escucharVivoRef.current = escucharHabla;
  const enviarEscuchaVivoRef = useRef(enviarEscuchaCel);
  enviarEscuchaVivoRef.current = enviarEscuchaCel;
  const escuchandoRef = useRef(escuchando);
  escuchandoRef.current = escuchando;

  const regenerar = useCallback((id: string) => {
    void enviarVivoRef.current(undefined, id);
  }, []);
  const enviarSugerencia = useCallback((texto: string) => {
    void enviarVivoRef.current(texto);
  }, []);
  const reintentar = useCallback(() => {
    setError('');
    void enviarVivoRef.current(ultimoTextoRef.current);
  }, [ultimoTextoRef, setError]);
  const hablarOEnviar = useCallback(() => {
    if (escuchandoRef.current) enviarEscuchaVivoRef.current();
    else void escucharVivoRef.current();
  }, []);

  // El resto de handlers también estables: sin esto, Composer y ChatHeader se
  // re-renderizan 30 veces por segundo durante el streaming (los props cambian
  // de identidad aunque su valor sea el mismo).
  const quitarFoto = useCallback(() => setFoto(null), [setFoto]);
  const enviarActual = useCallback(() => {
    void enviarVivoRef.current();
  }, []);
  const alternarBle = useCallback(() => {
    window.dispatchEvent(new Event('kira-ble-toggle'));
  }, []);
  const abrirModoCara = useCallback(() => setModoCara(true), []);
  const cerrarModoCara = useCallback(() => setModoCara(false), []);
  const abrirMemoriaPanel = useCallback(() => {
    sonidoUI('abrir');
    setMemoriaAbierta(true);
  }, []);
  const bajarYFijar = useCallback(() => {
    setAutoScroll(true);
    bajarAlFondo(true);
  }, [setAutoScroll, bajarAlFondo]);
  const abrirAjustesDesdeSidebar = useCallback(() => {
    sonidoUI('abrir');
    cerrarSidebar();
    setAjustesAbiertos(true);
  }, [cerrarSidebar]);
  const volverDesdeSidebar = useCallback(() => {
    cerrarSidebar();
    sidebarReturnFocusRef.current = null;
    volver();
    requestAnimationFrame(() => {
      document.querySelector<HTMLButtonElement>('.inicio-menu')?.focus();
    });
  }, [cerrarSidebar, volver, sidebarReturnFocusRef]);
  const cerrarAjustes = useCallback(() => {
    sonidoUI('cerrar');
    setAjustesAbiertos(false);
  }, []);
  const cerrarMemoria = useCallback(() => {
    sonidoUI('cerrar');
    setMemoriaAbierta(false);
  }, []);
  const cosquillas = useCallback(() => {
    void postSilencioso('/api/comando', {
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ cmd: 'REACCION:1' }),
    });
  }, []);

  const color = elegido?.color ?? '#a3e635';

  // Inicio single-character: abre una charla nueva y, si vino de una
  // sugerencia, deja el prompt listo en el composer.
  //
  // OJO con las dependencias: `personajes` llega VACIO en el primer render
  // (se llena cuando responde /api/personajes). Si esto se memorizara con
  // deps vacias, quedaria congelado con la lista vacia para siempre y el
  // boton "Empezar a hablar" no haria nada (el `if (!kira) return` cortaba).
  const comenzarDesdeInicio = useCallback(
    (prompt?: string) => {
      const kira = personajes[0];
      if (!kira) return;
      if (prompt) setInput(prompt);
      nuevoChat(kira);
    },
    [personajes, nuevoChat, setInput]
  );
  const empezarNuevo = useCallback(() => comenzarDesdeInicio(), [comenzarDesdeInicio]);

  const hayMensajesUsuario = mensajes.some((m) => m.rol === 'usuario');
  const ultimoKiraId = [...mensajes].reverse().find((m) => m.rol !== 'usuario')?.id;
  const respuestaActiva = escribiendo || streamPendiente;

  // ¿Qué indicador de "escribiendo" corresponde? Los puntitos mientras el
  // parcial está vacío, la burbuja con cursor apenas llega texto: NUNCA los
  // dos juntos (era el bug de los dos "escribiendo…" en pantalla).
  const indicador = indicadorEscribiendo(respuestaActiva, streamId, mensajes, elegido?.id);
  const mostrarPuntos = indicador === 'puntos';
  // qué dice el indicador mientras piensa: estado HONESTO (tool nombrada si
  // la hay, "pensando…" si todavía no hace nada visible)
  const labelTrace = labelHerramienta(herramienta);

  // LA CARA EN VIVO (header, hero, indicador): estado REAL, nunca intencion.
  // Escucha solo si el mic graba de verdad; habla solo si suena audio.
  kiraVivo.estado = escuchando ? 'escucha' : hablando ? 'habla' : mostrarPuntos ? 'piensa' : 'reposo';
  kiraVivo.emocion = emotion;
  kiraVivo.herramienta = herramienta;
  kiraVivo.nivel = Math.min(1, nivelEscucha / 60);
  if (!hablando) kiraVivo.boca = 0;

  return (
    // el data-pj activa el MODO del personaje: Kira tiñe todo de rojo (Among Us)
    <div
      className={`app${sidebarColapsada ? ' sidebar-colapsada' : ''}`}
      data-pj={elegido?.id}
    >
      {/* El puente BLE vive aquí durante toda la sesión, incluso al volver
          al inicio; su botón visual puede montarse y desmontarse en el menú. */}
      <PuenteBle oculto onEstado={setBleEstado} />
      {ajustesLocales.fondoVivo && <FondoVivo />}
      <PanelSidebar
        personajes={personajes}
        chats={chats}
        elegido={elegido}
        chatActivo={chatActivo}
        mbEstado={mbEstado}
        mbTexto={mbTexto}
        mbTitulo={mbTitulo}
        abierta={sidebarAbierta}
        colapsada={sidebarColapsada}
        setColapsada={setSidebarColapsada}
        onAbrirChat={abrirChat}
        onBorrarChat={borrarChat}
        onNuevoChat={nuevoChat}
        onSincronizar={sincronizar}
        onAbrirAjustes={abrirAjustesDesdeSidebar}
        onVolver={volverDesdeSidebar}
        onCerrar={cerrarSidebar}
      />

      {/* modal de ajustes: sonido, memoria/privacidad, chats, dispositivo y diagnóstico */}
      <Settings
        abierto={ajustesAbiertos}
        onCerrar={cerrarAjustes}
        sonido={sonido}
        onCambiarSonido={cambiarSonido}
        voz={ajustesLocales.voz}
        onCambiarVoz={cambiarVoz}
        autonomia={ajustesLocales.autonomia}
        onCambiarAutonomia={cambiarAutonomia}
        fondoVivo={ajustesLocales.fondoVivo}
        onCambiarFondoVivo={cambiarFondoVivo}
        volumenVoz={ajustesLocales.volumenVoz}
        onCambiarVolumenVoz={cambiarVolumenVoz}
        preferenciasMemoria={ajustesLocales.memoria}
        onCambiarPreferenciasMemoria={cambiarPreferenciasMemoria}
        totalChats={totalChats}
        onBorrarTodos={borrarTodosLosChats}
        onAbrirMemoria={abrirMemoriaDesdeAjustes}
        onBorrarMemoria={borrarMemoriaDesdeAjustes}
        estadoMicrobit={status}
        bleEstado={bleEstado}
        onAlternarBle={alternarBleDesdeAjustes}
        usuario={usuario}
        onCerrarSesion={onCerrarSesion}
      />
      <MemoriaPanel
        abierto={memoriaAbierta}
        onCerrar={cerrarMemoria}
        personajeId={elegido?.id ?? 'kira'}
        nombre={elegido?.nombre ?? 'Kira'}
        color={color}
      />

      {/* ============ PANEL PRINCIPAL ============ */}
      <main className="panel">
        {/* ---------- Bienvenida / selección ---------- */}
        {!elegido && (
          <PantallaInicio
            cargandoPjs={cargandoPjs}
            personajes={personajes}
            ultimoChat={ultimoChat}
            sugerencias={sugerencias}
            error={error}
            onAbrirSidebar={abrirSidebar}
            onAbrirChat={abrirChat}
            onEmpezarNuevo={empezarNuevo}
            onSugerencia={comenzarDesdeInicio}
          />
        )}

        {/* ---------- Chat ---------- */}
        {elegido && (
          <VistaChat
            elegido={elegido}
            color={color}
            tituloConversacion={tituloConversacion}
            mensajes={mensajes}
            mensajesRef={mensajesRef}
            autoScroll={autoScroll}
            scrolleo={scrolleo}
            cara={cara}
            streamId={streamId}
            mostrarPuntos={mostrarPuntos}
            ultimoKiraId={ultimoKiraId}
            respuestaActiva={respuestaActiva}
            hablando={hablando}
            escuchando={escuchando}
            transcribiendo={transcribiendo}
            nivelEscucha={nivelEscucha}
            segEscucha={segEscucha}
            labelTrace={labelTrace}
            ultimoTexto={ultimoTextoRef.current}
            vozActiva={ajustesLocales.voz}
            sugerencias={sugerencias}
            saludoPantalla={saludoPantalla}
            hayMensajesUsuario={hayMensajesUsuario}
            patronReal={patronReal}
            mbEstado={mbEstado}
            mbTexto={mbTexto}
            mbTitulo={mbTitulo}
            bleEstado={bleEstado}
            bleEtiqueta={bleEtiqueta}
            bleOcupado={bleOcupado}
            mbConectado={mbConectadoRef.current}
            rutaCel={rutaCelRef.current}
            recargando={recargando}
            modoCara={modoCara}
            error={error}
            input={input}
            foto={foto}
            procesandoFoto={procesandoFoto}
            textareaRef={textareaRef}
            fotoInputRef={fotoInputRef}
            onAbrirSidebar={abrirSidebar}
            onVolver={volver}
            onInput={setInput}
            onElegirFoto={elegirFoto}
            onQuitarFoto={quitarFoto}
            onEnviar={enviarActual}
            onDetener={detenerRespuesta}
            onEscuchar={hablarOEnviar}
            onCancelarEscucha={cancelarEscucha}
            onEnviarEscucha={enviarEscuchaCel}
            onRegenerar={regenerar}
            onReproducir={reproducirMensaje}
            onSugerenciaMensaje={enviarSugerencia}
            onReintentar={reintentar}
            onScroll={revisarScroll}
            onBajarAlFondo={bajarYFijar}
            onAlternarBle={alternarBle}
            onAbrirModoCara={abrirModoCara}
            onCerrarModoCara={cerrarModoCara}
            onAbrirMemoria={abrirMemoriaPanel}
            onRecargar={recargar}
            onCosquillas={cosquillas}
          />
        )}
      </main>
    </div>
  );
}



