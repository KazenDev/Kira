// Drawer de conversaciones: apertura, foco, trampa de Tab y Escape.
import { useCallback, useEffect, useRef, useState } from 'react';

export function useSidebar() {
  const [sidebarAbierta, setSidebarAbierta] = useState(false);
  const [sidebarColapsada, setSidebarColapsada] = useState(false);
  const returnFocusRef = useRef<HTMLElement | null>(null);

  const abrirSidebar = useCallback(() => {
    returnFocusRef.current = document.activeElement as HTMLElement | null;
    setSidebarAbierta(true);
  }, []);

  const cerrarSidebar = useCallback(() => {
    setSidebarAbierta(false);
    requestAnimationFrame(() => {
      const anterior = returnFocusRef.current;
      if (anterior?.isConnected) {
        anterior.focus();
      } else if (anterior) {
        document
          .querySelector<HTMLButtonElement>('.chat-header .hamburguesa, .inicio-menu')
          ?.focus();
      }
    });
  }, []);

  // Al abrir, el foco entra al drawer (el botón de cerrar es el primero).
  useEffect(() => {
    if (!sidebarAbierta) return;
    const id = window.setTimeout(() => {
      document.querySelector<HTMLButtonElement>('.sidebar-cerrar-movil')?.focus();
    }, 0);
    return () => window.clearTimeout(id);
  }, [sidebarAbierta]);

  // El drawer móvil se comporta como una capa real: Tab cicla adentro y
  // Escape siempre lo cierra.
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

  return {
    sidebarAbierta,
    sidebarColapsada,
    setSidebarColapsada,
    abrirSidebar,
    cerrarSidebar,
    sidebarReturnFocusRef: returnFocusRef,
  };
}
