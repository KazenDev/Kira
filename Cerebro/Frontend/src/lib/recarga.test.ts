/**
 * recarga.test.ts — el botón de recargar (el que reemplaza al "deslizá para
 * recargar" que se perdió al bloquear el scroll del documento).
 * Correr: npm run test
 */
import { afterEach, describe, expect, it, vi } from 'vitest';
import { recargarApp } from './recarga';

function stubear(sw: unknown, reload: () => void) {
  vi.stubGlobal('navigator', { serviceWorker: sw });
  vi.stubGlobal('location', { reload });
}

describe('recargarApp', () => {
  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('sin service worker (web normal) recarga al toque', async () => {
    const reload = vi.fn();
    stubear(undefined, reload);
    await recargarApp();
    expect(reload).toHaveBeenCalledTimes(1);
  });

  it('con SW pero sin versión nueva: recarga sin esperar el tope', async () => {
    const reload = vi.fn();
    stubear(
      { controller: {}, getRegistration: async () => ({ update: async () => {} }) },
      reload
    );
    const t0 = Date.now();
    await recargarApp(1500);
    expect(reload).toHaveBeenCalledTimes(1);
    expect(Date.now() - t0).toBeLessThan(300); // no se quedó esperando
  });

  it('con versión nueva: espera a que tome el control y RECIÉN AHÍ recarga', async () => {
    const orden: string[] = [];
    const reload = vi.fn(() => orden.push('reload'));
    stubear(
      {
        controller: {},
        addEventListener: (_t: string, cb: () => void) => {
          orden.push('control');
          setTimeout(cb, 40);
        },
        removeEventListener: () => {},
        getRegistration: async () => ({ update: async () => {}, waiting: {} }),
      },
      reload
    );
    await recargarApp(1500);
    expect(reload).toHaveBeenCalledTimes(1);
    expect(orden).toEqual(['control', 'reload']); // recargó DESPUÉS del SW nuevo
  });

  it('si el SW se cuelga, recarga igual (nunca deja la app sin recargar)', async () => {
    const reload = vi.fn();
    stubear(
      {
        controller: {},
        addEventListener: () => {}, // nunca avisa: simula un SW colgado
        removeEventListener: () => {},
        getRegistration: async () => ({ update: async () => {}, installing: {} }),
      },
      reload
    );
    await recargarApp(60); // tope corto para el test
    expect(reload).toHaveBeenCalledTimes(1);
  });

  it('si el SW tira error, recarga igual', async () => {
    const reload = vi.fn();
    stubear(
      {
        controller: {},
        getRegistration: async () => {
          throw new Error('sin red');
        },
      },
      reload
    );
    await recargarApp();
    expect(reload).toHaveBeenCalledTimes(1);
  });
});
