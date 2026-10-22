// Motor de conversación de Kira: compone voz, typewriter, micrófono,
// autonomía y el envío/streaming. App se queda con navegación y render.
import { useCallback, useEffect, useRef, useState } from 'react';
import type { Dispatch, MutableRefObject, RefObject, SetStateAction } from 'react';
import {
  enviarMensajeStream,
  horaActual,
  postSilencioso,
  type MensajeChat,
  type PersonajeInfo,
  type PreferenciasMemoria,
} from '../lib/api';
import { uuid } from '../lib/ui-utils';
import { sonidoEnviar, sonidoRecibir } from '../lib/sonidos';
import { useTypewriter } from './useTypewriter';
import { useReproductorVoz } from './useReproductorVoz';
import { useEscucha } from './useEscucha';
import { useAutonomia } from './useAutonomia';

type Opciones = {
  elegido: PersonajeInfo | null;
  chatActivo: string | null;
  mensajes: MensajeChat[];
  setMensajes: Dispatch<SetStateAction<MensajeChat[]>>;
  input: string;
  setInput: Dispatch<SetStateAction<string>>;
  foto: string | null;
  setFoto: Dispatch<SetStateAction<string | null>>;
  ajustes: {
    voz: boolean;
    volumenVoz: number;
    autonomia: boolean;
    memoria: PreferenciasMemoria;
  };
  mbConectadoRef: MutableRefObject<boolean>;
  textareaRef: RefObject<HTMLTextAreaElement>;
  setAutoScroll: Dispatch<SetStateAction<boolean>>;
  setError: Dispatch<SetStateAction<string>>;
  mostrarErrorTemporal: (mensaje: string, ms?: number) => void;
  generarTituloChat: (
    sesionId: string,
    pjId: string,
    primerMensaje: string,
    respuesta: string
  ) => void;
};

