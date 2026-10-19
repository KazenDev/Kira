// MODO CARA: Kira a pantalla completa para la feria (una tablet o un
// proyector al lado de la placa). La cara gigante en vivo, y abajo lo que va
// diciendo como SUBTITULOS (el texto ya viaja al ritmo de la voz). Tocarla le
// hace cosquillas: se rie en la pantalla y tambien en la micro:bit de verdad.
import { useEffect, useRef, useState } from 'react';
import { createPortal } from 'react-dom';
import { Mic, X } from 'lucide-react';
import KiraCara from './KiraCara';
import { hacerCosquillas } from './vivo';

interface Props {
  subtitulo: string;
  estadoTexto: string;
  escuchando: boolean;
  puedeHablar: boolean;
  onHablar: () => void;
  onCosquillas: () => void;
  onCerrar: () => void;
}

function tamCara() {
  return Math.round(Math.max(160, Math.min(window.innerWidth * 0.72, window.innerHeight * 0.5, 520)));
}

/** Los subtitulos muestran SOLO el final (lo que esta diciendo ahora). */
export function colaSubtitulo(texto: string, max = 150): string {
  const t = texto.trim();
  if (t.length <= max) return t;
  const corte = t.lastIndexOf(' ', t.length - max + 12);
  return '…' + t.slice(corte > 0 ? corte + 1 : t.length - max);
}

export default function ModoCara({
  subtitulo,
  estadoTexto,
  escuchando,
  puedeHablar,
  onHablar,
  onCosquillas,
  onCerrar,
}: Props) {
  const [tam, setTam] = useState(tamCara);
  const ultimoToque = useRef(0);
  const raiz = useRef<HTMLDivElement | null>(null);
  // el padre pasa una funcion nueva en cada render: si fuera dependencia del
  // efecto, entraria y saldria de pantalla completa sin parar
  const cerrarRef = useRef(onCerrar);
  cerrarRef.current = onCerrar;

  useEffect(() => {
    const medir = () => setTam(tamCara());
    window.addEventListener('resize', medir);
    const esc = (e: KeyboardEvent) => {
      if (e.key === 'Escape') cerrarRef.current();
    };
    window.addEventListener('keydown', esc);
    // pantalla completa de verdad (si el navegador deja; si no, igual tapa todo)
    raiz.current?.requestFullscreen?.().catch(() => undefined);
    return () => {
      window.removeEventListener('resize', medir);
      window.removeEventListener('keydown', esc);
      if (document.fullscreenElement) document.exitFullscreen?.().catch(() => undefined);
    };
  }, []);

  const cosquillas = () => {
    hacerCosquillas();
    // a la placa como mucho una vez cada 0,8 s (si no, la satura)
    const ahora = Date.now();
    if (ahora - ultimoToque.current > 800) {
      ultimoToque.current = ahora;
      onCosquillas();
    }
  };

  // portal al <body>: adentro del panel quedaba DEBAJO de la sidebar (el
  // panel tiene su propio contexto de apilamiento por el fondo vivo)
  return createPortal(
    <div className="modo-cara" ref={raiz} role="dialog" aria-label="Kira a pantalla completa">
      <button type="button" className="modo-cara-cerrar" onClick={onCerrar} aria-label="Salir del modo cara">
        <X size={22} strokeWidth={2} aria-hidden="true" />
      </button>
      <button
        type="button"
        className="modo-cara-tocar"
        onClick={cosquillas}
        aria-label="Hacerle cosquillas a Kira"
        title="Tocala: le hacés cosquillas"
      >
        <KiraCara size={tam} vivo mirarPuntero etiqueta="La cara de Kira" />
      </button>
      <p className="modo-cara-sub" aria-live="polite">
        {colaSubtitulo(subtitulo)}
      </p>
      <div className="modo-cara-pie">
        <span className="modo-cara-estado">{estadoTexto}</span>
        {(puedeHablar || escuchando) && (
          <button
            type="button"
            className={'modo-cara-mic' + (escuchando ? ' activo' : '')}
            onClick={onHablar}
            aria-label={escuchando ? 'Terminar y enviar' : 'Hablarle a Kira'}
          >
            <Mic size={22} strokeWidth={2} aria-hidden="true" />
            <span>{escuchando ? 'Te escucho… tocá para enviar' : 'Hablale'}</span>
          </button>
        )}
      </div>
    </div>,
    document.body
  );
}
