/**
 * recarga.ts — "recargar la app" hecho bien (el botón ⟳ del encabezado).
 *
 * POR QUÉ EXISTE: para que el teclado del celular no corra la página entera,
 * el documento dejó de ser scrolleable (`body { overflow: hidden }`) — y con
 * eso se fue el gesto de "deslizá para recargar". En una PWA instalada ese
 * gesto tampoco existe nunca (no hay barra del navegador). Así que la recarga
 * es un botón dentro de la app.
 *
 * OJO, ESTO NO ES UN location.reload() Y LISTA: el service worker sirve el
 * index.html desde su precache, así que una recarga a secas puede devolver el
 * HTML VIEJO y el usuario cree que recargó pero sigue viendo lo mismo. Acá le
 * pedimos al SW que busque una versión nueva, esperamos a que tome el control
 * y recién ahí recargamos. Si no hay SW (web normal, http plano) recarga ya.
 */
export async function recargarApp(topeMs = 1500): Promise<void> {
  try {
    const sw = navigator.serviceWorker;
    if (sw?.controller) {
      const reg = await sw.getRegistration();
      if (reg) {
        await reg.update(); // ¿hay un sw.js nuevo?
        if (reg.installing || reg.waiting) {
          await esperarControl(sw, topeMs); // que el nuevo tome el control
        }
      }
    }
  } catch {
    // sin red o sin SW: se recarga igual (peor es no poder recargar nunca)
  }
  location.reload();
}

/** Espera el `controllerchange` (o el tope, para no colgar el botón). */
function esperarControl(sw: ServiceWorkerContainer, topeMs: number): Promise<void> {
  return new Promise((resolver) => {
    function terminar() {
      clearTimeout(reloj);
      sw.removeEventListener('controllerchange', terminar);
      resolver();
    }
    const reloj = setTimeout(terminar, topeMs);
    sw.addEventListener('controllerchange', terminar);
  });
}
