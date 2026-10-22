// Reproductor de la voz de Kira: cola de clips TTS, audios sincronizados con
// el typewriter y la boca del micro:bit. No sabe nada del stream de chat.
import { useCallback, useEffect, useRef } from 'react';
import { fetchKira, postSilencioso } from '../lib/api';
import { topeDeClip, type AudioEnCola, type ClipVoz, type MarcasVoz } from '../lib/voz';

type Opciones = {
  vozActiva: boolean;
  volumen: number;
  setEscribiendo: (v: boolean) => void;
  setHablando: (v: boolean) => void;
  /** Arranca/reanuda el typewriter cuando hay un clip listo. */
  arrancarTypewriter: () => void;
  /** Texto completo del mensaje que se está revelando (para calcular largos). */
  largoObjetivo: () => number;
};

export function useReproductorVoz({
  vozActiva,
  volumen,
  setEscribiendo,
  setHablando,
  arrancarTypewriter,
  largoObjetivo,
}: Opciones) {
  const callaTimerRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const colaAudioRef = useRef<AudioEnCola[]>([]);
  const vistosAudioRef = useRef<Set<string>>(new Set());
  const ordenAudioRef = useRef(0);
  const reproduciendoAudioRef = useRef(false);
  const audioElRef = useRef<HTMLAudioElement | null>(null);
  const turnoAudioRef = useRef(0);
  const vozActivadaRef = useRef(vozActiva);
  const volumenVozRef = useRef(volumen);
  const blobsAudioRef = useRef<
    Map<string, Promise<{ blob: Blob; marcas: MarcasVoz | null }>>
  >(new Map());
  const limiteVozRef = useRef<number>(Infinity);
  const clipVozRef = useRef<ClipVoz | null>(null);
  const finVozRef = useRef(false);

  useEffect(() => {
    vozActivadaRef.current = vozActiva;
    volumenVozRef.current = volumen;
    const audio = audioElRef.current;
    if (audio) audio.volume = volumen;
    if (!vozActiva && audio && !audio.paused) {
      turnoAudioRef.current += 1;
      audio.pause();
      colaAudioRef.current = [];
      reproduciendoAudioRef.current = false;
      limiteVozRef.current = Infinity;
      clipVozRef.current = null;
      setEscribiendo(false);
      setHablando(false);
      postSilencioso('/api/calla');
    }
  }, [vozActiva, volumen, setEscribiendo, setHablando]);

  // AUDIO PERSISTENTE + DESBLOQUEO POR GESTO: Android/iOS bloquean el play()
  // programático si el usuario no tocó la página. Un único elemento de audio,
  // desbloqueado al primer toque, reutilizado para siempre.
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
  // cerraba entre frase y frase (justo en los "..."), y se veía y se oía
  // como que la voz se cortaba y volvía.
  const jugarAudio = useCallback(
    (url: string, alTerminar?: () => void, siguePara?: () => boolean) => {
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
      audio.src = url;
      audio.muted = false;
      audio.volume = volumenVozRef.current;
      // SYNC CON LA LATENCIA BLE: el TALK sale apenas se pide el audio, no
      // cuando arranca, para que la boca y el sonido aterricen juntos.
      postSilencioso('/api/talk');
      audio.onplaying = () => {
        setEscribiendo(false);
        setHablando(true);
      };
      let terminado = false;
      let vigia = 0;
      const terminarAudio = () => {
        if (terminado) return;
        terminado = true;
        window.clearInterval(vigia);
        setEscribiendo(false);
        setHablando(false);
        if (callaTimerRef.current) {
          clearTimeout(callaTimerRef.current);
          callaTimerRef.current = null;
        }
        if (!siguePara?.()) postSilencioso('/api/calla');
        alTerminar?.();
      };
      audio.onloadedmetadata = () => {
        if (callaTimerRef.current) clearTimeout(callaTimerRef.current);
        const ms = (audio.duration || 0) * 1000;
        if (Number.isFinite(ms) && ms > 800) {
          callaTimerRef.current = setTimeout(() => {
            callaTimerRef.current = null;
            if (!siguePara?.()) postSilencioso('/api/calla');
          }, ms - 350);
        }
      };
      audio.onended = terminarAudio;
      audio.onerror = terminarAudio;
      // VIGIA: a veces el navegador deja el audio TRABADO sin 'ended' ni
      // 'error'; si no avanza, se da por terminado y la charla sigue.
      let ultimoT = -1;
      let ultimoAvance = Date.now();
      vigia = window.setInterval(() => {
        if (turnoAudioRef.current !== miTurno) {
          window.clearInterval(vigia);
          return;
        }
        const t = audio.currentTime;
        if (document.hidden || t !== ultimoT) {
          ultimoT = t;
          ultimoAvance = Date.now();
          return;
        }
        const paciencia = t > 0 ? 3500 : 9000;
        if (Date.now() - ultimoAvance > paciencia) {
          console.warn('[voz] audio trabado: lo salteo', url);
          audio.pause();
          terminarAudio();
        }
      }, 500);
      audio.play().catch(() => terminarAudio());
      const t = setTimeout(() => {
        setEscribiendo(false);
      }, 30000);
      audio.addEventListener('playing', () => clearTimeout(t), { once: true });
      audio.addEventListener('ended', () => clearTimeout(t), { once: true });
    },
    [setEscribiendo, setHablando]
  );

  // Cada clip se baja entero y DESPUÉS sus tiempos (el server los tiene recién
  // cuando terminó de generar el audio). Sin tiempos -> null: ritmo parejo.
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

  const liberarVoz = useCallback(() => {
    limiteVozRef.current = Infinity;
    clipVozRef.current = null;
  }, []);

  // El tope de voz se consulta 30 veces por segundo desde el typewriter; se
  // cachea por frame para no recalcular la interpolación de marcas cada vez.
  const topeCacheRef = useRef<{ t: number; valor: number }>({ t: -1, valor: Infinity });
  const topeVoz = useCallback((): number => {
    const clip = clipVozRef.current;
    if (!clip) return limiteVozRef.current;
    const audio = audioElRef.current;
    const t = audio?.currentTime ?? 0;
    // mismo instante de audio => mismo tope (el typewriter llama varias veces
    // por frame y el tiempo del <audio> no cambia hasta el próximo frame)
    if (topeCacheRef.current.t === t) return topeCacheRef.current.valor;
    const valor = topeDeClip(clip, t, audio?.duration ?? NaN);
    topeCacheRef.current = { t, valor };
    return valor;
  }, []);

  const avanzarColaAudio = useCallback(() => {
    colaAudioRef.current
      .filter((c) => c.orden > ordenAudioRef.current && c.orden <= ordenAudioRef.current + 2)
      .forEach((c) => obtenerBlobAudio(c.url));
    if (reproduciendoAudioRef.current) return;
    const cabeza = colaAudioRef.current[0];
    if (!cabeza || cabeza.orden !== ordenAudioRef.current) return;
    colaAudioRef.current.shift();
    reproduciendoAudioRef.current = true;
    const miOrden = cabeza.orden;
    colaAudioRef.current
      .filter((c) => c.orden > miOrden && c.orden <= miOrden + 2)
      .forEach((c) => obtenerBlobAudio(c.url));
    const hayOtro = () => colaAudioRef.current.some((c) => c.orden === miOrden + 1);
    const sincroniza = limiteVozRef.current !== Infinity;
    const base = sincroniza ? (limiteVozRef.current === 0 ? 0 : limiteVozRef.current + 1) : 0;
    const largo = cabeza.texto != null ? cabeza.texto.length : Math.max(0, largoObjetivo() - base);
    const terminoClip = () => {
      if (limiteVozRef.current !== Infinity) {
        limiteVozRef.current = base + largo;
        clipVozRef.current = null;
        if (!hayOtro() && finVozRef.current) liberarVoz();
      }
      arrancarTypewriter();
    };
    if (
      miOrden === 0 &&
      !blobsAudioRef.current.has(cabeza.url) &&
      cabeza.url.includes('/api/tts-stream/')
    ) {
      if (limiteVozRef.current !== Infinity) clipVozRef.current = { base, largo, marcas: null };
      arrancarTypewriter();
      jugarAudio(
        cabeza.url,
        () => {
          terminoClip();
          reproduciendoAudioRef.current = false;
          ordenAudioRef.current = miOrden + 1;
          avanzarColaAudio();
        },
        hayOtro
      );
      return;
    }
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
        jugarAudio(
          objUrl,
          () => {
            URL.revokeObjectURL(objUrl);
            terminoClip();
            reproduciendoAudioRef.current = false;
            ordenAudioRef.current = miOrden + 1;
            avanzarColaAudio();
          },
          hayOtro
        );
      })
      .catch(() => {
        terminoClip();
        reproduciendoAudioRef.current = false;
        ordenAudioRef.current = miOrden + 1;
        avanzarColaAudio();
      });
  }, [jugarAudio, obtenerBlobAudio, liberarVoz, arrancarTypewriter, largoObjetivo]);

  /** Arranca una respuesta nueva: cola vacía y el texto espera a la voz. */
  const prepararRespuesta = useCallback(() => {
    colaAudioRef.current = [];
    vistosAudioRef.current.clear();
    ordenAudioRef.current = 0;
    reproduciendoAudioRef.current = false;
    limiteVozRef.current = vozActivadaRef.current ? 0 : Infinity;
    clipVozRef.current = null;
    finVozRef.current = false;
  }, []);

  /** Agrega una frase que ya tiene audio (evento del stream o lista final). */
  const encolar = useCallback(
    (url: string, orden: number, texto?: string) => {
      if (vistosAudioRef.current.has(url)) return;
      vistosAudioRef.current.add(url);
      colaAudioRef.current.push({ url, orden, texto });
      colaAudioRef.current.sort((a, b) => a.orden - b.orden);
      avanzarColaAudio();
    },
    [avanzarColaAudio]
  );

  /** Marca que el stream cerró: lo que sobre ya no espera a la voz. */
  const marcarFin = useCallback(() => {
    finVozRef.current = true;
  }, []);

  /** Suelta el tope de voz si no queda nada por sonar. */
  const soltarSiNoQuedaVoz = useCallback(
    (urls: (string | null | undefined)[] | null | undefined) => {
      const queda =
        colaAudioRef.current.length > 0 ||
        reproduciendoAudioRef.current ||
        (urls ?? []).some((u) => !!u && !vistosAudioRef.current.has(u));
      if (!vozActivadaRef.current || !urls?.length || !queda) liberarVoz();
    },
    [liberarVoz]
  );

  const estaReproduciendo = useCallback(() => reproduciendoAudioRef.current, []);
  const vozEncendida = useCallback(() => vozActivadaRef.current, []);
  const hayClip = useCallback(() => clipVozRef.current !== null, []);

  const reproducirMensaje = useCallback(
    (urls: string[], permitido: boolean) => {
      const lista = urls.filter(Boolean);
      const audioOcupado = audioElRef.current && !audioElRef.current.paused;
      if (!lista.length || !permitido || audioOcupado || reproduciendoAudioRef.current) return;
      colaAudioRef.current = lista.map((url, orden) => ({ url, orden }));
      ordenAudioRef.current = 0;
      reproduciendoAudioRef.current = false;
      avanzarColaAudio();
    },
    [avanzarColaAudio]
  );

  const detener = useCallback(() => {
    liberarVoz();
    colaAudioRef.current = [];
    vistosAudioRef.current.clear();
    ordenAudioRef.current = 0;
    reproduciendoAudioRef.current = false;
    if (callaTimerRef.current) {
      clearTimeout(callaTimerRef.current);
      callaTimerRef.current = null;
    }
    turnoAudioRef.current += 1; // el vigia del audio cortado no debe seguir la cola
    if (audioElRef.current && !audioElRef.current.paused) audioElRef.current.pause();
    postSilencioso('/api/calla');
  }, [liberarVoz]);

  useEffect(() => {
    return () => {
      if (callaTimerRef.current) clearTimeout(callaTimerRef.current);
      const audio = audioElRef.current;
      if (audio && !audio.paused) audio.pause();
    };
  }, []);

  return {
    jugarAudio,
    reproducirMensaje,
    prepararRespuesta,
    encolar,
    marcarFin,
    soltarSiNoQuedaVoz,
    liberarVoz,
    topeVoz,
    detener,
    estaReproduciendo,
    vozEncendida,
    hayClip,
  };
}
