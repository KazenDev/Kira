// El modal de los 3 puntos se salia de la pantalla en el celular.
//
// Causa: 07-product-responsive.css se importa DESPUES de 07-product-chat.css,
// y su regla de movil anclaba `.header-more-menu` con left/right/bottom. Como
// el modal centraba con `translate: -50% -50%`, al pisarle el left/right el
// centrado se desarmaba y el modal terminaba cortado por la derecha y abajo.
//
// Estos tests leen el CSS y verifican las dos cosas que lo hacian posible:
//   1. el centrado NO depende de translate (lo escribe Motion en el transform)
//   2. la regla de movil excluye explicitamente al modal
import { describe, expect, it } from 'vitest';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { dirname, join } from 'node:path';

// Se lee el CSS del disco. Con ?raw o import.meta.glob no sirve: el plugin de
// CSS de Vite transforma el archivo a un módulo vacío antes de que el `?raw`
// lo intercepte (verificado: el valor llega como "").
const aqui = dirname(fileURLToPath(import.meta.url));
const leer = (nombre: string) => readFileSync(join(aqui, '..', 'Estilos', nombre), 'utf-8');
const chat = leer('07-product-chat.css');
const responsive = leer('07-product-responsive.css');

/** Extrae el cuerpo de la primera regla con ese selector exacto, sin comentarios. */
function regla(css: string, selector: string): string {
  const esc = selector.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');
  const m = new RegExp(`${esc}\\s*\\{([^}]*)\\}`, 's').exec(css);
  return m ? m[1].replace(/\/\*[\s\S]*?\*\//g, '') : '';
}

describe('modal de opciones (3 puntos)', () => {
  const modal = regla(chat, '.header-more-modal');

  it('existe la regla del modal', () => {
    expect(modal.length).toBeGreaterThan(0);
  });

  it('centra sin translate (Motion anima transform y se pelearían)', () => {
    expect(modal).not.toMatch(/translate\s*:/);
    // inset:0 + margin:auto es el centrado que no usa transform
    expect(modal).toMatch(/inset\s*:\s*0/);
    expect(modal).toMatch(/margin\s*:\s*auto/);
    expect(modal).toMatch(/height\s*:\s*fit-content/);
  });

  it('nunca puede ser mas ancho que la pantalla', () => {
    expect(modal).toMatch(/width\s*:\s*min\(\s*340px\s*,\s*calc\(100vw\s*-\s*32px\)\s*\)/);
  });

  it('tiene tope de alto para no salirse en pantallas bajas', () => {
    expect(modal).toMatch(/max-height\s*:\s*calc\(100dvh/);
    expect(modal).toMatch(/overflow-y\s*:\s*auto/);
  });

  it('la regla de movil NO toca al modal', () => {
    // El selector anclado abajo debe excluir el modal explicitamente.
    expect(responsive).toMatch(/\.header-more-menu:not\(\.header-more-modal\)/);
    // y no debe existir un anclaje a bottom sobre el modal a secas
    const porBloque = responsive.split('@media').slice(1);
    for (const bloque of porBloque) {
      if (!bloque.includes('.header-more-modal')) continue;
      expect(bloque).not.toMatch(/\.header-more-menu\s*\{[^}]*bottom\s*:/s);
    }
  });
});
