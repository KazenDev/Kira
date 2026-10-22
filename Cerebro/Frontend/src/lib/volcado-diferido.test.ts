import { describe, expect, it, vi, beforeEach, afterEach } from 'vitest';
import { crearVolcadoDiferido } from './volcado-diferido';

describe('crearVolcadoDiferido', () => {
  beforeEach(() => vi.useFakeTimers());
  afterEach(() => vi.useRealTimers());

  it('agrupa varias escrituras en una sola', () => {
    const escribir = vi.fn();
    const volcado = crearVolcadoDiferido<number>(escribir, 700);

    volcado.programar(1);
    volcado.programar(2);
    volcado.programar(3);

    expect(escribir).not.toHaveBeenCalled();
    vi.advanceTimersByTime(700);
    expect(escribir).toHaveBeenCalledTimes(1);
    expect(escribir).toHaveBeenCalledWith(3);
  });

  it('volcar() escribe lo pendiente sin esperar', () => {
    const escribir = vi.fn();
    const volcado = crearVolcadoDiferido<string>(escribir, 700);
    volcado.programar('hola');

    volcado.volcar();

    expect(escribir).toHaveBeenCalledWith('hola');
    vi.advanceTimersByTime(700);
    expect(escribir).toHaveBeenCalledTimes(1);
  });

  it('volcar() sin nada pendiente no escribe', () => {
    const escribir = vi.fn();
    const volcado = crearVolcadoDiferido<string>(escribir, 700);
    volcado.volcar();
    expect(escribir).not.toHaveBeenCalled();
  });

  it('cancelar() descarta el valor pendiente', () => {
    const escribir = vi.fn();
    const volcado = crearVolcadoDiferido<number>(escribir, 700);
    volcado.programar(9);
    volcado.cancelar();

    expect(volcado.pendiente()).toBe(false);
    vi.advanceTimersByTime(700);
    expect(escribir).not.toHaveBeenCalled();
  });

  it('tras volcar, el reloj no vuelve a escribir', () => {
    const escribir = vi.fn();
    const volcado = crearVolcadoDiferido<number>(escribir, 700);
    volcado.programar(1);
    vi.advanceTimersByTime(700);
    vi.advanceTimersByTime(700);
    expect(escribir).toHaveBeenCalledTimes(1);
  });
});
