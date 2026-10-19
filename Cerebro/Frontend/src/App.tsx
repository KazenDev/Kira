import { useCallback, useEffect, useMemo, useRef, useState } from 'react';
import type { CSSProperties, ChangeEvent } from 'react';
import { Mic, ArrowUp, ArrowDown, ArrowRight, Square, Brain, RefreshCw, Camera, Check, X, Menu, Plus, Bluetooth, BluetoothConnected, Maximize2 } from 'lucide-react';
import { AnimatePresence, motion } from 'motion/react';
import {
  enviarMensajeStream,
  fetchKira,
  borrarMemoria,
  escucharVoz,
  transcribirDesdeCel,
  horaActual,
  obtenerPersonajes,
  obtenerStatus,
  generarTitulo,
  EstadoMicrobit,
  type PreferenciasMemoria,
  type UsuarioKira,
  PREFERENCIAS_MEMORIA_POR_DEFECTO,
  PersonajeInfo,
  MensajeChat,
  obtenerEventos,
} from './api';
import KiraCara from './KiraCara';
import FondoVivo from './FondoVivo';
import ModoCara from './ModoCara';
import { bocaDeLetra, kiraVivo } from './vivo';
import Sidebar from './Sidebar';
import ChatHeader from './ChatHeader';
import PuenteBle, { type EstadoPuenteBle } from './PuenteBle';
import MemoriaPanel from './MemoriaPanel';
import { recargarApp } from './recarga';
import { indicadorEscribiendo, labelHerramienta, estadoMicrobit, textoMicrobit, elegirSugerencias, SALUDOS_KIRA, uuid } from './ui-utils';
import { T, EASE_OUT } from './motion';
import Burbuja from './Burbuja';
import Settings from './Settings';
import { sonidoEnviar, sonidoRecibir, sonidoActivo, setSonidoActivo, sonidoUI } from './sonidos';
import { fotoDesdeArchivo } from './foto';
import { ChatSesion, tituloSinEmoji, horaLista } from './tipos';

type ChatsPorPersonaje = Record<string, ChatSesion[]>;
type AudioEnCola = { url: string; orden: number; texto?: string };
// [segundo, letras visibles ya dichas] de un clip (ver /api/tts-tiempos)
type MarcasVoz = [number, number][];
// El clip que suena AHORA, para escribir el texto al ritmo de la voz: arranca
// en la letra `base` del mensaje y cubre `largo` letras.
type ClipVoz = { base: number; largo: number; marcas: MarcasVoz | null };
// Letras por segundo de la voz de Kira (medido: 17-18): mientras el primer
// clip suena en streaming todavia no hay marcas, y se estima un pelin abajo
const LETRAS_POR_SEG = 16;

type AppProps = {
  usuario: UsuarioKira;
  onCerrarSesion: () => Promise<void>;
};

const STORAGE_CHATS = 'kira_chats_v2';
const MAX_TEXTAREA_HEIGHT = 132;
const STATUS_POLL_MS = 3000;
const LIMITE_GRABACION_MS = 60_000;

// reloj m:ss para el cronómetro de la grabación (estilo ChatGPT: 0:07)
function fmtReloj(segundos: number): string {
  const m = Math.floor(segundos / 60);
  const s = Math.floor(segundos % 60);
  return `${m}:${String(s).padStart(2, '0')}`;
}

// factores por barra del waveform: una campana suave para que parezca VOZ
// y no ruido al azar (16 barras, 0.5 = bajita, 1.3 = la más alta)
const FACTORES_BARRAS = [0.5, 0.8, 1.1, 0.9, 1.2, 0.7, 1.0, 1.3, 0.9, 1.15, 0.7, 1.0, 1.2, 0.8, 0.95, 0.55];

const claveActual = (userId: string, id: string) => `kira_chat_activo_${userId}_${id}`;
const STORAGE_AJUSTES = 'kira_ajustes_v1';

type AjustesLocales = {
  voz: boolean;
  volumenVoz: number;
  memoria: PreferenciasMemoria;
  autonomia: boolean;   // Kira habla sola (eventos de la placa, silencios)
  fondoVivo: boolean;   // el resplandor WebGL de fondo (apagable en celus viejos)
};

const AJUSTES_INICIALES: AjustesLocales = {
  voz: true,
  volumenVoz: 0.8,
  autonomia: true,
  fondoVivo: true,
  memoria: { ...PREFERENCIAS_MEMORIA_POR_DEFECTO },
};

function cargarAjustes(userId: string): AjustesLocales {
  try {
    const raw = localStorage.getItem(`${STORAGE_AJUSTES}_${userId}`);
    if (!raw) return { ...AJUSTES_INICIALES, memoria: { ...AJUSTES_INICIALES.memoria } };
    const parsed = JSON.parse(raw) as Partial<AjustesLocales>;
    const memoria = parsed.memoria && typeof parsed.memoria === 'object'
      ? parsed.memoria as Partial<PreferenciasMemoria>
      : {};
    const volumen = Number(parsed.volumenVoz);
    return {
      voz: parsed.voz !== false,
      autonomia: parsed.autonomia !== false,
      fondoVivo: parsed.fondoVivo !== false,
      volumenVoz: Number.isFinite(volumen) ? Math.max(0, Math.min(1, volumen)) : AJUSTES_INICIALES.volumenVoz,
      memoria: {
        recordar: memoria.recordar !== false,
        hechos: memoria.hechos !== false,
        experiencias: memoria.experiencias !== false,
        usar: memoria.usar !== false,
      },
    };
  } catch {
    return { ...AJUSTES_INICIALES, memoria: { ...AJUSTES_INICIALES.memoria } };
  }
}

function guardarAjustes(userId: string, valores: AjustesLocales) {
  try {
    localStorage.setItem(`${STORAGE_AJUSTES}_${userId}`, JSON.stringify(valores));
  } catch {
    // Ajustes sigue funcionando en memoria si localStorage está bloqueado.
  }
}

function cargarChats(userId: string): ChatsPorPersonaje {
  try {
    const raw = localStorage.getItem(`${STORAGE_CHATS}_${userId}`);
    if (!raw) return {};
    const parsed = JSON.parse(raw);
    return parsed && typeof parsed === 'object' && !Array.isArray(parsed) ? parsed : {};
  } catch {
    return {};
  }
}

function guardarChats(userId: string, chats: ChatsPorPersonaje) {
  try {
    localStorage.setItem(`${STORAGE_CHATS}_${userId}`, JSON.stringify(chats));
  } catch {
    // Si localStorage está lleno/bloqueado, el chat sigue funcionando en memoria.
  }
}

function postSilencioso(url: string, init?: RequestInit) {
  return fetchKira(url, { method: 'POST', ...init }).catch(() => null);
}

// Saludo que cambia con la hora local: pequeño detalle para que se sienta vivo.
function saludoDelDia(): string {
  const h = new Date().getHours();
  if (h >= 5 && h < 12) return '¡Buenos días!';
  if (h >= 12 && h < 19) return '¡Buenas tardes!';
  return '¡Buenas noches!';
}