export function useMotorConversacion({
  elegido,
  chatActivo,
  mensajes,
  setMensajes,
  input,
  setInput,
  foto,
  setFoto,
  ajustes,
  mbConectadoRef,
  textareaRef,
  setAutoScroll,
  setError,
  mostrarErrorTemporal,
  generarTituloChat,
}: Opciones) {
  const [escribiendo, setEscribiendo] = useState(false);
  const [streamPendiente, setStreamPendiente] = useState(false);
  const [herramienta, setHerramienta] = useState<string | null>(null);
  const [emotion, setEmotion] = useState('happy');
  const [hablando, setHablando] = useState(false);

  const streamPendienteRef = useRef(false);
  const escribiendoRef = useRef(false);
  escribiendoRef.current = escribiendo;
  const abortRef = useRef<AbortController | null>(null);
  const respuestaSesionRef = useRef(0);
  const enviarRef = useRef<((t?: string, r?: string, e?: string) => Promise<void>) | null>(null);

  // El typewriter y el reproductor se necesitan mutuamente; los refs rompen
  // el ciclo sin recrear callbacks en cada render.
  const arrancarTypewriterRef = useRef<() => void>(() => {});
  const largoObjetivoRef = useRef<() => number>(() => 0);

  const reproductor = useReproductorVoz({
    vozActiva: ajustes.voz,
    volumen: ajustes.volumenVoz,
    setEscribiendo,
    setHablando,
    arrancarTypewriter: () => arrancarTypewriterRef.current(),
    largoObjetivo: () => largoObjetivoRef.current(),
  });

  const typewriter = useTypewriter({
    setMensajes,
    topeVoz: reproductor.topeVoz,
    sincronizarBoca: reproductor.hayClip,
  });
  arrancarTypewriterRef.current = typewriter.arrancar;
  largoObjetivoRef.current = typewriter.largoObjetivo;

  const detenerRespuestaRef = useRef<() => void>(() => {});

  const escucha = useEscucha({
    activo: !!elegido,
    ocupada: () => escribiendoRef.current || streamPendienteRef.current,
    estaPendiente: () => streamPendienteRef.current,
    mbConectadoRef,
    enviar: (texto) => void enviarRef.current?.(texto),
    detenerRespuesta: () => detenerRespuestaRef.current(),
    estaReproduciendo: reproductor.estaReproduciendo,
    setError,
    mostrarErrorTemporal,
  });

  const autonomia = useAutonomia({
    activa: ajustes.autonomia,
    libre: () =>
      !streamPendienteRef.current &&
      !reproductor.estaReproduciendo() &&
      !escucha.estaEscuchando(),
    hablarSola: (evento) => void enviarRef.current?.(undefined, undefined, evento),
  });

  const detenerRespuesta = useCallback(() => {
    const idEnCurso = typewriter.idEnCurso();
    respuestaSesionRef.current += 1;
    abortRef.current?.abort();
    abortRef.current = null;
    typewriter.terminar();
    setEscribiendo(false);
    streamPendienteRef.current = false;
    setStreamPendiente(false);
    setHablando(false);
    typewriter.detener();

    // Si se corta antes del primer carácter, no dejamos un mensaje vacío fantasma.
    if (idEnCurso) {
      setMensajes((actuales) => {
        const parcial = actuales.find((m) => m.id === idEnCurso);
        return parcial && !parcial.contenido.trim()
          ? actuales.filter((m) => m.id !== idEnCurso)
          : actuales;
      });
    }
    reproductor.detener();
  }, [reproductor.detener, setMensajes, typewriter.detener, typewriter.idEnCurso, typewriter.terminar]);
  detenerRespuestaRef.current = detenerRespuesta;

  const reproducirMensaje = useCallback(
    (urls: string[]) => {
      reproductor.reproducirMensaje(
        urls,
        !escribiendo && !streamPendienteRef.current && !hablando
      );
    },
    [reproductor.reproducirMensaje, escribiendo, hablando]
  );

  useEffect(() => {
    typewriter.detener();
  }, [chatActivo, typewriter.detener]);

  // Limpieza final: nada de streams vivos al desmontar.
  useEffect(() => {
    return () => {
      abortRef.current?.abort();
    };
  }, []);

  // evento: turno de AUTONOMÍA (nadie escribió: la placa avisó algo o hubo
  // un silencio largo). No hay burbuja del usuario ni loading en la placa.
  const enviar = async (textoDesdeVoz?: string, reemplazarId?: string, evento?: string) => {
    if (!elegido || escribiendo || streamPendienteRef.current) return;
    if (!evento) autonomia.marcarActividad();
    const sesionRespuesta = ++respuestaSesionRef.current;
    const esRegeneracion = !!reemplazarId;
    let base = mensajes;
    if (esRegeneracion) {
      const idx = mensajes.findIndex((m) => m.id === reemplazarId);
      const idxUser =
        idx > 0 ? [...mensajes.slice(0, idx)].map((m) => m.rol).lastIndexOf('usuario') : -1;
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
    typewriter.ultimoTextoRef.current = texto; // botón "reintentar"
    if (!esRegeneracion && !evento) {
      sonidoEnviar();
      setInput('');
      setFoto(null);
    }
    setError('');
    setAutoScroll(true);
    setHablando(false);
    if (textareaRef.current) textareaRef.current.style.height = 'auto';
    typewriter.preparar();

    const controller = new AbortController();
    abortRef.current = controller;

    const esPrimerIntercambio =
      !esRegeneracion && !evento && !mensajes.some((m) => m.rol === 'usuario');
    const primerMensajeChat = esPrimerIntercambio ? texto || '📷 (foto)' : '';

    const idParcial = reemplazarId ?? uuid();
    typewriter.iniciar(idParcial);
    if (esRegeneracion) {
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
    setHerramienta(null);
    reproductor.prepararRespuesta();
    // LA PLACA FÍSICA TAMBIÉN PIENSA: LOADING mientras la IA contesta.
    // (En un turno por evento no: la placa está en plena reacción.)
    if (!evento) postSilencioso('/api/loading');

    const historial = base.map((m) => ({
      rol: m.rol === 'usuario' ? 'user' : 'assistant',
      // las fotos VIEJAS viajan como texto: la imagen real solo va en el turno
      // nuevo (si no, cada turno reenviaría megabytes de base64 al vacío)
      contenido: m.contenido.trim() || (m.imagen ? '(te mandé una foto)' : ''),
      // el rastro vuelve hacia atrás a propósito: es lo que le permite a Kira
      // citar sus propias fuentes si le repreguntan.
      rastro: m.rol === 'kira' ? (m.rastro ?? null) : null,
      emotion: m.rol === 'kira' ? (m.emotion ?? null) : null,
    }));

    await enviarMensajeStream(
      elegido.id,
      texto,
      historial,
      // onDelta: el texto va llegando; el typewriter lo revela de a poco
      (textoParcial) => {
        if (sesionRespuesta !== respuestaSesionRef.current) return;
        typewriter.setObjetivo(textoParcial);
        typewriter.arrancar();
      },
      // onFin: respuesta completa + cola de voz
      (resp) => {
        if (sesionRespuesta !== respuestaSesionRef.current) return;
        setHerramienta(null); // ya hay respuesta: el trace queda viejo
        sonidoRecibir(); // notita suave al llegar la respuesta (si está activo)
        typewriter.setObjetivo(resp.message);
        // la emoción se actualiza YA (sincrónica): si el TTS es corto y termina
        // antes de que el typewriter acabe, la cara no debe volver a la vieja
        setEmotion(resp.emotion);
        postSilencioso('/api/emocion', {
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ emotion: resp.emotion }),
        });
        const audioUrls = resp.tts_urls?.length
          ? resp.tts_urls
          : resp.tts_url
            ? [resp.tts_url]
            : null;
        reproductor.marcarFin();
        reproductor.soltarSiNoQuedaVoz(resp.tts_urls);
        const completar = () => {
          if (sesionRespuesta !== respuestaSesionRef.current) return;
          streamPendienteRef.current = false;
          setStreamPendiente(false);
          abortRef.current = null;
          typewriter.terminar();
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
          // la emoción ya se mandó arriba: sin voz solo cortamos el "escribiendo"
          setEscribiendo(false);
        };
        if (reproductor.vozEncendida() && resp.tts_urls?.length) {
          // TTS POR FRASES: la lista autoritativa (en orden). Los eventos
          // 'audio' ya adelantaron algunas: encolar() deduplica.
          for (let i = 0; i < resp.tts_urls.length; i++) {
            reproductor.encolar(resp.tts_urls[i], i);
          }
        } else if (reproductor.vozEncendida() && resp.tts_url) {
          // compat: una sola URL (respuestas cortas de una frase)
          reproductor.jugarAudio(resp.tts_url);
        } else {
          typewriter.setFin(() => {
            completar();
            sinVoz();
          });
        }
        if (typewriter.revelado() >= resp.message.length) {
          // el typewriter ya mostró todo: completamos directo
          completar();
          if (!reproductor.vozEncendida() || !resp.tts_url) sinVoz();
          typewriter.setFin(null);
        } else {
          // falta revelar: al terminar se ejecuta completar (o el fin que ya
          // había puesto la rama sin voz, que además apaga "escribiendo")
          if (!typewriter.hayFin()) typewriter.setFin(completar);
          typewriter.arrancar();
        }
      },
      // onError: el stream se cortó o la IA falló a mitad
      (e) => {
        if (sesionRespuesta !== respuestaSesionRef.current) return;
        setHerramienta(null);
        abortRef.current = null;
        typewriter.detener();
        typewriter.terminar();
        setEscribiendo(false);
        streamPendienteRef.current = false;
        setStreamPendiente(false);
        // si fue el USUARIO quien cortó (stop/Escape), no es un error: se
        // conserva el texto parcial. Si Kira hablaba SOLA (evento) tampoco.
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
        // si el mensaje parcial quedó VACÍO se borra (burbuja fantasma); con
        // texto parcial se conserva (mejor perder algo que nada)
        setMensajes((m) => {
          const parcial = m.find((msg) => msg.id === idParcial);
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
        if (!reproductor.vozEncendida()) return;
        reproductor.encolar(url, orden, textoFrase);
      },
      chatActivo,
      // LA FOTO: el último argumento (null si no hay) -> el backend la mete en
      // el mensaje del usuario como bloque image_url y la IA la ve
      fotoEnviada,
      // tool trace: el backend avisa qué herramienta está usando
      setHerramienta,
      ajustes.memoria,
      evento ?? null
    );
  };
  enviarRef.current = enviar;

  return {
    escribiendo,
    streamPendiente,
    streamId: typewriter.streamId,
    herramienta,
    emotion,
    setEmotion,
    hablando,
    setHablando,
    escuchando: escucha.escuchando,
    transcribiendo: escucha.transcribiendo,
    nivelEscucha: escucha.nivelEscucha,
    segEscucha: escucha.segEscucha,
    rutaCelRef: escucha.rutaCelRef,
    enviar,
    detenerRespuesta,
    reproducirMensaje,
    escucharHabla: escucha.escucharHabla,
    cancelarEscucha: escucha.cancelarEscucha,
    cortarEscucha: escucha.cortarEscucha,
    enviarEscuchaCel: escucha.enviarEscuchaCel,
    ultimoTextoRef: typewriter.ultimoTextoRef,
  };
}
