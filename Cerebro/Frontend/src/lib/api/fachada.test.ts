import { describe, expect, it } from 'vitest';
import { PREFERENCIAS_MEMORIA_POR_DEFECTO } from './memoria';
import { topeDeClip } from '../voz';

describe('PREFERENCIAS_MEMORIA_POR_DEFECTO', () => {
  it('arranca con toda la memoria encendida', () => {
    expect(PREFERENCIAS_MEMORIA_POR_DEFECTO).toEqual({
      recordar: true,
      hechos: true,
      experiencias: true,
      usar: true,
    });
  });

  it('cada copia es independiente (no comparte referencia)', () => {
    const copia = { ...PREFERENCIAS_MEMORIA_POR_DEFECTO };
    copia.recordar = false;
    expect(PREFERENCIAS_MEMORIA_POR_DEFECTO.recordar).toBe(true);
  });

  it('sigue exportándose por la fachada pública', async () => {
    const api = await import('./index');
    expect(api.PREFERENCIAS_MEMORIA_POR_DEFECTO).toBe(PREFERENCIAS_MEMORIA_POR_DEFECTO);
    expect(typeof api.fetchKira).toBe('function');
    expect(typeof api.enviarMensajeStream).toBe('function');
    expect(typeof api.bleTomarTx).toBe('function');
    expect(typeof api.obtenerMemoria).toBe('function');
    expect(typeof api.obtenerEventos).toBe('function');
    expect(typeof api.horaActual).toBe('function');
  });

  it('el cálculo de voz sigue disponible para el reproductor', () => {
    expect(topeDeClip({ base: 0, largo: 10, marcas: null }, 0, 0)).toBe(0);
  });
});
