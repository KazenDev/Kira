import { describe, expect, it } from 'vitest';
import { ANIM_HERRAMIENTA, ANIMS_PENSANDO, caraQuieta, crearMotor, cuadro, type EntradaCara } from './caraMotor';
import { bocaDeLetra } from './vivo';
import { colaSubtitulo } from './ModoCara';

const base: EntradaCara = {
  estado: 'reposo', emocion: 'happy', herramienta: null, boca: 0, nivel: 0, mirarX: null, reducido: false,
};

/** Corre el motor `seg` segundos a 30 fps y devuelve todos los cuadros. */
function correr(ent: Partial<EntradaCara>, seg = 3, semilla = 7) {
  const m = crearMotor(semilla);
  const cuadros: number[][] = [];
  for (let i = 0; i <= seg * 30; i++) cuadros.push(Array.from(cuadro(m, { ...base, ...ent }, i / 30)));
  return cuadros;
}

const encendidos = (g: number[]) => g.filter((v) => v > 0.3).length;

describe('caraMotor', () => {
  it('todos los brillos quedan en [0, 1] en todos los estados y animaciones', () => {
    const casos: Partial<EntradaCara>[] = [
      { estado: 'reposo' }, { estado: 'habla', boca: 1 }, { estado: 'escucha', nivel: 1 },
      { estado: 'piensa' }, { risa: true },
      ...Object.keys(ANIM_HERRAMIENTA).map((h) => ({ estado: 'piensa' as const, herramienta: h })),
    ];
    for (const c of casos)
      for (const g of correr(c, 2))
        for (const v of g) {
          expect(v).toBeGreaterThanOrEqual(0);
          expect(v).toBeLessThanOrEqual(1);
        }
  });

  it('cada emocion tiene una cara quieta distinta y con algo prendido', () => {
    const emociones = ['happy', 'sad', 'neutral', 'surprised', 'angry', 'fastidio', 'miedo', 'cansado'];
    const firmas = new Set(emociones.map((e) => Array.from(caraQuieta(e)).map((v) => (v > 0.3 ? 1 : 0)).join('')));
    expect(firmas.size).toBe(emociones.length);
    for (const e of emociones) expect(encendidos(Array.from(caraQuieta(e)))).toBeGreaterThan(2);
  });

  it('una emocion desconocida no rompe: cae en neutral', () => {
    expect(Array.from(caraQuieta('zzz'))).toEqual(Array.from(caraQuieta('neutral')));
  });

  it('parpadea: en 10 s los ojos se apagan al menos una vez', () => {
    const cuadros = correr({ estado: 'reposo' }, 10);
    // ojos de happy: fila 1, columnas 1 y 3 (brillo sumado por la mirada en subpixel)
    const ojos = cuadros.map((g) => g[5] + g[6] + g[7] + g[8] + g[9]);
    expect(Math.min(...ojos.slice(30))).toBeLessThan(0.5);
    expect(Math.max(...ojos)).toBeGreaterThan(0.8);
  });

  it('con movimiento reducido no parpadea ni mueve la mirada', () => {
    const cuadros = correr({ estado: 'reposo', reducido: true }, 10).slice(20);
    const ojos = cuadros.map((g) => g[6] + g[8]);
    expect(Math.min(...ojos)).toBeGreaterThan(1.5);
  });

  it('hablando, la boca abierta prende la fila de abajo; cerrada no', () => {
    const abierta = correr({ estado: 'habla', emocion: 'neutral', boca: 1 }, 1).at(-1)!;
    const cerrada = correr({ estado: 'habla', emocion: 'neutral', boca: 0 }, 1).at(-1)!;
    const filaAbajo = (g: number[]) => g[21] + g[22] + g[23];
    expect(filaAbajo(abierta)).toBeGreaterThan(1.5);
    expect(filaAbajo(cerrada)).toBeLessThan(0.2);
  });

  it('pensando, la animacion se MUEVE (no es una imagen quieta)', () => {
    for (const h of [null, ...Object.keys(ANIM_HERRAMIENTA)]) {
      const cuadros = correr({ estado: 'piensa', herramienta: h }, 3);
      const firmas = new Set(cuadros.slice(10).map((g) => g.map((v) => (v > 0.4 ? 1 : 0)).join('')));
      expect(firmas.size, `animacion de ${h ?? 'pensar'}`).toBeGreaterThan(2);
    }
  });

  it('al volver a pensar elige otra animacion (no repite la anterior)', () => {
    const m = crearMotor(3);
    const elegidas: string[] = [];
    let t = 0;
    for (let i = 0; i < 12; i++) {
      cuadro(m, { ...base, estado: 'piensa' }, (t += 0.1));
      elegidas.push(m.anim);
      cuadro(m, { ...base, estado: 'reposo' }, (t += 0.1));
    }
    for (let i = 1; i < elegidas.length; i++) expect(elegidas[i]).not.toBe(elegidas[i - 1]);
    expect(elegidas.every((a) => ANIMS_PENSANDO.includes(a))).toBe(true);
  });

  it('escuchando, el borde late con el nivel del microfono', () => {
    const borde = (g: number[]) => g[10] + g[14] + g[15] + g[19] + g[20] + g[24];
    const fuerte = correr({ estado: 'escucha', nivel: 1 }, 2).slice(10).map(borde);
    const silencio = correr({ estado: 'escucha', nivel: 0 }, 2).slice(10).map(borde);
    expect(Math.max(...fuerte)).toBeGreaterThan(Math.max(...silencio) + 1);
  });
});

describe('bocaDeLetra', () => {
  it('vocal abre, consonante a medias, pausa cierra', () => {
    expect(bocaDeLetra('a')).toBe(1);
    expect(bocaDeLetra('m')).toBeCloseTo(0.4);
    expect(bocaDeLetra(' ')).toBeLessThan(0.1);
    expect(bocaDeLetra(undefined)).toBe(0);
    expect(bocaDeLetra('i')).toBeLessThan(bocaDeLetra('a'));
  });
});

describe('colaSubtitulo', () => {
  it('texto corto: entero', () => {
    expect(colaSubtitulo('Hola, ¿cómo estás?')).toBe('Hola, ¿cómo estás?');
  });
  it('texto largo: solo el final, cortado en una palabra y con "…"', () => {
    const largo = 'palabra '.repeat(60).trim();
    const sub = colaSubtitulo(largo, 150);
    expect(sub.startsWith('…palabra')).toBe(true);
    expect(sub.length).toBeLessThanOrEqual(151);
    expect(largo.endsWith(sub.slice(1))).toBe(true);
  });
});
