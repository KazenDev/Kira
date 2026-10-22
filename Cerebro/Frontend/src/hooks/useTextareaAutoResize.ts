import { useCallback, useEffect, useRef } from 'react';

/** El textarea crece con el texto hasta un máximo (como los chats modernos). */
export function useTextareaAutoResize(
  valor: string,
  altoMaximo = 132
) {
  const textareaRef = useRef<HTMLTextAreaElement | null>(null);

  const autoResize = useCallback(() => {
    const el = textareaRef.current;
    if (!el) return;
    el.style.height = 'auto';
    el.style.height = Math.min(el.scrollHeight, altoMaximo) + 'px';
  }, [altoMaximo]);

  useEffect(() => {
    autoResize();
  }, [valor, autoResize]);

  return { textareaRef, autoResize };
}
