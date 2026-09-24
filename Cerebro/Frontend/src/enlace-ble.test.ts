/**
 * enlace-ble.test.ts — la guardia del enlace (el bug: 28 comandos mandados,
 * 1 sola respuesta de la placa). Correr: npm run test
 */
import { describe, expect, it } from 'vitest';
import { SILENCIO, debeRearmarEnlace, msDeSilencio, type DatosEnlace } from './enlace-ble';

const base: DatosEnlace = {
  esDueno: true,
  tieneEnlace: true,
  comandosSinRespuesta: 3,
  msDeSilencio: 20_000,
  msDesdeUltimoRearme: Infinity,
};

describe('debeRearmarEnlace', () => {
  it('enlace MUDO con comandos pendientes: re-arma', () => {
    expect(debeRearmarEnlace(base)).toBe(true);
  });

  it('la placa responde normal: NO re-arma', () => {
    expect(debeRearmarEnlace({ ...base, msDeSilencio: 1_500 })).toBe(false);
  });

  it('sin comandos pendientes NO re-arma: que la placa calle es normal', () => {
    expect(debeRearmarEnlace({ ...base, comandosSinRespuesta: 0 })).toBe(false);
  });

  it('si no somos el puente, no tocamos nada', () => {
    expect(debeRearmarEnlace({ ...base, esDueno: false })).toBe(false);
  });

  it('sin enlace GATT no re-arma (eso lo cubre la reconexion de siempre)', () => {
    expect(debeRearmarEnlace({ ...base, tieneEnlace: false })).toBe(false);
  });

  it('todavia no paso nada en la sesion: no re-arma', () => {
    expect(debeRearmarEnlace({ ...base, msDeSilencio: null })).toBe(false);
  });

  it('respeta el cooldown: no re-arma dos veces seguidas', () => {
    expect(debeRearmarEnlace({ ...base, msDesdeUltimoRearme: 5_000 })).toBe(false);
  });

  it('pasado el cooldown y si sigue mudo, re-arma otra vez', () => {
    expect(debeRearmarEnlace({ ...base, msDesdeUltimoRearme: SILENCIO.rearmeMinMs + 1 })).toBe(true);
  });

  it('el limite exacto del silencio YA alcanza (un milisegundo antes, no)', () => {
    expect(debeRearmarEnlace({ ...base, msDeSilencio: SILENCIO.silencioMs - 1 })).toBe(false);
    expect(debeRearmarEnlace({ ...base, msDeSilencio: SILENCIO.silencioMs })).toBe(true);
  });

  it('acepta una configuracion propia (tests/ferias con otros tiempos)', () => {
    const cfg = { silencioMs: 1_000, rearmeMinMs: 2_000 };
    // con la config del test (mas corta) re-arma mucho antes que con la real
    expect(
      debeRearmarEnlace({ ...base, msDeSilencio: 1_500, msDesdeUltimoRearme: 5_000 }, cfg)
    ).toBe(true);
    // y con los tiempos REALES ese mismo caso todavia no re-arma
    expect(debeRearmarEnlace({ ...base, msDeSilencio: 1_500, msDesdeUltimoRearme: 5_000 })).toBe(
      false
    );
  });

  it('caso REAL del 17-sep: 28 comandos, 1 respuesta hace 12s -> re-arma', () => {
    expect(
      debeRearmarEnlace({
        esDueno: true,
        tieneEnlace: true,
        comandosSinRespuesta: 28,
        msDeSilencio: 12_000,
        msDesdeUltimoRearme: Infinity,
      })
    ).toBe(true);
  });
});

describe('msDeSilencio', () => {
  it('sin señales todavia devuelve null', () => {
    expect(msDeSilencio(10_000, 0, 0)).toBeNull();
  });

  it('cuenta desde la señal MAS RECIENTE (un comando recien mandado reinicia)', () => {
    expect(msDeSilencio(10_000, 2_000, 9_000)).toBe(1_000);
  });

  it('si la placa respondio despues, cuenta desde esa respuesta', () => {
    expect(msDeSilencio(10_000, 9_500, 3_000)).toBe(500);
  });

  it('nunca devuelve negativo (relojes que se cruzan)', () => {
    expect(msDeSilencio(1_000, 5_000, 0)).toBe(0);
  });
});
