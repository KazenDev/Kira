import { describe, expect, it } from 'vitest';
import { LETRAS_POR_SEG, topeDeClip } from './voz';

describe('topeDeClip', () => {
  it('usa las marcas de palabra interpolando entre ellas', () => {
    const clip = {
      base: 0,
      largo: 100,
      marcas: [
        [0, 0],
        [1, 50],
        [2, 100],
      ] as [number, number][],
    };
    expect(topeDeClip(clip, 0, NaN)).toBe(0);
    expect(topeDeClip(clip, 0.5, NaN)).toBe(25);
    expect(topeDeClip(clip, 1, NaN)).toBe(50);
    expect(topeDeClip(clip, 1.5, NaN)).toBe(75);
    expect(topeDeClip(clip, 2, NaN)).toBe(100);
  });

  it('no pasa del 100% de las marcas aunque el audio siga', () => {
    const clip = {
      base: 10,
      largo: 20,
      marcas: [
        [0, 0],
        [1, 20],
      ] as [number, number][],
    };
    expect(topeDeClip(clip, 99, NaN)).toBe(30);
  });

  it('con duración conocida reparte proporcional al tiempo', () => {
    const clip = { base: 0, largo: 40, marcas: null };
    expect(topeDeClip(clip, 0, 10)).toBe(0);
    expect(topeDeClip(clip, 5, 10)).toBe(20);
    expect(topeDeClip(clip, 10, 10)).toBe(40);
  });

  it('en streaming estima con LETRAS_POR_SEG', () => {
    const clip = { base: 0, largo: LETRAS_POR_SEG * 2, marcas: null };
    expect(topeDeClip(clip, 1, Infinity)).toBe(LETRAS_POR_SEG);
  });

  it('sin marcas ni duración ni largo no avanza', () => {
    expect(topeDeClip({ base: 3, largo: 0, marcas: null }, 5, NaN)).toBe(3);
  });
});
