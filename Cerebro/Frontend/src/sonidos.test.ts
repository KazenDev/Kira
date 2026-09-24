/**
 * sonidos.test.ts — los sonidos NUNCA deben romper la app.
 * vitest corre en entorno node (sin jsdom): el test SIMULA el localStorage
 * del navegador, y de paso cubre los dos peores casos reales:
 *   - localStorage bloqueada (modo incógnito) -> todo apagado, sin lanzar
 *   - sin AudioContext (navegador viejo)       -> sonidoUI es no-op
 * Correr: npm run test
 */
import { afterEach, beforeEach, describe, expect, it } from 'vitest';
import { setSonidoActivo, sonidoActivo, sonidoUI } from './sonidos';

// shim de localStorage del navegador (Map por debajo)
function shimStorage() {
  const mapa = new Map<string, string>();
  (globalThis as Record<string, unknown>).localStorage = {
    getItem: (k: string) => (mapa.has(k) ? (mapa.get(k) as string) : null),
    setItem: (k: string, v: string) => {
      mapa.set(k, String(v));
    },
    removeItem: (k: string) => {
      mapa.delete(k);
    },
    clear: () => mapa.clear(),
  };
}

describe('sonidos: el switch y la robustez', () => {
  beforeEach(shimStorage);
  afterEach(() => {
    delete (globalThis as Record<string, unknown>).localStorage;
  });

  it('apagado por defecto (la feria arranca en silencio)', () => {
    expect(sonidoActivo()).toBe(false);
  });

  it('el switch persiste en localStorage', () => {
    setSonidoActivo(true);
    expect(sonidoActivo()).toBe(true);
    expect(localStorage.getItem('kira_sonido')).toBe('1');
    setSonidoActivo(false);
    expect(sonidoActivo()).toBe(false);
  });

  it('localStorage BLOQUEADA (modo incógnito) -> apagado y sin lanzar', () => {
    (globalThis as Record<string, unknown>).localStorage = {
      getItem() {
        throw new Error('SecurityError: bloqueado');
      },
      setItem() {
        throw new Error('SecurityError: bloqueado');
      },
    };
    expect(sonidoActivo()).toBe(false);
    expect(() => setSonidoActivo(true)).not.toThrow();
    expect(() => sonidoUI('cambio')).not.toThrow();
  });

  it('sonidoUI con el switch APAGADO no hace nada (opcional de verdad)', () => {
    setSonidoActivo(false);
    expect(() => {
      sonidoUI('abrir');
      sonidoUI('cerrar');
      sonidoUI('cambio');
    }).not.toThrow();
  });

  it('sonidoUI ENCENDIDO sin AudioContext (node/navegador viejo) tampoco revienta', () => {
    setSonidoActivo(true);
    expect(() => {
      sonidoUI('abrir');
      sonidoUI('cerrar');
      sonidoUI('cambio');
    }).not.toThrow();
  });
});
