// Micrófono de Kira: dos rutas (micro:bit por serial o MediaRecorder del
// celular), medidor de nivel, límite de 60s y botones A/B de la placa.
import { useCallback, useEffect, useRef, useState } from 'react';
import type { Dispatch, MutableRefObject, SetStateAction } from 'react';
import { escucharVoz, obtenerEventos, postSilencioso, transcribirDesdeCel } from '../lib/api';

const LIMITE_GRABACION_MS = 60_000;

type Opciones = {
  /** Hay un personaje abierto (sin él no se puede hablar). */
  activo: boolean;
  /** Kira está escribiendo o el stream sigue pendiente. */
  ocupada: () => boolean;
  /** El SSE del chat sigue abierto (para cortarlo al hablarle a la placa). */
  estaPendiente: () => boolean;
  mbConectadoRef: MutableRefObject<boolean>;
  enviar: (texto: string) => void;
  detenerRespuesta: () => void;
  estaReproduciendo: () => boolean;
  setError: Dispatch<SetStateAction<string>>;
  mostrarErrorTemporal: (mensaje: string, ms?: number) => void;
};

export function useEscucha({
  activo,
  ocupada,
  estaPendiente,
  mbConectadoRef,
  enviar,
  detenerRespuesta,
  estaReproduciendo,
  setError,
  mostrarErrorTemporal,
}: Opciones) {
  const [escuchando, setEscuchando] = useState(false);
  const [nivelEscucha, setNivelEscucha] = useState(0);
  const [segEscucha, setSegEscucha] = useState(0);
  // Mientras AssemblyAI convierte el audio en texto. Sin esto, al soltar el
  // boton de grabar no pasaba NADA visible durante 1-3 segundos (el audio
  // viaja, se transcribe y recien ahi aparece el mensaje): parecia que la
  // grabacion se habia perdido. Con esto la UI muestra "transcribiendo...".
  const [transcribiendo, setTranscribiendo] = useState(false);
  const escuchandoRef = useRef(false);

  const escuchaSesionRef = useRef(0);
  const limiteGrabacionRef = useRef<ReturnType<typeof setTimeout> | null>(null);
  const grabadorCel = useRef<MediaRecorder | null>(null);
  const streamCel = useRef<MediaStream | null>(null);
  const chunksCel = useRef<Blob[]>([]);
  const audioCtxCel = useRef<AudioContext | null>(null);
  const rafNivelCel = useRef<number | null>(null);
  const rutaCelRef = useRef(false);

  // Callbacks frescos sin recrear la grabación en curso.
  const enviarRef = useRef(enviar);
  enviarRef.current = enviar;
  const detenerRespuestaRef = useRef(detenerRespuesta);
  detenerRespuestaRef.current = detenerRespuesta;
  const estaReproduciendoRef = useRef(estaReproduciendo);
  estaReproduciendoRef.current = estaReproduciendo;
  const ocupadaRef = useRef(ocupada);
  ocupadaRef.current = ocupada;
  const estaPendienteRef = useRef(estaPendiente);
  estaPendienteRef.current = estaPendiente;

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
  // y lo vuelca a nivelEscucha (0..90, misma escala que la ruta serial) para
  // que las barras de la píldora reaccionen a la voz REAL.
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

  const detenerGrabacionCel = () => {
    if (grabadorCel.current?.state === 'recording') grabadorCel.current.stop();
  };

  const grabarVozCel = async () => {
    const sesionEscucha = ++escuchaSesionRef.current;

    try {
      // audio: true a secas deja que el navegador aplique cancelacion de eco y
      // supresion de ruido AGRESIVAS, pensadas para llamadas: recortan las
      // consonantes y se comen las palabras cortas, que es justo lo que
      // necesita el reconocedor. Se apagan y se deja el micro limpio.
      // (autoGainControl se mantiene: la feria es ruidosa y sube la voz.)
      const stream = await navigator.mediaDevices.getUserMedia({
        audio: {
          echoCancellation: false,
          noiseSuppression: false,
          autoGainControl: true,
          channelCount: 1,
        },
      });

      // Si el usuario cambió de chat mientras aparecía el permiso del micrófono,
      // no dejamos una captura huérfana viva.
      if (sesionEscucha !== escuchaSesionRef.current) {
        stream.getTracks().forEach((track) => track.stop());
        return;
      }

      streamCel.current = stream;
      chunksCel.current = [];
      prenderAnalizador(stream, sesionEscucha);

      // Bitrate explicito: el default de opus en MediaRecorder ronda los
      // 24-32 kbps y ahi se pierden las fricativas ("s", "f", "j"). 64 kbps
      // es voz limpia de sobra y el archivo sigue pesando poco.
      const rec = new MediaRecorder(stream, {
        audioBitsPerSecond: 64_000,
      });
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

        const blob = new Blob(chunksCel.current, { type: rec.mimeType || 'audio/webm' });
        if (blob.size < 800) {
          mostrarErrorTemporal('No se grabó nada — probá de nuevo');
          return;
        }

        // El audio YA se grabo: de aca en adelante lo que falta es subirlo y
        // transcribirlo. Se avisa en pantalla para que no parezca que el
        // audio se perdio (es el segundo o dos mas largo de todo el flujo).
        setTranscribiendo(true);
        postSilencioso('/api/loading');
        try {
          const texto = ((await transcribirDesdeCel(blob)) ?? '').trim();
          if (sesionEscucha !== escuchaSesionRef.current) return;
          if (!texto) {
            mostrarErrorTemporal('No te escuché — hablá más fuerte y cerca');
            return;
          }
          enviarRef.current(texto);
        } catch (e) {
          if (sesionEscucha !== escuchaSesionRef.current) return;
          mostrarErrorTemporal(e instanceof Error ? e.message : String(e), 5000);
        } finally {
          if (sesionEscucha === escuchaSesionRef.current) setTranscribiendo(false);
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
    if (!activo || ocupadaRef.current()) return;

    // Sin serial, un segundo toque corta MediaRecorder y dispara onstop.
    if (!mbConectadoRef.current) {
      if (escuchando) {
        detenerGrabacionCel();
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
        enviarRef.current(texto);
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

  // ---------- HABLARLE DIRECTO A LA PLACA (botón A) ----------
  // Se aprieta A, se habla, se aprieta A otra vez (B cancela). Dos caminos:
  // - POR AIRE (celular): la placa avisa TECLA:A / TECLA:B y graba el
  //   micrófono del celular (el audio de la placa no entra por Bluetooth).
  // - CON CABLE: el server captura el audio de la placa y avisa OIDO:...
  //   (INICIO, TEXTO:<lo que dijo>, NADA, CANCELADO).
  // Apretar A mientras Kira habla la corta: es la persona queriendo hablar.
  const teclaPlacaRef = useRef<(t: string) => void>(() => undefined);
  teclaPlacaRef.current = (t: string) => {
    if (mbConectadoRef.current || !activo) return; // con cable escucha el server
    const grabandoCel = escuchandoRef.current && rutaCelRef.current;
    if (t === 'A') {
      if (grabandoCel) {
        detenerGrabacionCel(); // envía
      } else if (!escuchandoRef.current) {
        if (estaPendienteRef.current() || estaReproduciendoRef.current()) detenerRespuestaRef.current();
        setError('');
        void grabarVozCel();
      }
    } else if (t === 'B' && grabandoCel) {
      cancelarEscucha();
    }
  };
  const oidoPlacaRef = useRef<(s: string) => void>(() => undefined);
  oidoPlacaRef.current = (s: string) => {
    if (s === 'INICIO') {
      if (estaPendienteRef.current() || estaReproduciendoRef.current()) detenerRespuestaRef.current();
      escuchaSesionRef.current += 1;
      rutaCelRef.current = false;
      setError('');
      setNivelEscucha(0);
      setEscuchando(true);
      escuchandoRef.current = true;
      return;
    }
    if (!escuchandoRef.current || rutaCelRef.current) return; // no era una escucha de la placa
    setEscuchando(false);
    escuchandoRef.current = false;
    if (s.startsWith('TEXTO:')) {
      const texto = s.slice(6).trim();
      if (texto) void enviarRef.current(texto);
    } else if (s === 'NADA') {
      mostrarErrorTemporal('No te escuché nada — hablá más cerca del micro:bit');
    }
  };

  useEffect(() => {
    const alTecla = (e: Event) => teclaPlacaRef.current(String((e as CustomEvent).detail));
    window.addEventListener('kira-tecla', alTecla);
    // con cable: los avisos del server llegan por /api/eventos (aparte de la
    // autonomía, que puede estar apagada: hablarle a la placa anda igual)
    let vivo = true;
    let seq = -1;
    const id = window.setInterval(async () => {
      if (document.hidden || !mbConectadoRef.current) return;
      try {
        const r = await obtenerEventos(seq);
        if (!vivo) return;
        const primera = seq < 0;
        seq = r.seq;
        if (primera) return;
        for (const ev of r.eventos) {
          if (ev.linea.startsWith('OIDO:')) oidoPlacaRef.current(ev.linea.slice(5));
        }
      } catch {
        /* sin server un rato: el próximo intento sigue */
      }
    }, 700);
    return () => {
      vivo = false;
      window.removeEventListener('kira-tecla', alTecla);
      window.clearInterval(id);
    };
  }, [mbConectadoRef]);

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

  // Limpieza final: nada de micrófonos o timers vivos al desmontar.
  useEffect(() => {
    return () => {
      if (limiteGrabacionRef.current) clearTimeout(limiteGrabacionRef.current);
      streamCel.current?.getTracks().forEach((track) => track.stop());
    };
  }, []);

  const estaEscuchando = useCallback(() => escuchandoRef.current, []);

  return {
    escuchando,
    transcribiendo,
    nivelEscucha,
    segEscucha,
    rutaCelRef,
    escucharHabla,
    cancelarEscucha,
    cortarEscucha,
    enviarEscuchaCel: detenerGrabacionCel,
    estaEscuchando,
  };
}
