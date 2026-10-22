import { useEffect, useRef } from 'react';

/** Foco de un modal: guarda el elemento anterior, enfoca el panel al abrir,
 *  atrapa Tab y devuelve el foco al cerrar. */
export function useFocoModal(abierto: boolean) {
  const returnFocusRef = useRef<HTMLElement | null>(null);
  const panelRef = useRef<HTMLDivElement | null>(null);
  const closeRef = useRef<HTMLButtonElement | null>(null);

  useEffect(() => {
    if (!abierto) {
      const objetivo = returnFocusRef.current;
      returnFocusRef.current = null;
      if (!objetivo) return;
      const rect = objetivo.getBoundingClientRect();
      const sigueVisible = Boolean(
        rect &&
          rect.right > 0 &&
          rect.left < window.innerWidth &&
          rect.bottom > 0 &&
          rect.top < window.innerHeight
      );
      const vuelveAlDrawerOculto =
        objetivo.closest('.sidebar') !== null && window.innerWidth <= 860;
      if (!vuelveAlDrawerOculto && sigueVisible && objetivo) objetivo.focus();
      else
        document
          .querySelector<HTMLButtonElement>('.chat-header .hamburguesa, .inicio-menu')
          ?.focus();
      return;
    }
    if (!returnFocusRef.current?.isConnected) {
      returnFocusRef.current = document.activeElement as HTMLElement | null;
    }
    const focusTimer = window.setTimeout(() => closeRef.current?.focus(), 0);

    const mantenerFoco = (event: KeyboardEvent) => {
      if (event.key !== 'Tab') return;
      const focusables = panelRef.current?.querySelectorAll<HTMLElement>(
        'button:not([disabled]), input:not([disabled]), [href], [tabindex]:not([tabindex="-1"])'
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
    };

    document.addEventListener('keydown', mantenerFoco);
    return () => {
      window.clearTimeout(focusTimer);
      document.removeEventListener('keydown', mantenerFoco);
    };
  }, [abierto]);

  return { panelRef, closeRef };
}
