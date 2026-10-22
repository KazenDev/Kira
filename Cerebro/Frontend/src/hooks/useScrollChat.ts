import { useCallback, useRef, useState } from 'react';

/** Scroll inteligente del chat: expone el contenedor, el estado de "el usuario
 *  está abajo" y helpers. El efecto de seguir el texto vive en App porque
 *  necesita los estados del motor de conversación. */
export function useScrollChat() {
  const [autoScroll, setAutoScroll] = useState(true);
  const [scrolleo, setScrolleo] = useState(false);
  const mensajesRef = useRef<HTMLElement | null>(null);

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

  return { mensajesRef, autoScroll, setAutoScroll, scrolleo, revisarScroll, bajarAlFondo };
}