export default function App({ usuario, onCerrarSesion }: AppProps) {
  const [personajes, setPersonajes] = useState<PersonajeInfo[]>([]);
  const [elegido, setElegido] = useState<PersonajeInfo | null>(null);
  const [chats, setChats] = useState<ChatsPorPersonaje>(() => cargarChats(usuario.id));
  const [chatActivo, setChatActivo] = useState<string | null>(null);
  const [mensajes, setMensajes] = useState<MensajeChat[]>([]);
  const [input, setInput] = useState('');
  // FOTO pendiente de enviar (data URL ya reducido): se ve como miniatura en el
  // composer y viaja con el mensaje. La IA la MIRA (visión de DeepSeek V4.1).
  const [foto, setFoto] = useState<string | null>(null);
  const [procesandoFoto, setProcesandoFoto] = useState(false);
  const [escribiendo, setEscribiendo] = useState(false);
  // El SSE sigue pendiente aunque el TTS ya empezó a sonar. Separa ambos
  // estados para que el primer audio no habilite un segundo envío.
  const [streamPendiente, setStreamPendiente] = useState(false);
  const streamPendienteRef = useRef(false);
  // tool trace: QUÉ herramienta está usando la IA ahora (sensor, web, cuenta…)
  // para mostrar un estado nombrado en vez de "pensando…" genérico
  const [herramienta, setHerramienta] = useState<string | null>(null);
  // el botón ⟳ del encabezado: pide versión nueva al SW y recarga la app
  const [recargando, setRecargando] = useState(false);
  const [escuchando, setEscuchando] = useState(false); // grabando con el mic (GPT)
  const [nivelEscucha, setNivelEscucha] = useState(0);  // fuerza del audio en vivo 0..~90
  const [segEscucha, setSegEscucha] = useState(0);       // cronómetro de la grabación en curso
  const escuchandoRef = useRef(false);
  const [streamId, setStreamId] = useState<string | null>(null); // mensaje parcial en streaming
  const [emotion, setEmotion] = useState('happy');
  const [modoCara, setModoCara] = useState(false); // Kira a pantalla completa (feria)
  // el markdown se baja en un rato libre (ver Burbuja: carga diferida)
  useEffect(() => {
    const precargar = () => void import('./MensajeMarkdown');
    const ric = (window as unknown as { requestIdleCallback?: (f: () => void) => number }).requestIdleCallback;
    if (ric) ric(precargar);
    else window.setTimeout(precargar, 1500);
  }, []);
  // cada visita: 3 sugerencias distintas y un saludo de pantalla distinto
  const [sugerencias] = useState(() => elegirSugerencias());
  const [saludoPantalla] = useState(
    () => SALUDOS_KIRA[Math.floor(Math.random() * SALUDOS_KIRA.length)]
  );
  const [hablando, setHablando] = useState(false); // la boca habla (TALK)
  const [error, setError] = useState('');
  const [status, setStatus] = useState<EstadoMicrobit | null>(null);
  const [bleEstado, setBleEstado] = useState<EstadoPuenteBle>('desconectado');
  const [memoriaAbierta, setMemoriaAbierta] = useState(false); // panel 🧠 de memoria viva
  const [sidebarAbierta, setSidebarAbierta] = useState(false); // solo móvil
  const [sidebarColapsada, setSidebarColapsada] = useState(false); // desktop: plegada a iconos
  const [ajustesAbiertos, setAjustesAbiertos] = useState(false); // modal de ajustes
  const [ajustesLocales, setAjustesLocales] = useState<AjustesLocales>(() => cargarAjustes(usuario.id));
  const [sonido, setSonido] = useState<boolean>(() => sonidoActivo());
  const sidebarReturnFocusRef = useRef<HTMLElement | null>(null);
  const titulosEnCursoRef = useRef<Set<string>>(new Set()); // sesiones con titulo en generacion
  const textareaRef = useRef<HTMLTextAreaElement | null>(null);
  const fotoInputRef = useRef<HTMLInputElement | null>(null); // la cámara del celular
  const mensajesRef = useRef<HTMLElement | null>(null);
  const abortRef = useRef<AbortController | null>(null); // corta el stream (botón stop)
  const errorTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const escuchaSesionRef = useRef(0); // invalida callbacks viejos al cambiar de chat
  const respuestaSesionRef = useRef(0); // invalida callbacks de streams viejos
  const limiteGrabacionRef = useRef<ReturnType<typeof setTimeout> | null>(null);

  const mostrarErrorTemporal = useCallback((mensaje: string, ms = 3500) => {
    if (errorTimerRef.current) clearTimeout(errorTimerRef.current);
    setError(mensaje);
    errorTimerRef.current = setTimeout(() => {
      setError('');
      errorTimerRef.current = null;
    }, ms);
  }, []);

  // ---------- SCROLL INTELIGENTE: seguir el texto nuevo salvo que el
  // usuario haya scrolleado hacia arriba a leer algo ----------
  const [autoScroll, setAutoScroll] = useState(true);
  const [scrolleo, setScrolleo] = useState(false); // el header reacciona al scroll

  const revisarScroll = useCallback(() => {
    const el = mensajesRef.current;
    if (!el) return;
    const alFondo = el.scrollHeight - el.scrollTop - el.clientHeight < 90;
    setAutoScroll(alFondo);
    setScrolleo(el.scrollTop > 8);
  }, []);

  const bajarAlFondo = useCallback((suave = true) => {
    const el = mensajesRef.current;
    if (!el) return;
    el.scrollTo({ top: el.scrollHeight, behavior: suave ? 'smooth' : 'auto' });
  }, []);

  // el textarea crece solo con el texto (hasta ~5 lineas), como los chats modernos
  const autoResize = useCallback(() => {
    const el = textareaRef.current;
    if (!el) return;
    el.style.height = 'auto';
    el.style.height = Math.min(el.scrollHeight, MAX_TEXTAREA_HEIGHT) + 'px';
  }, []);

  useEffect(() => {
    autoResize();
  }, [input, autoResize]);

  const abrirSidebar = useCallback(() => {
    sidebarReturnFocusRef.current = document.activeElement as HTMLElement | null;
    setSidebarAbierta(true);
  }, []);

  const cerrarSidebar = useCallback(() => {
    setSidebarAbierta(false);
    requestAnimationFrame(() => {
      const anterior = sidebarReturnFocusRef.current;
      if (anterior?.isConnected) {
        anterior.focus();
      } else if (anterior) {
        document.querySelector<HTMLButtonElement>('.chat-header .hamburguesa, .inicio-menu')?.focus();
      }
    });
  }, []);

  useEffect(() => {
    if (!sidebarAbierta) return;
    const id = window.setTimeout(() => {
      document.querySelector<HTMLButtonElement>('.sidebar-cerrar-movil')?.focus();
    }, 0);
    return () => window.clearTimeout(id);
  }, [sidebarAbierta]);

  // ---------- typewriter REAL: revela el texto de a poco aunque el navegador
  // bufferize el stream completo de una (garantiza el efecto palabra por palabra) ----------
  const streamIdRef = useRef<string | null>(null);         // id del mensaje parcial actual
  const ultimoTextoRef = useRef('');                       // último texto enviado (botón reintentar)
  const mbConectadoRef = useRef(false);                    // ¿hay serial? (elige la ruta de voz)
  const streamObjetivoRef = useRef('');                    // texto completo que va llegando
  const streamReveladoRef = useRef(0);                     // chars ya mostrados
  const streamTimerRef = useRef<number | null>(null);      // interval del typewriter
  const streamFinRef = useRef<(() => void) | null>(null);  // completar cuando termine de revelar

  // cargar personajes
  const [cargandoPjs, setCargandoPjs] = useState(true);
  useEffect(() => {
    obtenerPersonajes()
      .then(setPersonajes)
      .catch((e) => setError(String(e.message || e)))
      .finally(() => setCargandoPjs(false));
  }, []);

  // polling del estado del micro:bit (cada 3s) -> verificación REAL
  useEffect(() => {
    let vivo = true;
    const tick = async () => {
      try {
        const s = await obtenerStatus();
        if (vivo) {
          setStatus(s);
          // para decidir la ruta de voz sin recrear el callback cada 3s
          mbConectadoRef.current = s.microbit.conectado;
        }
      } catch {
        /* backend caído, se mantiene el último estado */
      }
    };
    tick();
    const id = setInterval(tick, STATUS_POLL_MS);
    return () => {
      vivo = false;
      clearInterval(id);
    };
  }, []);

  // El drawer móvil se comporta como una capa real: Escape siempre lo cierra.
  useEffect(() => {
    if (!sidebarAbierta) return;
    const tecladoDrawer = (event: KeyboardEvent) => {
      if (event.key === 'Tab') {
        const focusables = Array.from(
          document.querySelectorAll<HTMLElement>(
            '.sidebar button:not([disabled]), .sidebar input:not([disabled]), .sidebar [href], .sidebar [tabindex]:not([tabindex="-1"])'
          )
        ).filter((el) => el.getClientRects().length > 0);
        if (!focusables.length) return;
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
      if (event.key === 'Escape') {
        event.preventDefault();
        event.stopImmediatePropagation();
        cerrarSidebar();
      }
    };
    document.addEventListener('keydown', tecladoDrawer);
    return () => document.removeEventListener('keydown', tecladoDrawer);
  }, [sidebarAbierta, cerrarSidebar]);

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

  // persistir los chats cuando cambian
  useEffect(() => {
    guardarChats(usuario.id, chats);
  }, [chats, usuario.id]);

  // sincronizar los mensajes actuales con la sesion activa (se guardan en chats)
  useEffect(() => {
    if (!elegido || !chatActivo) return;
    setChats((c) => ({
      ...c,
      [elegido.id]: (c[elegido.id] ?? []).map((s) => {
        if (s.id !== chatActivo) return s;
        // el orden del sidebar solo cambia cuando llega un mensaje NUEVO,
        // no cuando simplemente se abre un chat
        const ultimoNuevo = mensajes[mensajes.length - 1]?.id;
        const ultimoViejo = s.mensajes[s.mensajes.length - 1]?.id;
        const huboNuevo = ultimoNuevo !== ultimoViejo;
        return {
          ...s,
          mensajes,
          fecha: huboNuevo ? new Date().toISOString() : s.fecha,
        };
      }),
    }));
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mensajes, chatActivo]);

  // seguir la conversación solo si el usuario está cerca del fondo.
  // Commiteado por requestAnimationFrame: 33 scrollTo/segundo a pelo pelean
  // con el compositor del navegador en el cel; por frame va suave.
  useEffect(() => {
    if (!autoScroll) return;
    const raf = requestAnimationFrame(() => bajarAlFondo(false));
    return () => cancelAnimationFrame(raf);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [mensajes, escribiendo, streamPendiente]);

  // TIMER del calla anticipado (compensa la latencia BLE al cerrar la boca)
  const callaTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  // COLA DE VOZ (TTS por frases): los audios llegan ordenados durante el
  // stream (evento 'audio') + la lista final del 'fin'; se reproducen
  // encadenados. vistosAudio evita duplicados entre ambas fuentes.
  const colaAudioRef = useRef<AudioEnCola[]>([]);
  const vistosAudioRef = useRef<Set<string>>(new Set());
  const ordenAudioRef = useRef(0); // proximo orden esperado al reproducir
  const reproduciendoAudioRef = useRef(false);
  // AUDIO PERSISTENTE + DESBLOQUEO POR GESTO 🔊: Android/iOS BLOQUEAN el
  // play() programático si el usuario no tocó la página — la respuesta de
  // Kira llega segundos después de apretar enviar y el permiso ya caducó
  // (voz muda + nunca se mandaba TALK al micro:bit). Un UNICO elemento de
  // audio, desbloqueado al primer toque, reutilizado para siempre.
  const audioElRef = useRef<HTMLAudioElement | null>(null);
  // cada reproduccion tiene su numero: el vigia de un audio viejo (cortado con
  // stop o reemplazado por la frase siguiente) se da cuenta y no toca nada
  const turnoAudioRef = useRef(0);
  const vozActivadaRef = useRef(ajustesLocales.voz);
  const volumenVozRef = useRef(ajustesLocales.volumenVoz);

  useEffect(() => {
    vozActivadaRef.current = ajustesLocales.voz;
    volumenVozRef.current = ajustesLocales.volumenVoz;
    const audio = audioElRef.current;
    if (audio) audio.volume = ajustesLocales.volumenVoz;
    if (!ajustesLocales.voz && audio && !audio.paused) {
      turnoAudioRef.current += 1; // apagaron la voz: el vigia no sigue la cola
      audio.pause();
      colaAudioRef.current = [];
      reproduciendoAudioRef.current = false;
      limiteVozRef.current = Infinity; // el texto ya no espera a la voz
      clipVozRef.current = null;
      setEscribiendo(false);
      setHablando(false);
      postSilencioso('/api/calla');
    }
  }, [ajustesLocales.voz, ajustesLocales.volumenVoz]);

  useEffect(() => {
    const el = new Audio();
    el.preload = 'auto';
    audioElRef.current = el;
    let desbloqueado = false;
    const desbloquear = () => {
      if (desbloqueado || !audioElRef.current) return;
      const a = audioElRef.current;
      a.muted = true;
      a.play()
        .then(() => {
          a.pause();
          a.currentTime = 0;
          a.muted = false;
          desbloqueado = true;
        })
        .catch(() => {
          a.muted = false;
        });
    };
    document.addEventListener('pointerdown', desbloquear, { capture: true });
    return () => document.removeEventListener('pointerdown', desbloquear, { capture: true });
  }, []);

  // siguePara(): ¿ya hay OTRO clip de esta respuesta esperando? Si lo hay, al
  // terminar este clip NO se manda CALLA: antes la boca del micro:bit se
  // cerraba entre frase y frase (justo en los "..."), y se veia y se oia
  // como que la voz se cortaba y volvia.
  const jugarAudio = useCallback((url: string, alTerminar?: () => void, siguePara?: () => boolean) => {
    const audio = audioElRef.current ?? new Audio();
    audioElRef.current = audio;
    const miTurno = ++turnoAudioRef.current;
    if (!audio.paused) audio.pause();
    if (!vozActivadaRef.current) {
      // La respuesta textual/TTS ya llegó, pero el usuario apagó la voz:
      // no dejamos la placa esperando un TALK que nunca sonará.
      setEscribiendo(false);
      setHablando(false);
      postSilencioso('/api/calla');
      alTerminar?.();
      return;
    }
    audio.src = url; // el MISMO elemento ya desbloqueado por el gesto
    audio.muted = false;
    audio.volume = volumenVozRef.current;
    // SYNC CON LA LATENCIA BLE (200-500ms): el TALK sale APENAS se pide el
    // audio (no cuando arranca): mientras el mp3 se descarga y decodifica
    // en el cel (~300-800ms), el comando viaja por el relay en paralelo y
    // AMBOS aterrizan juntos. Sin esto la boca siempre llegaba tarde.
    postSilencioso('/api/talk');
    // la boca del micro:bit se sincroniza con el audio REAL
    audio.onplaying = () => {
      setEscribiendo(false); // el TTS ya confirmó: se termina el loading
      setHablando(true);
    };
    // al terminar la voz, la boca CALLA al instante y la cara queda quieta
    // en su emocion (antes la boca quedaba en bucle infinito hasta el
    // proximo comando: la cara hablaba sola en silencio)
    let terminado = false;
    let vigia = 0;
    const terminarAudio = () => {
      if (terminado) return; // ended + error + vigia: una sola vez
      terminado = true;
      window.clearInterval(vigia);
      setEscribiendo(false);
      setHablando(false);
      if (callaTimerRef.current) {
        clearTimeout(callaTimerRef.current);
        callaTimerRef.current = null;
      }
      if (!siguePara?.()) postSilencioso('/api/calla');
      alTerminar?.(); // cadena: TTS por frases -> suena la siguiente
    };
    // CALLA ANTICIPADO: el elemento conoce su duracion; programamos el
    // calla 350ms ANTES del final para que aterrice justo cuando muere la
    // voz (compensa el relay). Solo para audios de mas de 0.8s.
    audio.onloadedmetadata = () => {
      if (callaTimerRef.current) clearTimeout(callaTimerRef.current);
      const ms = (audio.duration || 0) * 1000;
      // en streaming la duracion es Infinity: setTimeout(Infinity) dispara YA
      // (cerraba la boca al arrancar); ahi alcanza con el CALLA del onended
      if (Number.isFinite(ms) && ms > 800) {
        callaTimerRef.current = setTimeout(() => {
          callaTimerRef.current = null;
          if (!siguePara?.()) postSilencioso('/api/calla');
        }, ms - 350);
      }
    };
    audio.onended = terminarAudio;
    audio.onerror = terminarAudio;
    // VIGIA: a veces el navegador deja el audio TRABADO a mitad de una frase
    // sin avisar ni 'ended' ni 'error' (paso en vivo: quedo en "¿Vos los ha"
    // y el texto y la cola de voz esperaron para siempre). Si no avanza, se
    // da por terminado y la charla sigue (el texto se completa solo).
    let ultimoT = -1;
    let ultimoAvance = Date.now();
    vigia = window.setInterval(() => {
      if (turnoAudioRef.current !== miTurno) {
        window.clearInterval(vigia); // otra frase o un stop: ya no es mio
        return;
      }
      const t = audio.currentTime;
      if (document.hidden || t !== ultimoT) {
        ultimoT = t;
        ultimoAvance = Date.now();
        return;
      }
      // antes de arrancar se banca la carga (streaming); ya sonando, menos
      const paciencia = t > 0 ? 3500 : 9000;
      if (Date.now() - ultimoAvance > paciencia) {
        console.warn('[voz] audio trabado: lo salteo', url);
        audio.pause();
        terminarAudio();
      }
    }, 500);
    // si el audio NO puede arrancar (autoplay bloqueado, TTS vacio, etc) el
    // loading no debe quedarse colgado: cortamos escribiendo. Un timeout de
    // respaldo por si ni onplaying ni onerror llegan.
    audio.play().catch(() => terminarAudio());
    const t = setTimeout(() => {
      setEscribiendo(false);
    }, 30000); // respaldo absoluto: nunca quedarse en loading
    audio.addEventListener('playing', () => clearTimeout(t), { once: true });
    audio.addEventListener('ended', () => clearTimeout(t), { once: true });
  }, []);

  // AVANZAR LA COLA DE VOZ: reproduce el siguiente chunk en orden (si ya
  // llegó), con prefetch del siguiente mientras suena (calienta el caché
  // del server: la generación Fish del próximo arranca antes).
  // Cada clip se baja entero y DESPUES sus tiempos (el server los tiene recien
  // cuando termino de generar el audio). Sin tiempos -> null: ritmo parejo.
  const blobsAudioRef = useRef<Map<string, Promise<{ blob: Blob; marcas: MarcasVoz | null }>>>(
    new Map()
  );
  const obtenerBlobAudio = useCallback((url: string) => {
    const ya = blobsAudioRef.current.get(url);
    if (ya) return ya;
    const p = fetchKira(url).then(async (r) => {
      if (!r.ok) throw new Error(`audio ${r.status}`);
      const blob = await r.blob();
      let marcas: MarcasVoz | null = null;
      if (url.includes('/api/tts-stream/')) {
        try {
          const t = await fetchKira(url.replace('/api/tts-stream/', '/api/tts-tiempos/'));
          if (t.ok) marcas = ((await t.json()) as { marcas?: MarcasVoz }).marcas ?? null;
        } catch {
          marcas = null;
        }
      }
      return { blob, marcas };
    });
    p.catch(() => blobsAudioRef.current.delete(url));
    blobsAudioRef.current.set(url, p);
    return p;
  }, []);

  // TEXTO AL RITMO DE LA VOZ (como un transcriptor): con la voz prendida el
  // typewriter no pasa de lo que Kira ya DIJO. limiteVoz = letras ya dichas
  // de los clips terminados; clipVoz = el que suena (se mide con currentTime).
  // Infinity = sin tope (voz apagada, sin audio, o ya termino todo).
  const limiteVozRef = useRef<number>(Infinity);
  const clipVozRef = useRef<ClipVoz | null>(null);
  const finVozRef = useRef(false); // ya llego el 'fin' del stream
  const liberarVoz = useCallback(() => {
    limiteVozRef.current = Infinity;
    clipVozRef.current = null;
  }, []);
  const topeVoz = (): number => {
    const c = clipVozRef.current;
    if (!c) return limiteVozRef.current;
    const a = audioElRef.current;
    const t = a?.currentTime ?? 0;
    let frac = 0;
    const m = c.marcas;
    if (m && m.length > 1 && m[m.length - 1][1] > 0) {
      // interpolado entre marcas (una por palabra): va letra por letra
      let n = m[m.length - 1][1];
      for (let i = 0; i < m.length - 1; i++) {
        if (t < m[i + 1][0]) {
          const tramo = m[i + 1][0] - m[i][0];
          const k = tramo > 0 ? (t - m[i][0]) / tramo : 1;
          n = m[i][1] + (m[i + 1][1] - m[i][1]) * Math.max(0, k);
          break;
        }
      }
      frac = n / m[m.length - 1][1];
    } else if (a && a.duration && Number.isFinite(a.duration)) {
      frac = t / a.duration;
    } else if (c.largo > 0) {
      frac = (t * LETRAS_POR_SEG) / c.largo; // streaming: duracion desconocida
    }
    return c.base + Math.round(Math.min(1, frac) * c.largo);
  };

  const avanzarColaAudio = useCallback(() => {
    // PRECARGA apenas llegan: el TTS se genera recien cuando se pide la URL,
    // asi que una frase que llega mientras suena otra se pide YA (antes se
    // pedia al terminar la anterior y quedaba un hueco de ~1 s)
    colaAudioRef.current
      .filter((c) => c.orden > ordenAudioRef.current && c.orden <= ordenAudioRef.current + 2)
      .forEach((c) => obtenerBlobAudio(c.url));
    if (reproduciendoAudioRef.current) return;
    const cabeza = colaAudioRef.current[0];
    if (!cabeza || cabeza.orden !== ordenAudioRef.current) return;
    colaAudioRef.current.shift();
    reproduciendoAudioRef.current = true;
    const miOrden = cabeza.orden;
    // PRECARGA DE VERDAD: los dos clips siguientes se bajan YA y el blob se
    // GUARDA (antes se descargaba y se tiraba, y al tocarle se volvia a
    // bajar entero: ese era el hueco entre frase y frase).
    colaAudioRef.current
      .filter((c) => c.orden > miOrden && c.orden <= miOrden + 2)
      .forEach((c) => obtenerBlobAudio(c.url));
    const hayOtro = () => colaAudioRef.current.some((c) => c.orden === miOrden + 1);
    // donde empieza este clip en el mensaje (la frase anterior + el espacio)
    const sincroniza = limiteVozRef.current !== Infinity;
    const base = sincroniza ? (limiteVozRef.current === 0 ? 0 : limiteVozRef.current + 1) : 0;
    const largo =
      cabeza.texto != null ? cabeza.texto.length : Math.max(0, streamObjetivoRef.current.length - base);
    const terminoClip = () => {
      // lo de este clip ya se dijo entero; si no viene otro y el stream ya
      // cerro, se suelta el tope (lo que sobre se escribe normal)
      if (limiteVozRef.current !== Infinity) {
        limiteVozRef.current = base + largo;
        clipVozRef.current = null;
        if (!hayOtro() && finVozRef.current) liberarVoz();
      }
      arrancarTypewriter();
    };
    if (miOrden === 0 && !blobsAudioRef.current.has(cabeza.url) && cabeza.url.includes('/api/tts-stream/')) {
      // LA PRIMERA FRASE suena EN STREAMING (arranca con el primer byte, ~1 s
      // antes que esperar el mp3 entero). Sus marcas exactas no sirven: el
      // navegador baja el audio al ritmo en que suena y el server las tiene
      // recien al final. El texto va al ritmo medido de la voz (LETRAS_POR_SEG).
      if (limiteVozRef.current !== Infinity) clipVozRef.current = { base, largo, marcas: null };
      arrancarTypewriter();
      jugarAudio(cabeza.url, () => {
        terminoClip();
        reproduciendoAudioRef.current = false;
        ordenAudioRef.current = miOrden + 1;
        avanzarColaAudio();
      }, hayOtro);
      return;
    }
    // una descarga colgada tambien trababa la cola para siempre: 15 s y se saltea
    const conTope = Promise.race([
      obtenerBlobAudio(cabeza.url),
      new Promise<never>((_, rechazar) =>
        window.setTimeout(() => rechazar(new Error('audio: descarga trabada')), 15000)
      ),
    ]);
    conTope
      .then(({ blob, marcas }) => {
        blobsAudioRef.current.delete(cabeza.url);
        const objUrl = URL.createObjectURL(blob);
        if (limiteVozRef.current !== Infinity) clipVozRef.current = { base, largo, marcas };
        arrancarTypewriter();
        jugarAudio(objUrl, () => {
          URL.revokeObjectURL(objUrl);
          terminoClip();
          reproduciendoAudioRef.current = false;
          ordenAudioRef.current = miOrden + 1;
          avanzarColaAudio();
        }, hayOtro);
      })
      .catch(() => {
        // chunk fallido: se salta (su texto se muestra igual) y sigue
        terminoClip();
        reproduciendoAudioRef.current = false;
        ordenAudioRef.current = miOrden + 1;
        avanzarColaAudio();
      });
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [jugarAudio, obtenerBlobAudio, liberarVoz]);

  // Replay manual: reutiliza la cola y el MISMO elemento de audio. No se permite
  // mientras Kira todavía está respondiendo o hablando para no cortar frases.
  const reproducirMensaje = useCallback(
    (urls: string[]) => {
      const lista = urls.filter(Boolean);
      const audioOcupado = audioElRef.current && !audioElRef.current.paused;
      if (
        !lista.length ||
        escribiendo ||
        streamPendienteRef.current ||
        hablando ||
        audioOcupado ||
        reproduciendoAudioRef.current
      ) {
        return;
      }
      colaAudioRef.current = lista.map((url, orden) => ({ url, orden }));
      ordenAudioRef.current = 0;
      reproduciendoAudioRef.current = false;
      avanzarColaAudio();
    },
    [avanzarColaAudio, escribiendo, hablando]
  );

  // detiene el typewriter (cortar, cambiar de chat o error)
  const detenerTypewriter = useCallback(() => {
    if (streamTimerRef.current !== null) {
      window.clearInterval(streamTimerRef.current);
      streamTimerRef.current = null;
    }
    streamFinRef.current = null;
  }, []);

  // arranca el revelado progresivo del texto (si ya corre uno, no hace nada)
  const arrancarTypewriter = useCallback(() => {
    if (streamTimerRef.current !== null) return;
    streamTimerRef.current = window.setInterval(() => {
      const objetivo = streamObjetivoRef.current;
      const revelado = streamReveladoRef.current;
      const idParcial = streamIdRef.current;
      if (!idParcial) {
        detenerTypewriter();
        return;
      }
      const tope = Math.min(objetivo.length, topeVoz());
      if (revelado >= tope && revelado < objetivo.length) {
        kiraVivo.boca *= 0.6; // pausa de la voz: la boca se cierra
        return; // esperando a la voz
      }
      if (revelado >= objetivo.length) {
        // alcanzó el texto actual: pausamos el timer (un próximo delta lo retoma)
        window.clearInterval(streamTimerRef.current!);
        streamTimerRef.current = null;
        const fin = streamFinRef.current;
        if (fin) {
          streamFinRef.current = null;
          fin();
        }
        return;
      }
      // velocidad adaptativa: un mensaje largo revela más rápido (máx ~4s)
      const vel = Math.max(1, Math.ceil(objetivo.length / 150));
      const siguiente = Math.min(revelado + vel, tope);
      streamReveladoRef.current = siguiente;
      if (clipVozRef.current) kiraVivo.boca = bocaDeLetra(objetivo[siguiente - 1]);
      const texto = objetivo.slice(0, siguiente);
      setMensajes((m) =>
        m.map((msg) => (msg.id === idParcial ? { ...msg, contenido: texto } : msg))
      );
    }, 30);
  }, [detenerTypewriter]);

  // si cambiamos de chat o volvemos al inicio, cortamos el typewriter viejo
  useEffect(() => {
    detenerTypewriter();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [chatActivo]);

  // al desmontar, nunca dejar un interval colgado
  useEffect(() => {
    return () => {
      if (streamTimerRef.current !== null) window.clearInterval(streamTimerRef.current);
    };
  }, []);

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

  // ---------- STOP: corta la respuesta en curso ----------
  // Aborta el stream HTTP, congela el texto parcial (se conserva lo escrito),
  // corta el loading y calla la boca del micro:bit (queda quieta en su cara,
  // sin transicion fantasma: la emocion ya quedo puesta al llegar el fin)
  const detenerRespuesta = useCallback(() => {
    const idEnCurso = streamIdRef.current;
    respuestaSesionRef.current += 1;
    abortRef.current?.abort();
    abortRef.current = null;
    streamIdRef.current = null;
    setStreamId(null);
    setEscribiendo(false);
    streamPendienteRef.current = false;
    setStreamPendiente(false);
    setHablando(false);
    detenerTypewriter();

    // Si se corta antes del primer carácter, no dejamos un mensaje vacío fantasma.
    if (idEnCurso) {
      setMensajes((actuales) => {
        const parcial = actuales.find((m) => m.id === idEnCurso);
        return parcial && !parcial.contenido.trim()
          ? actuales.filter((m) => m.id !== idEnCurso)
          : actuales;
      });
    }
    // la cola de voz muere con el stream: nada de frases huerfanas sonando
    liberarVoz();
    colaAudioRef.current = [];
    vistosAudioRef.current.clear();
    ordenAudioRef.current = 0;
    reproduciendoAudioRef.current = false;
    // El audio se corta a la mitad: matar el CALLA programado y callar ya.
    if (callaTimerRef.current) {
      clearTimeout(callaTimerRef.current);
      callaTimerRef.current = null;
    }
    turnoAudioRef.current += 1; // el vigia del audio cortado no debe seguir la cola
    if (audioElRef.current && !audioElRef.current.paused) audioElRef.current.pause();
    postSilencioso('/api/calla');
  }, [detenerTypewriter, liberarVoz]);

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

  const abrirChat = (pj: PersonajeInfo, sesionId: string) => {
    sonidoUI('cambio'); // micro-tono al cambiar de conversación (opcional)
    cortarEscucha();
    detenerRespuesta();

    const lista = chats[pj.id] ?? [];
    const sesion = lista.find((c) => c.id === sesionId) ?? lista[lista.length - 1];
    if (!sesion) {
      nuevoChat(pj);
      return;
    }

    setElegido(pj);
    setChatActivo(sesion.id);
    cerrarSidebar();
    setMensajes(sesion.mensajes);

    const ultimo = [...sesion.mensajes].reverse().find((m) => m.rol === pj.id);
    setEmotion(ultimo?.emotion ?? 'happy');
    localStorage.setItem(claveActual(usuario.id, pj.id), sesion.id);
  };

  const nuevoChat = (pj: PersonajeInfo) => {
    sonidoUI('cambio');
    cortarEscucha();
    detenerRespuesta();
    setMemoriaAbierta(false);
    setElegido(pj);
    cerrarSidebar();
    // Solo cuentan los títulos automáticos "Chat N"; un título como "RTX 5090"
    // no debe convertir el siguiente chat en "Chat 5091".
    const numeros = (chats[pj.id] ?? [])
      .map((c) => c.titulo.match(/^Chat\s+(\d+)$/i)?.[1])
      .filter((n): n is string => Boolean(n))
      .map(Number);
    const sesion: ChatSesion = {
      id: uuid(),
      titulo: `Chat ${(numeros.length ? Math.max(...numeros) : 0) + 1}`,
      fecha: new Date().toISOString(),
      // sin saludo guardado: el chat nuevo muestra la cara viva y un
      // saludo de pantalla (antes era un texto fijo de asistente)
      mensajes: [],
    };
    setChats((c) => ({ ...c, [pj.id]: [...(c[pj.id] ?? []), sesion] }));
    setChatActivo(sesion.id);
    setMensajes(sesion.mensajes);
    setEmotion('happy');
    localStorage.setItem(claveActual(usuario.id, pj.id), sesion.id);
  };

  // Inicio single-character: abre una charla nueva y, si vino de una
  // sugerencia, deja el prompt listo en el composer.
  const comenzarDesdeInicio = (prompt?: string) => {
    const kira = personajes[0];
    if (!kira) return;
    if (prompt) setInput(prompt);
    nuevoChat(kira);
  };

  // genera el titulo del chat en SEGUNDO PLANO, UNA vez por sesion:
  // - solo si el chat todavia tiene el titulo por defecto ('Chat N')
  // - con un Set de sesiones en curso (no comparte estado entre chats)
  const generarTituloChat = useCallback(
    (sesionId: string, pjId: string, primerMensaje: string, respuesta: string) => {
      if (titulosEnCursoRef.current.has(sesionId)) return;
      titulosEnCursoRef.current.add(sesionId);
      generarTitulo(primerMensaje, respuesta)
        .then((titulo) => {
          setChats((c) => ({
            ...c,
            [pjId]: (c[pjId] ?? []).map((s) =>
              s.id === sesionId && s.titulo.startsWith('Chat ') ? { ...s, titulo } : s
            ),
          }));
        })
        .catch(() => {
          // Si falla, el título por defecto se conserva; no afecta el chat.
        })
        .finally(() => {
          titulosEnCursoRef.current.delete(sesionId);
        });
    },
    []
  );

  // ---------- VOZ: dos rutas, una sola sesión activa ----------
  // a) con serial: escucharVoz() manual; A envía y B cancela en el micro:bit
  // b) sin serial: MediaRecorder del teléfono + transcripción en backend
  const grabadorCel = useRef<MediaRecorder | null>(null);
  const streamCel = useRef<MediaStream | null>(null);
  const chunksCel = useRef<Blob[]>([]);
  // NIVEL EN VIVO de la ruta del celular: sin esto el orbe/barras nunca
  // reaccionaban en el teléfono (solo la ruta serial medía el audio).
  const audioCtxCel = useRef<AudioContext | null>(null);
  const rafNivelCel = useRef<number | null>(null);
  // qué ruta arrancó la grabación actual (solo el celular tiene límite 60s)
  const rutaCelRef = useRef(false);

  const apagarAnalizador = () => {
    if (rafNivelCel.current !== null) {
      cancelAnimationFrame(rafNivelCel.current);
      rafNivelCel.current = null;
    }
    const ctx = audioCtxCel.current;
    audioCtxCel.current = null;
    if (ctx && ctx.state !== 'closed') ctx.close().catch(() => {});
  };

  // MIDE EL MICRÓFONO del celular: promedio de la banda de la voz (~94–2900Hz)
  // y lo volca a nivelEscucha (0..90, misma escala que la ruta serial) para
  // que las barras de la píldora reaccionen a tu voz REAL.
  const prenderAnalizador = (stream: MediaStream, sesion: number) => {
    try {
      const AC =
        window.AudioContext ??
        (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
      if (!AC) return;
      const ctx = new AC();
      audioCtxCel.current = ctx;
      const fuente = ctx.createMediaStreamSource(stream);
      const analizador = ctx.createAnalyser();
      analizador.fftSize = 512; // bin ≈ 94Hz: los bins 1..31 cubren la voz
      fuente.connect(analizador);
      const datos = new Uint8Array(analizador.frequencyBinCount);
      let ultimoTs = 0;
      let suave = 0;
      const tick = (ts: number) => {
        // la sesión cambió (cancelar/navegó): nadie escucha más
        if (sesion !== escuchaSesionRef.current || audioCtxCel.current !== ctx) return;
        if (ts - ultimoTs > 60) {
          ultimoTs = ts;
          analizador.getByteFrequencyData(datos);
          let suma = 0;
          for (let i = 1; i < 31; i++) suma += datos[i];
          suave = suave * 0.55 + (suma / 30) * 0.45; // anti-tiritón
          setNivelEscucha(Math.min(90, suave * 0.75));
        }
        rafNivelCel.current = requestAnimationFrame(tick);
      };
      rafNivelCel.current = requestAnimationFrame(tick);
    } catch {
      /* sin Web Audio: las barras quedan con su ola base, nada roto */
    }
  };

  const cortarEscucha = useCallback(() => {
    // Invalida cualquier callback pendiente de una escucha anterior.
    escuchaSesionRef.current += 1;
    escuchandoRef.current = false;
    setEscuchando(false);
    setNivelEscucha(0);
    apagarAnalizador();

    if (limiteGrabacionRef.current) {
      clearTimeout(limiteGrabacionRef.current);
      limiteGrabacionRef.current = null;
    }

    const rec = grabadorCel.current;
    if (rec && rec.state === 'recording') {
      try {
        rec.stop();
      } catch {
        // El recorder pudo cerrarse entre el chequeo y stop().
      }
    }

    streamCel.current?.getTracks().forEach((track) => track.stop());
    streamCel.current = null;
    grabadorCel.current = null;
  }, []);

  const grabarVozCel = async () => {
    const sesionEscucha = ++escuchaSesionRef.current;

    try {
      const stream = await navigator.mediaDevices.getUserMedia({ audio: true });

      // Si el usuario cambió de chat mientras aparecía el permiso del micrófono,
      // no dejamos una captura huérfana viva.
      if (sesionEscucha !== escuchaSesionRef.current) {
        stream.getTracks().forEach((track) => track.stop());
        return;
      }

      streamCel.current = stream;
      chunksCel.current = [];
      prenderAnalizador(stream, sesionEscucha);

      const rec = new MediaRecorder(stream);
      grabadorCel.current = rec;

      rec.ondataavailable = (ev) => {
        if (ev.data.size > 0) chunksCel.current.push(ev.data);
      };

      rec.onstop = async () => {
        apagarAnalizador();
        if (limiteGrabacionRef.current) {
          clearTimeout(limiteGrabacionRef.current);
          limiteGrabacionRef.current = null;
        }

        stream.getTracks().forEach((track) => track.stop());
        streamCel.current = null;
        if (grabadorCel.current === rec) grabadorCel.current = null;

        // Si esta escucha fue cancelada por navegación, ignoramos TODO lo que llegue.
        if (sesionEscucha !== escuchaSesionRef.current) return;

        setEscuchando(false);
        escuchandoRef.current = false;
        postSilencioso('/api/loading');

        const blob = new Blob(chunksCel.current, { type: rec.mimeType || 'audio/webm' });
        if (blob.size < 800) {
          mostrarErrorTemporal('No se grabó nada — probá de nuevo');
          return;
        }

        try {
          const texto = ((await transcribirDesdeCel(blob)) ?? '').trim();
          if (sesionEscucha !== escuchaSesionRef.current) return;
          if (!texto) {
            mostrarErrorTemporal('No te escuché — hablá más fuerte y cerca');
            return;
          }
          enviar(texto);
        } catch (e) {
          if (sesionEscucha !== escuchaSesionRef.current) return;
          mostrarErrorTemporal(e instanceof Error ? e.message : String(e), 5000);
        }
      };

      rec.start();
      rutaCelRef.current = true;
      setEscuchando(true);
      escuchandoRef.current = true;
      postSilencioso('/api/voz');

      limiteGrabacionRef.current = setTimeout(() => {
        if (grabadorCel.current === rec && rec.state === 'recording') rec.stop();
      }, LIMITE_GRABACION_MS);
    } catch (e) {
      if (sesionEscucha !== escuchaSesionRef.current) return;
      setEscuchando(false);
      escuchandoRef.current = false;
      mostrarErrorTemporal(
        'El micrófono no se pudo abrir: ' + (e instanceof Error ? e.message : String(e)),
        5000
      );
    }
  };

  const escucharHabla = async () => {
    if (!elegido || escribiendo || streamPendienteRef.current) return;

    // Sin serial, un segundo toque corta MediaRecorder y dispara onstop.
    if (!mbConectadoRef.current) {
      if (escuchando) {
        if (grabadorCel.current?.state === 'recording') grabadorCel.current.stop();
        return;
      }
      setError('');
      await grabarVozCel();
      return;
    }

    if (escuchando) return;

    const sesionEscucha = ++escuchaSesionRef.current;
    setError('');
    rutaCelRef.current = false;
    setEscuchando(true);
    escuchandoRef.current = true;
    setNivelEscucha(0);
    postSilencioso('/api/loading');

    await escucharVoz(
      (nivel) => {
        if (sesionEscucha === escuchaSesionRef.current && escuchandoRef.current) {
          setNivelEscucha(nivel);
        }
      },
      (res) => {
        if (sesionEscucha !== escuchaSesionRef.current) return;
        setEscuchando(false);
        escuchandoRef.current = false;

        const texto = (res.transcripcion ?? '').trim();
        if (!texto) {
          mostrarErrorTemporal('No te escuché nada — hablá más cerca del micro:bit');
          return;
        }
        enviar(texto);
      },
      (e) => {
        if (sesionEscucha !== escuchaSesionRef.current) return;
        setEscuchando(false);
        escuchandoRef.current = false;
        mostrarErrorTemporal(String(e.message || e));
      },
      () => {
        if (sesionEscucha !== escuchaSesionRef.current) return;
        setEscuchando(false);
        escuchandoRef.current = false;
        setNivelEscucha(0);
      }
    );
  };

  // ✕ CANCELAR: tira la grabación sin mandar nada (sirve en AMBAS rutas;
  // en el micro:bit también podés cancelar con B).
  const cancelarEscucha = () => {
    cortarEscucha(); // invalida la sesión: el onstop NO transcribe ni envía
    postSilencioso('/api/cancelar'); // descarta el audio; no intenta transcribirlo
  };

  const cambiarSonido = (v: boolean) => {
    setSonido(v);
    setSonidoActivo(v);
  };

  const cambiarVoz = (v: boolean) => {
    setAjustesLocales((actual) => {
      const siguiente = { ...actual, voz: v };
      guardarAjustes(usuario.id, siguiente);
      return siguiente;
    });
  };

  const cambiarAutonomia = (v: boolean) => {
    setAjustesLocales((actual) => {
      const siguiente = { ...actual, autonomia: v };
      guardarAjustes(usuario.id, siguiente);
      return siguiente;
    });
  };

  const cambiarFondoVivo = (v: boolean) => {
    setAjustesLocales((actual) => {
      const siguiente = { ...actual, fondoVivo: v };
      guardarAjustes(usuario.id, siguiente);
      return siguiente;
    });
  };

  const cambiarVolumenVoz = (v: number) => {
    const volumen = Math.max(0, Math.min(1, v));
    setAjustesLocales((actual) => {
      const siguiente = { ...actual, volumenVoz: volumen };
      guardarAjustes(usuario.id, siguiente);
      return siguiente;
    });
  };

  const cambiarPreferenciasMemoria = (memoria: PreferenciasMemoria) => {
    setAjustesLocales((actual) => {
      const siguiente = { ...actual, memoria: { ...memoria } };
      guardarAjustes(usuario.id, siguiente);
      return siguiente;
    });
  };

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

  // borra TODAS las conversaciones (desde Ajustes, con doble confirmacion)
  const borrarTodosLosChats = () => {
    cortarEscucha();
    detenerRespuesta();
    setChats({});
    setElegido(null);
    setChatActivo(null);
    setMensajes([]);
    setError('');
    setMemoriaAbierta(false);
    setAjustesAbiertos(false);
    personajes.forEach((pj) => localStorage.removeItem(claveActual(usuario.id, pj.id)));
    postSilencioso('/api/stop');
  };

  const borrarChat = (pjId: string, sesionId: string) => {
    const esActivo = chatActivo === sesionId && elegido?.id === pjId;
    if (esActivo) {
      cortarEscucha();
      detenerRespuesta();
    }

    const encontrado = personajes.find((p) => p.id === pjId) ?? elegido;
    if (!encontrado) return;
    const pj: PersonajeInfo = encontrado;
    const lista = (chats[pjId] ?? []).filter((c) => c.id !== sesionId);
    setChats((c) => ({
      ...c,
      [pjId]: (c[pjId] ?? []).filter((c) => c.id !== sesionId),
    }));
    if (chatActivo === sesionId && elegido?.id === pjId) {
      if (lista.length > 0) {
        abrirChat(pj, lista[lista.length - 1].id);
      } else {
        // si no quedan chats de este personaje, abrir el último de otro (o la bienvenida)
        const otro = personajes.find((p) => p.id !== pjId);
        const otraLista = otro ? (chats[otro.id] ?? []) : [];
        if (otro && otraLista.length > 0) {
          abrirChat(otro, otraLista[otraLista.length - 1].id);
        } else {
          setElegido(null);
          setChatActivo(null);
          setMensajes([]);
        }
      }
    }
    postSilencioso('/api/stop');
  };

  const volver = () => {
    cortarEscucha();
    detenerRespuesta();
    // INICIO = cerrar esta conversación: se olvida cuál era el chat "activo"
    // del personaje, así al volver a entrar arranca uno NUEVO en vez de
    // retomar el que estabas mirando (ni el último de la lista).
    if (elegido) localStorage.removeItem(claveActual(usuario.id, elegido.id));
    setElegido(null);
    setChatActivo(null);
    setMensajes([]);
    setError('');
    setHablando(false);
    setMemoriaAbierta(false);
    postSilencioso('/api/stop');
  };

  // ---------- 📷 CÁMARA: sacar la foto y reducirla ANTES de mandarla ----------
  // El mismo input con `capture` abre la cámara trasera en el celular (y el
  // selector de archivos en la compu). La reducción vive en foto.ts.
  const elegirFoto = useCallback(
    async (e: ChangeEvent<HTMLInputElement>) => {
      const archivo = e.target.files?.[0];
      // limpiar el input: sin esto, volver a elegir LA MISMA foto no dispara
      e.target.value = '';
      if (!archivo) return;
      setProcesandoFoto(true);
      try {
        setFoto(await fotoDesdeArchivo(archivo));
      } catch {
        mostrarErrorTemporal('No se pudo leer la foto, probá otra vez');
      } finally {
        setProcesandoFoto(false);
      }  }, [mostrarErrorTemporal]);

  // CRONÓMETRO de la grabación: corre mientras el mic está abierto y se
  // reinicia solo al cerrar (en la ruta del celular marca el acercamiento
  // al límite de 60s con la barrita de progreso de la píldora).
  useEffect(() => {
    if (!escuchando) {
      setSegEscucha(0);
      return;
    }
    const t0 = Date.now();
    setSegEscucha(0);
    const id = window.setInterval(() => {
      setSegEscucha(Math.floor((Date.now() - t0) / 1000));
    }, 250);
    return () => window.clearInterval(id);
  }, [escuchando]);

  // ------------------------------------------------------------------------
  // AUTONOMIA: Kira habla SOLA cuando le pasa algo en la placa (la tocaron,
  // la marearon, terminó un juego...) o cuando hace rato que nadie le habla
  // con la pantalla abierta ("oye... sé que andás por ahí").
  // Frenos: nunca mientras alguien escribe, graba o ella habla; como mucho
  // una vez cada AUTONOMIA_PAUSA_MS; y se apaga en Ajustes.
  // ------------------------------------------------------------------------
  const ultimaActividadRef = useRef(Date.now());
  const ultimoProactivoRef = useRef(0);
  const seqEventosRef = useRef(-1);
  const ocultaDesdeRef = useRef<number | null>(null);
  const enviarRef = useRef<((t?: string, r?: string, e?: string) => Promise<void>) | null>(null);

  // evento: turno de AUTONOMIA (nadie escribio: la placa avisó algo o hubo
  // un silencio largo). No hay burbuja del usuario ni loading en la placa.
  const enviar = async (textoDesdeVoz?: string, reemplazarId?: string, evento?: string) => {
    if (!elegido || escribiendo || streamPendienteRef.current) return;
    if (!evento) ultimaActividadRef.current = Date.now();
    const sesionRespuesta = ++respuestaSesionRef.current;
    const esRegeneracion = !!reemplazarId;
    // REGENERAR ⟳: reenvía el mensaje del usuario anterior a reemplazarId
    // y reescribe la respuesta EN EL MISMO mensaje (sin burbujas nuevas)
    let base = mensajes;
    if (esRegeneracion) {
      const idx = mensajes.findIndex((m) => m.id === reemplazarId);
      const idxUser = idx > 0 ? [...mensajes.slice(0, idx)].map((m) => m.rol).lastIndexOf('usuario') : -1;
      if (idxUser < 0) return;
      base = mensajes.slice(0, idxUser + 1);
    }
    const texto = evento
      ? ''
      : esRegeneracion
        ? (base[base.length - 1]?.contenido ?? '').trim()
        : (textoDesdeVoz ?? input).trim();
    // FOTO: la pendiente del composer; al REGENERAR se reenvía la del mensaje
    // original (así la IA vuelve a MIRARLA en vez de responder a ciegas)
    const fotoEnviada = evento
      ? null
      : esRegeneracion
        ? (base[base.length - 1]?.imagen ?? null)
        : foto;
    // un turno vale con texto, con foto, o con las dos cosas juntas
    if (!evento && !texto && !fotoEnviada) return;
    ultimoTextoRef.current = texto; // para el botón "reintentar" de errores
    if (!esRegeneracion && !evento) {
      sonidoEnviar(); // pop sutil (solo si el usuario activo el sonido)
      setInput('');
      setFoto(null); // la foto ya viajó: la miniatura se limpia
    }
    setError('');
    setAutoScroll(true); // al enviar, siempre seguimos la conversación
    setHablando(false);
    if (textareaRef.current) textareaRef.current.style.height = 'auto';
    // reseteamos el typewriter para este nuevo mensaje
    detenerTypewriter();
    streamObjetivoRef.current = '';
    streamReveladoRef.current = 0;

    // el stream se puede cortar con el botón stop (AbortController)
    const controller = new AbortController();
    abortRef.current = controller;

    // ¿es el PRIMER intercambio de este chat? (no hay mensajes del usuario aun;
    // en regeneración no: no se toca el título ni se agregan burbujas)
    const esPrimerIntercambio = !esRegeneracion && !evento && !mensajes.some((m) => m.rol === 'usuario');
    // si el primer mensaje fue SOLO una foto, el título igual sale (con su emoji)
    const primerMensajeChat = esPrimerIntercambio ? (texto || '📷 (foto)') : '';

    // mensaje parcial del personaje: se llena SOLO con el typewriter (streaming).
    // En regeneración se reutiliza el id existente (se reescribe en su lugar).
    const idParcial = reemplazarId ?? uuid();
    streamIdRef.current = idParcial;
    setStreamId(idParcial);
    if (esRegeneracion) {
      // limpiar el mensaje viejo para revelar la nueva respuesta desde cero
      setMensajes((m) =>
        m.map((msg) => (msg.id === reemplazarId ? { ...msg, contenido: '' } : msg))
      );
    } else if (evento) {
      // Kira habla sola: solo su burbuja
      setMensajes((m) => [
        ...m,
        { id: idParcial, rol: elegido.id as 'kira', contenido: '', hora: horaActual() },
      ]);
    } else {
      setMensajes((m) => [
        ...m,
        { id: uuid(), rol: 'usuario', contenido: texto, imagen: fotoEnviada, hora: horaActual() },
        { id: idParcial, rol: elegido.id as 'kira', contenido: '', hora: horaActual() },
      ]);
    }
    setEscribiendo(true);
    streamPendienteRef.current = true;
    setStreamPendiente(true);
    setHerramienta(null); // tool trace del turno anterior, si hubo
    // COLA DE VOZ FRESCA: cada respuesta tiene su propia cadena de frases
    colaAudioRef.current = [];
    vistosAudioRef.current.clear();
    ordenAudioRef.current = 0;
    reproduciendoAudioRef.current = false;
    // con voz, el texto sale al ritmo de lo que va diciendo
    limiteVozRef.current = vozActivadaRef.current ? 0 : Infinity;
    clipVozRef.current = null;
    finVozRef.current = false;
    // LA PLACA FISICA TAMBIEN PIENSA: LOADING random en el micro:bit
    // (via serial o por el puente BLE en modo feria) mientras la IA
    // contesta. Sin esto la cara queda congelada en la emocion vieja.
    // (En un turno por evento no: la placa esta en plena reaccion.)
    if (!evento) postSilencioso('/api/loading');

    const historial = base.map((m) => ({
      rol: m.rol === 'usuario' ? 'user' : 'assistant',
      // las fotos VIEJAS viajan como texto: la imagen real solo va en el turno
      // nuevo (si no, cada turno reenviaría megabytes de base64 al vacío)
      contenido: m.contenido.trim() || (m.imagen ? '(te mandé una foto)' : ''),
      // el rastro vuelve hacia atras a proposito: es lo que le permite a Kira
      // citar sus propias fuentes si le repreguntan. Antes no volvia, por eso
      // no tenia memoria de sus busquedas (ver app/services/rastro.py).
      rastro: m.rol === 'kira' ? (m.rastro ?? null) : null,
      // la emocion de cada respuesta de Kira: el backend le devuelve a la IA
      // sus turnos en el mismo JSON que ella produce (ver contenido_para_ia)
      emotion: m.rol === 'kira' ? (m.emotion ?? null) : null,
    }));

    await enviarMensajeStream(
      elegido.id,
      texto,
      historial,
      // onDelta: el texto va llegando; el typewriter lo revela de a poco
      (textoParcial) => {
        if (sesionRespuesta !== respuestaSesionRef.current) return;
        streamObjetivoRef.current = textoParcial;
        arrancarTypewriter();
      },
      // y la voz arranca en paralelo (jugarAudio corta el loading en onplaying)
      (resp) => {
        if (sesionRespuesta !== respuestaSesionRef.current) return;
        setHerramienta(null); // ya hay respuesta: el trace queda viejo
        sonidoRecibir(); // notita suave al llegar la respuesta (si esta activo)
        streamObjetivoRef.current = resp.message;
        // la emocion se actualiza YA (sincrona): si el TTS es corto y termina
        // antes de que el typewriter acabe, la cara no debe volver a la vieja
        setEmotion(resp.emotion);
        // LA CARA YA CAMBIA a la emocion del mensaje AHORA, ANTES de hablar:
        // asi cuando llega el TALK (al arrancar el audio) el micro:bit usa la
        // boca de la emocion NUEVA (ej. triste habla triste), no la anterior.
        postSilencioso('/api/emocion', {
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ emotion: resp.emotion }),
        });
        const audioUrls = resp.tts_urls?.length
          ? resp.tts_urls
          : resp.tts_url
            ? [resp.tts_url]
            : null;
        finVozRef.current = true;
        // sin cola de frases (sin voz, TTS caido, respuesta de respaldo) o con
        // todo ya dicho: el texto no espera a nadie
        const quedaVoz =
          colaAudioRef.current.length > 0 ||
          reproduciendoAudioRef.current ||
          (resp.tts_urls ?? []).some((u) => !vistosAudioRef.current.has(u));
        if (!vozActivadaRef.current || !resp.tts_urls?.length || !quedaVoz) liberarVoz();
        const completar = () => {
          if (sesionRespuesta !== respuestaSesionRef.current) return;
          streamPendienteRef.current = false;
          setStreamPendiente(false);
          streamIdRef.current = null;
          abortRef.current = null;
          setStreamId(null);
          setMensajes((m) =>
            m.map((msg) =>
              msg.id === idParcial
                ? {
                    ...msg,
                    contenido: resp.message,
                    emotion: resp.emotion,
                    audio_url: resp.tts_url,
                    audio_urls: audioUrls,
                    fuentes: resp.fuentes ?? null,
                    rastro: resp.rastro ?? null,
                  }
                : msg
            )
          );
          if (chatActivo && primerMensajeChat) {
            generarTituloChat(chatActivo, elegido.id, primerMensajeChat, resp.message);
          }
        };
        const sinVoz = () => {
          // la emocion ya se mando arriba (la cara ya esta en la emocion del
          // mensaje): sin voz solo cortamos el estado de escribiendo
          setEscribiendo(false);
        };
        if (vozActivadaRef.current && resp.tts_urls?.length) {
          // TTS POR FRASES: la lista autoritativa (en orden). Los eventos
          // 'audio' ya adelantaron algunas: se fusiona sin duplicar.
          for (let i = 0; i < resp.tts_urls.length; i++) {
            const url = resp.tts_urls[i];
            if (!vistosAudioRef.current.has(url)) {
              vistosAudioRef.current.add(url);
              colaAudioRef.current.push({ url, orden: i });
            }
          }
          colaAudioRef.current.sort((a, b) => a.orden - b.orden);
          avanzarColaAudio();
        } else if (vozActivadaRef.current && resp.tts_url) {
          // compat: una sola URL (respuestas cortas de una frase)
          jugarAudio(resp.tts_url);
        } else {
          streamFinRef.current = () => {
            completar();
            sinVoz();
          };
        }
        if (streamReveladoRef.current >= resp.message.length) {
          // el typewriter ya mostró todo: completamos directo
          completar();
          if (!vozActivadaRef.current || !resp.tts_url) sinVoz();
          streamFinRef.current = null;
        } else {
          // falta revelar: al terminar se ejecuta completar (y sinVoz si no hay voz)
          streamFinRef.current = streamFinRef.current ?? completar;
          arrancarTypewriter();
        }
      },
      // onError: el stream se corto o la IA fallo a mitad
      (e) => {
        if (sesionRespuesta !== respuestaSesionRef.current) return;
        setHerramienta(null);
        streamIdRef.current = null;
        abortRef.current = null;
        detenerTypewriter();
        setStreamId(null);
        setEscribiendo(false);
        streamPendienteRef.current = false;
        setStreamPendiente(false);
        // si fue el USUARIO quien cortó (botón stop / Escape), no es un
        // error: se conserva el texto parcial y listo. Si Kira hablaba SOLA
        // (evento) tampoco: nadie le pidio nada, no hay que asustar con un
        // "el cerebro fallo"
        if (e.name === 'AbortError' || evento) {
          if (evento && e.name !== 'AbortError') console.warn('turno autonomo fallido', evento, e);
          setMensajes((m) => {
            const parcial = m.find((msg) => msg.id === idParcial);
            if (parcial && !parcial.contenido.trim()) {
              return m.filter((msg) => msg.id !== idParcial);
            }
            return m;
          });
          return;
        }
        setError(String(e instanceof Error ? e.message : e));
        // si el mensaje parcial quedo VACIO (fallo antes de escribir nada),
        // se borra para no dejar una burbuja fantasma; si tiene texto parcial
        // se conserva (mejor perder algo que nada)
        setMensajes((m) => {
          const parcial = m.find((msg) => msg.id === idParcial);
          // si quedo vacio se borra (burbuja fantasma); con texto parcial se queda
          if (parcial && !parcial.contenido.trim()) {
            return m.filter((msg) => msg.id !== idParcial);
          }
          return m;
        });
      },
      controller.signal,
      // onAudio: una frase ya tiene su audio listo — a la cola, en orden
      (url, orden, textoFrase) => {
        if (sesionRespuesta !== respuestaSesionRef.current) return;
        if (!vozActivadaRef.current) return;
        if (vistosAudioRef.current.has(url)) return;
        vistosAudioRef.current.add(url);
        colaAudioRef.current.push({ url, orden, texto: textoFrase });
        colaAudioRef.current.sort((a, b) => a.orden - b.orden);
        avanzarColaAudio();
      },
      chatActivo,
      // LA FOTO: el último argumento (null si no hay) -> el backend la mete en
      // el mensaje del usuario como bloque image_url y la IA la ve
      fotoEnviada,
      // tool trace: el backend avisa qué herramienta está usando
      setHerramienta,
      // privacidad: los interruptores de Ajustes viajan con este turno
      ajustesLocales.memoria,
      evento ?? null
    );
  };
  enviarRef.current = enviar;

  useEffect(() => {
    if (!ajustesLocales.autonomia) return;
    const PAUSA_MS = 20000;          // entre dos intervenciones espontaneas
    const SILENCIO_MS = 4 * 60000;   // pantalla abierta y nadie habla
    const VOLVIO_MS = 2 * 60000;     // estuvo afuera de la pestaña al menos esto
    let vivo = true;

    const libre = () =>
      !streamPendienteRef.current &&
      !reproduciendoAudioRef.current &&
      !escuchandoRef.current &&
      Date.now() - ultimoProactivoRef.current > PAUSA_MS &&
      Date.now() - ultimaActividadRef.current > 6000;

    const hablarSola = (evento: string) => {
      if (!libre() || !enviarRef.current) return false;
      ultimoProactivoRef.current = Date.now();
      void enviarRef.current(undefined, undefined, evento);
      return true;
    };

    // Un evento que llega mientras Kira habla o piensa NO se pierde: queda
    // esperando hasta 90 s (antes se tiraba, y el atardecer pasa una vez).
    // Los del mundo real y los juegos ganan contra un toque cualquiera.
    let pendiente: { linea: string; t: number } | null = null;
    const pesa = (l: string) => (l.startsWith('MUNDO:') || l.startsWith('JUEGO:') || l === 'EVENTO:TIMER' ? 2 : 1);

    const tick = async () => {
      if (document.hidden) return;
      try {
        const r = await obtenerEventos(seqEventosRef.current);
        if (!vivo) return;
        const primeraVez = seqEventosRef.current < 0;
        seqEventosRef.current = r.seq;
        // El ultimo evento que valga la pena (los de antes de abrir la web no)
        const ultimo = primeraVez ? undefined : r.eventos[r.eventos.length - 1];
        if (ultimo && (!pendiente || pesa(ultimo.linea) >= pesa(pendiente.linea))) {
          pendiente = { linea: ultimo.linea, t: Date.now() };
        }
        if (pendiente && Date.now() - pendiente.t > 90000) pendiente = null;
        if (pendiente && hablarSola(pendiente.linea)) {
          pendiente = null;
          return;
        }
      } catch {
        // sin eventos (backend viejo, sin placa): la autonomia por silencio sigue
      }
      const quieto = Date.now() - Math.max(ultimaActividadRef.current, ultimoProactivoRef.current);
      if (quieto > SILENCIO_MS) hablarSola('SILENCIO');
    };

    const alCambiarVisibilidad = () => {
      if (document.hidden) {
        ocultaDesdeRef.current = Date.now();
        return;
      }
      const desde = ocultaDesdeRef.current;
      ocultaDesdeRef.current = null;
      if (desde && Date.now() - desde > VOLVIO_MS) hablarSola('VOLVIO');
    };

    const id = window.setInterval(() => void tick(), 1500);
    document.addEventListener('visibilitychange', alCambiarVisibilidad);
    return () => {
      vivo = false;
      window.clearInterval(id);
      document.removeEventListener('visibilitychange', alCambiarVisibilidad);
    };
  }, [ajustesLocales.autonomia]);

  // Limpieza final: nada de streams, micrófonos o timers vivos al desmontar.
  useEffect(() => {
    return () => {
      abortRef.current?.abort();
      if (errorTimerRef.current) clearTimeout(errorTimerRef.current);
      if (limiteGrabacionRef.current) clearTimeout(limiteGrabacionRef.current);
      if (callaTimerRef.current) clearTimeout(callaTimerRef.current);
      streamCel.current?.getTracks().forEach((track) => track.stop());
      const audio = audioElRef.current;
      if (audio && !audio.paused) audio.pause();
    };
  }, []);

  // la cara del micro:bit virtual: loading mientras piensa, talk al hablar, sino la emoción
  const cara = hablando ? 'talk' : streamPendiente ? 'loading' : emotion;

  // replica REAL: el patron que retransmite el micro:bit fisico (si llega)
  const patronReal = status?.microbit.leds ?? null;

  // estado del micro:bit para el indicador
  const mb = status?.microbit;
  // 'relay' = "placa por BLE": el enlace lo lleva el celular. Es un estado
  // BUENO (punto ámbar con latido), no una falla: antes se pintaba rojo.
  const mbEstado = estadoMicrobit(mb);
  const mbTexto = textoMicrobit(mb);
  const mbTitulo = mb?.ultimo_ack ? `Último ACK: ${mb.ultimo_ack}` : '';
  const bleOcupado = bleEstado === 'conectando' || bleEstado === 'reconectando';
  const bleEtiqueta =
    bleEstado === 'conectado'
      ? 'Desconectar micro:bit'
      : bleEstado === 'rechazado'
        ? 'Reintentar conexión BLE'
        : bleOcupado
          ? 'Conectando micro:bit…'
          : 'Conectar micro:bit';

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

  const color = elegido?.color ?? '#e8443b';  const totalChats = useMemo(
    () => Object.values(chats).reduce((total, lista) => total + lista.length,  0),
    [chats]
  );
  // el chat más reciente de cualquier personaje: el "continuar donde
  // dejaste" del hero (patrón moderno: retomar > empezar de cero)
  const ultimoChat = useMemo(() => {
    const todos = personajes.flatMap((pj) =>
      (chats[pj.id] ?? []).map((sesion) => ({ pj, sesion }))
    );
    todos.sort(
      (a, b) => new Date(b.sesion.fecha).getTime() - new Date(a.sesion.fecha).getTime()
    );
    return todos[0] ?? null;
  }, [chats, personajes]);

  const sesionActual = useMemo(
    () =>
      elegido && chatActivo
        ? (chats[elegido.id] ?? []).find((sesion) => sesion.id === chatActivo) ?? null
        : null,
    [chats, chatActivo, elegido]
  );
  const tituloConversacion = sesionActual
    ? tituloSinEmoji(sesionActual.titulo)
    : `Conversación con ${elegido?.nombre ?? 'Kira'}`;

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
      {/* ============ SIDEBAR: historial de chats a la izquierda ============ */}
      <aside
        aria-label="Conversaciones"
        aria-modal={sidebarAbierta ? true : undefined}
        role={sidebarAbierta ? 'dialog' : undefined}
        className={
          'sidebar' +
          (sidebarAbierta ? ' abierta' : '') +
          (sidebarColapsada ? ' colapsada' : '')
        }
      >
        <Sidebar
          personajes={personajes}
          chats={chats}
          elegido={elegido}
          chatActivo={chatActivo}
          mbEstado={mbEstado}
          mbTexto={mbTexto}
          mbTitulo={mbTitulo}
          colapsada={sidebarColapsada}
          onToggleColapsada={() => setSidebarColapsada((v) => !v)}
          onAbrirChat={abrirChat}
          onBorrarChat={borrarChat}
          onNuevoChat={nuevoChat}
          onSincronizar={sincronizar}
          onAbrirAjustes={() => {
            sonidoUI('abrir');
            setSidebarAbierta(false);
            setAjustesAbiertos(true);
          }}
          onVolver={() => {
            setSidebarAbierta(false);
            sidebarReturnFocusRef.current = null;
            volver();
            requestAnimationFrame(() => {
              document.querySelector<HTMLButtonElement>('.inicio-menu')?.focus();
            });
          }}
          onCerrar={cerrarSidebar}
        />
      </aside>
      {sidebarAbierta && (
        <button
          type="button"
          className="sidebar-fondo"
          aria-label="Cerrar conversaciones"
          onClick={cerrarSidebar}
        />
      )}

      {/* modal de ajustes: sonido, memoria/privacidad, chats, dispositivo y diagnóstico */}
      <Settings
        abierto={ajustesAbiertos}
        onCerrar={() => {
          sonidoUI('cerrar');
          setAjustesAbiertos(false);
        }}
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
        onCerrar={() => {
          sonidoUI('cerrar');
          setMemoriaAbierta(false);
        }}
        personajeId={elegido?.id ?? 'kira'}
        nombre={elegido?.nombre ?? 'Kira'}
        color={color}
      />

      {/* ============ PANEL PRINCIPAL ============ */}
      <main className="panel">
        {/* ---------- Bienvenida / selección ---------- */}
        {!elegido && (
          <div className="pantalla pantalla-inicio">
            <div className="pantalla-topbar">
              <button
                type="button"
                className="inicio-menu"
                onClick={abrirSidebar}
                aria-label="Abrir conversaciones"
              >
                <Menu size={18} strokeWidth={2} aria-hidden="true" />
                <span>Conversaciones</span>
              </button>
            </div>

            <header className="hero hero-kira">
              <KiraCara size={128} vivo mirarPuntero className="hero-cara" etiqueta="La cara de Kira" />
              <p className="hero-saludo">{saludoDelDia()}</p>
              <h1>Kira</h1>
              <p className="subtitulo">
                Una IA con voz y emociones que vive en una micro:bit. Hablale,
                escuchala y mirá cómo responde la carita.
              </p>

              {cargandoPjs ? (
                <div className="welcome-loading" role="status" aria-label="Preparando a Kira">
                  <span />
                  <span />
                </div>
              ) : (
                personajes[0] && (
                  <>
                    <div className="welcome-acciones">
                      {ultimoChat ? (
                        <>
                          <button
                            type="button"
                            className="welcome-primary"
                            onClick={() => abrirChat(ultimoChat.pj, ultimoChat.sesion.id)}
                          >
                            <span>Continuar conversación</span>
                            <span className="welcome-primary-meta">
                              {tituloSinEmoji(ultimoChat.sesion.titulo)} · {horaLista(ultimoChat.sesion.fecha)}
                            </span>
                            <ArrowRight size={16} strokeWidth={2} aria-hidden="true" />
                          </button>
                          <button
                            type="button"
                            className="welcome-secondary"
                            onClick={() => comenzarDesdeInicio()}
                          >
                            <Plus size={15} strokeWidth={2} aria-hidden="true" />
                            Empezar de nuevo
                          </button>
                        </>
                      ) : (
                        <button
                          type="button"
                          className="welcome-primary welcome-primary-simple"
                          onClick={() => comenzarDesdeInicio()}
                        >
                          Empezar a hablar
                          <ArrowRight size={16} strokeWidth={2} aria-hidden="true" />
                        </button>
                      )}
                    </div>

                    <div className="sugerencias welcome-suggestions" aria-label="Sugerencias para empezar">
                      {sugerencias.map((s) => (
                        <button
                          key={s}
                          type="button"
                          className="sugerencia"
                          onClick={() => comenzarDesdeInicio(s)}
                        >
                          <span>{s}</span>
                          <ArrowRight size={14} strokeWidth={2} aria-hidden="true" />
                        </button>
                      ))}
                    </div>
                  </>
                )
              )}
            </header>

            {error && <p className="error" role="status">{error}</p>}
          </div>
        )}

        {/* ---------- Chat ---------- */}
        {elegido && (
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
              onAbrirSidebar={abrirSidebar}
              onVolver={volver}
              acciones={
                <>
                  <button
                    type="button"
                    className="header-menu-item"
                    onClick={() => window.dispatchEvent(new Event('kira-ble-toggle'))}
                    disabled={bleOcupado}
                  >
                    {bleEstado === 'conectado' ? (
                      <BluetoothConnected size={16} strokeWidth={2} aria-hidden="true" />
                    ) : (
                      <Bluetooth size={16} strokeWidth={2} aria-hidden="true" />
                    )}
                    <span>{bleEtiqueta}</span>
                  </button>
                  <button
                    type="button"
                    className="header-menu-item"
                    onClick={() => setModoCara(true)}
                  >
                    <Maximize2 size={16} strokeWidth={2} aria-hidden="true" />
                    <span>Modo cara (pantalla completa)</span>
                  </button>
                  <button
                    type="button"
                    className="header-menu-item"
                    onClick={() => {
                      sonidoUI('abrir');
                      setMemoriaAbierta(true);
                    }}
                  >
                    <Brain size={16} strokeWidth={2} aria-hidden="true" />
                    <span>Recuerdos de Kira</span>
                  </button>
                  <button
                    type="button"
                    className="header-menu-item"
                    onClick={recargar}
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
              }
            />
            <section
              className="mensajes"
              ref={mensajesRef}
              onScroll={revisarScroll}
              aria-busy={respuestaActiva}
            >
              {mensajes.map((m) => {
                // El parcial nace VACÍO (no llegó ni una palabra todavía):
                // mientras tanto no se dibuja, porque ese momento lo
                // representa el indicador de puntitos. Si se dibujara, se
                // veían DOS indicadores a la vez: la burbuja con el cursor
                // "|" + su pie "escribiendo…", y abajo los puntitos.
                if (mostrarPuntos && m.id === streamId) return null;
                const audioUrls = m.audio_urls?.length
                  ? m.audio_urls
                  : m.audio_url
                    ? [m.audio_url]
                    : [];
                return (
                  <Burbuja
                    key={m.id}
                    m={m}
                    personajeId={elegido.id}
                    color={color}
                    parcial={streamId === m.id}
                    ultimo={m.id === ultimoKiraId}
                    onRegenerar={
                      m.rol !== 'usuario' && !respuestaActiva
                        ? (id) => enviar(undefined, id)
                        : undefined
                    }
                    onEscuchar={
                      m.rol !== 'usuario' && ajustesLocales.voz && audioUrls.length > 0 && !respuestaActiva && !hablando
                        ? () => reproducirMensaje(audioUrls)
                        : undefined
                    }
                  />
                );
              })}

              {/* chat vacío: la cara viva de Kira, su saludo y sugerencias
                  para romper el hielo (patrón de activación) */}
              {mensajes.length === 0 && !respuestaActiva && (
                <div className="chat-vacio">
                  <KiraCara size={104} vivo mirarPuntero etiqueta="La cara de Kira" />
                  <p className="chat-vacio-saludo">{saludoPantalla}</p>
                </div>
              )}
              {!hayMensajesUsuario && !respuestaActiva && !input.trim() && (
                <div className="sugerencias">
                  {sugerencias.map((s) => (
                    <button
                      key={s}
                      type="button"
                      className="sugerencia"
                      onClick={() => enviar(s)}
                    >
                      <span>{s}</span>
                      <ArrowRight size={14} strokeWidth={2} aria-hidden="true" />
                    </button>
                  ))}
                </div>
              )}

              <AnimatePresence>
                {mostrarPuntos && (
                  <motion.div
                    className="typing"
                    style={{ '--color': color } as CSSProperties}
                    initial={{ opacity: 0, y: 4 }}
                    animate={{ opacity: 1, y: 0 }}
                    exit={{ opacity: 0, y: -4 }}
                    transition={{ duration: T.fast, ease: EASE_OUT }}
                    role="status"
                    aria-live="polite"
                  >
                    <div className="typing-avatar">
                      <KiraCara size={32} estado="piensa" />
                    </div>
                    <span className="typing-texto">
                      <span className="typing-dots" aria-hidden="true">
                        <span />
                        <span />
                        <span />
                      </span>
                      {/* crossfade corto al cambiar de estado (pensando ->
                          "buscando en la web…") con la misma curva del resto */}
                      <motion.span
                        key={labelTrace ?? 'pensando'}
                        initial={{ opacity: 0, y: 2 }}
                        animate={{ opacity: 1, y: 0 }}
                        transition={{ duration: T.fast, ease: EASE_OUT }}
                      >
                        {labelTrace
                          ? `${elegido.nombre} está ${labelTrace}`
                          : `${elegido.nombre} está pensando…`}
                      </motion.span>
                    </span>
                  </motion.div>
                )}
              </AnimatePresence>
              {error && (
                <p className="error" role="alert">
                  {error}{' '}
                  {ultimoTextoRef.current && !respuestaActiva && (
                    <button
                      className="error-reintentar"
                      onClick={() => {
                        setError('');
                        enviar(ultimoTextoRef.current);
                      }}
                    >
                      reintentar
                    </button>
                  )}
                </p>
              )}
            </section>

            {/* botón flotante: volver abajo cuando scrolleaste para arriba */}
            <AnimatePresence>
              {!autoScroll && (
                <motion.button
                  type="button"
                  className="bajar-btn"
                  onClick={() => {
                    setAutoScroll(true);
                    bajarAlFondo(true);
                  }}
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
                subtitulo={ultimoKiraId ? (mensajes.find((m) => m.id === ultimoKiraId)?.contenido ?? '') : ''}
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
                onHablar={() => {
                  if (escuchando) {
                    if (grabadorCel.current?.state === 'recording') grabadorCel.current.stop();
                  } else {
                    void escucharHabla();
                  }
                }}
                onCosquillas={() =>
                  postSilencioso('/api/comando', {
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ cmd: 'REACCION:1' }),
                  })
                }
                onCerrar={() => setModoCara(false)}
              />
            )}

            <footer className="input-bar">
              {escuchando ? (
                // PÍLDORA DE GRABACIÓN (est. ChatGPT, patrón EXA): punto REC
                // latiente + cronómetro + barras que reaccionan a tu voz REAL
                // + ✕ cancelar / ✓ enviar. El waveform reacciona a la voz real.
                <div
                  className="orbe-escucha"
                  style={
                    {
                      '--nivel': nivelEscucha,
                      '--progreso': `${Math.min(100, (segEscucha / 60) * 100)}%`,
                    } as CSSProperties
                  }
                  role="group"
                  aria-label="Kira está escuchando"
                >
                  <button
                    type="button"
                    className="orbe-accion orbe-cancelar"
                    onClick={(e) => {
                      e.stopPropagation();
                      cancelarEscucha();
                    }}
                    title="Cancelar la grabación (Esc)"
                    aria-label="Cancelar la grabación"
                  >
                    <X size={16} strokeWidth={2.4} aria-hidden="true" />
                  </button>
                  <span className="orbe-estado" aria-hidden="true">
                    <span className="orbe-rec" />
                    <span className="orbe-tiempo">{fmtReloj(segEscucha)}</span>
                  </span>
                  <div className="orbe-barras" aria-hidden="true">
                    {FACTORES_BARRAS.map((f, i) => (
                      <i
                        key={i}
                        style={{ '--f': f, animationDelay: `${i * 0.07}s` } as CSSProperties}
                      />
                    ))}
                  </div>
                  <span className="orbe-hint">
                    {mbConectadoRef.current ? 'hablá… te escucha' : 'hablá…'}
                  </span>
                  {!mbConectadoRef.current && (
                    <button
                      type="button"
                      className="orbe-accion orbe-enviar"
                      onClick={(e) => {
                        e.stopPropagation();
                        if (grabadorCel.current?.state === 'recording') {
                          grabadorCel.current.stop();
                        }
                      }}
                      title="Cortar y enviar"
                      aria-label="Cortar la grabación y enviar"
                    >
                      <Check size={17} strokeWidth={2.6} aria-hidden="true" />
                    </button>
                  )}
                  {rutaCelRef.current && <span className="orbe-progreso" aria-hidden="true" />}
                </div>
              ) : (
                <div className="composer">
                  {/* LA CÁMARA: input nativo oculto. `capture="environment"`
                      abre la cámara TRASERA directo en el celular (en la compu
                      cae al selector de archivos, igual sirve). */}
                  <input
                    ref={fotoInputRef}
                    className="foto-input"
                    type="file"
                    accept="image/*"
                    capture="environment"
                    onChange={elegirFoto}
                    aria-hidden="true"
                    tabIndex={-1}
                  />
                  {foto && (
                    <div className="foto-chip">
                      <img src={foto} alt="Foto lista para enviar" />
                      <span className="foto-chip-texto">foto lista para enviar</span>
                      <button
                        type="button"
                        className="foto-chip-quitar"
                        onClick={() => setFoto(null)}
                        title="Quitar la foto"
                        aria-label="Quitar la foto"
                      >
                        <X size={14} strokeWidth={2.4} aria-hidden="true" />
                      </button>
                    </div>
                  )}
                  <textarea
                    ref={textareaRef}
                    value={input}
                    onChange={(e) => setInput(e.target.value)}
                    onKeyDown={(e) => {
                      if (e.key === 'Enter' && !e.shiftKey) {
                        e.preventDefault();
                        enviar();
                      }
                    }}
                    placeholder={`Escribile a ${elegido.nombre}...`}
                    maxLength={300}
                    rows={1}
                    autoFocus
                  />
                  <div className="composer-acciones">
                    <button
                      type="button"
                      className={'icon-btn' + (foto ? ' activo' : '')}
                      onClick={() => fotoInputRef.current?.click()}
                      disabled={respuestaActiva || procesandoFoto}
                      title="Sacá una foto y mandásela: la mira de verdad"
                      aria-label="Sacar una foto"
                    >
                      {procesandoFoto ? (
                        <span className="foto-spinner" aria-hidden="true" />
                      ) : (
                        <Camera size={20} strokeWidth={2} aria-hidden="true" />
                      )}
                    </button>
                    <button
                      type="button"
                      className="icon-btn mic-btn"
                      onClick={escucharHabla}
                      disabled={respuestaActiva}
                      title="Hablale: el micro:bit escucha y Kira responde"
                    >
                      <Mic size={20} strokeWidth={2} aria-hidden="true" />
                    </button>
                    <span className="composer-espacio" aria-hidden="true" />
                    <span
                      className={'char-count' + (input.length > 250 ? ' visible' : '')}
                      aria-hidden="true"
                    >
                      {300 - input.length}
                    </span>
                    {respuestaActiva ? (
                      <button
                        type="button"
                        className="send-btn stop"
                        onClick={detenerRespuesta}
                        title="Cortar la respuesta (Esc)"
                        aria-label="Cortar la respuesta"
                      >
                        <Square
                          size={14}
                          strokeWidth={2.6}
                          fill="currentColor"
                          aria-hidden="true"
                        />
                      </button>
                    ) : (
                      <button
                        type="button"
                        className="send-btn"
                        onClick={() => enviar()}
                        // con una FOTO pendiente se puede enviar aunque no haya texto
                        disabled={!input.trim() && !foto}
                        title="Enviar (Enter · Shift+Enter salto de línea)"
                      >
                        <ArrowUp size={20} strokeWidth={2.4} aria-hidden="true" />
                      </button>
                    )}
                  </div>
                </div>
              )}
            </footer>
          </div>
        )}
      </main>
    </div>
  );
}



