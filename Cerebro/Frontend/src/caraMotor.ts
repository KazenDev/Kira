// Motor de la CARA DE KIRA en la web: la misma matriz 5x5 de LEDs rojos de
// su micro:bit, pero viva. Puro (sin DOM): recibe el estado y el tiempo y
// devuelve el brillo de cada LED (0..1). KiraCara.tsx lo dibuja en canvas.
//
// Trucos (los mismos que el motor de vida de la placa, ver MICROBIT.md):
//   - subpixel: un ojo en x=1.4 reparte su brillo entre el LED 1 y el 2, asi
//     la mirada se desliza en vez de saltar de a un LED
//   - persistencia: los LEDs suben rapido y se apagan lento (fosforo)
//   - parpadeos y miradas con tiempos al azar: nunca dos veces igual

import type { EstadoCara } from './vivo';

export type Rejilla = Float32Array; // 25 brillos, fila por fila

export interface EntradaCara {
  estado: EstadoCara;
  emocion: string;
  herramienta: string | null;
  boca: number; // 0..1 (hablando)
  nivel: number; // 0..1 (escuchando)
  mirarX: number | null; // -1..1 si sigue al puntero
  reducido: boolean; // prefers-reduced-motion
}

export interface Motor {
  mostrado: Rejilla;
  objetivo: Rejilla;
  ultimoT: number;
  proximoParpadeo: number;
  parpadeoHasta: number;
  mirada: number;
  miradaObj: number;
  proximaMirada: number;
  boca: number;
  anim: string;
  animDesde: number;
  estadoPrevio: EstadoCara;
  azar: () => number;
}

// ---------------------------------------------------------------- utilidades

function semillaAzar(semilla: number): () => number {
  // mulberry32: chico, rapido y reproducible (los tests usan semilla fija)
  let a = semilla >>> 0;
  return () => {
    a = (a + 0x6d2b79f5) >>> 0;
    let t = a;
    t = Math.imul(t ^ (t >>> 15), t | 1);
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61);
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296;
  };
}

export function crearMotor(semilla = Date.now()): Motor {
  return {
    mostrado: new Float32Array(25),
    objetivo: new Float32Array(25),
    ultimoT: 0,
    proximoParpadeo: 1.5,
    parpadeoHasta: 0,
    mirada: 0,
    miradaObj: 0,
    proximaMirada: 2,
    boca: 0,
    anim: 'orbita',
    animDesde: 0,
    estadoPrevio: 'reposo',
    azar: semillaAzar(semilla),
  };
}

function pon(g: Rejilla, x: number, y: number, v: number) {
  if (x < 0 || x > 4 || y < 0 || y > 4) return;
  const i = y * 5 + x;
  if (v > g[i]) g[i] = Math.min(1, v);
}

/** Un punto en x fraccionaria: el brillo se reparte entre los dos LEDs. */
function ponSub(g: Rejilla, x: number, y: number, v: number) {
  const x0 = Math.floor(x);
  const f = x - x0;
  pon(g, x0, y, v * (1 - f));
  pon(g, x0 + 1, y, v * f);
}

function dibujo(g: Rejilla, filas: string[], v = 1, dx = 0, dy = 0) {
  for (let y = 0; y < filas.length; y++) {
    for (let x = 0; x < filas[y].length; x++) {
      if (filas[y][x] === '#') pon(g, x + dx, y + dy, v);
      else if (filas[y][x] === '+') pon(g, x + dx, y + dy, v * 0.45);
    }
  }
}

// El borde de la matriz en sentido horario (para las animaciones que giran)
const ANILLO: [number, number][] = [
  [0, 0], [1, 0], [2, 0], [3, 0], [4, 0], [4, 1], [4, 2], [4, 3],
  [4, 4], [3, 4], [2, 4], [1, 4], [0, 4], [0, 3], [0, 2], [0, 1],
];
// De afuera hacia adentro
const ESPIRAL: [number, number][] = [
  ...ANILLO, [1, 1], [2, 1], [3, 1], [3, 2], [3, 3], [2, 3], [1, 3], [1, 2], [2, 2],
];

// ---------------------------------------------------------------- la cara

/** Bocas por emocion (filas 2-4). */
const BOCAS: Record<string, string[]> = {
  happy: ['.....', '#...#', '.###.'],
  sad: ['.....', '.###.', '#...#'],
  neutral: ['.....', '.###.', '.....'],
  surprised: ['.....', '.###.', '.###.'],
  angry: ['.....', '.###.', '#...#'],
  fastidio: ['....+', '..##.', '.....'],
  miedo: ['.....', '.#.#.', '#.#.#'],
  cansado: ['.....', '.....', '.+++.'],
};

function cara(g: Rejilla, emocion: string, ojos: number, mirada: number, boca: number, hablando: boolean) {
  const e = BOCAS[emocion] ? emocion : 'neutral';
  // OJOS (con la mirada en subpixel)
  const yOjo = e === 'cansado' ? 2 : 1;
  const vOjo = (e === 'cansado' ? 0.45 : e === 'fastidio' ? 0.6 : 1) * ojos;
  ponSub(g, 1 + mirada, yOjo, vOjo);
  ponSub(g, 3 + mirada, yOjo, vOjo);
  if (e === 'surprised' || e === 'miedo') {
    // ojos grandes: dos filas
    ponSub(g, 1 + mirada, 0, vOjo * 0.8);
    ponSub(g, 3 + mirada, 0, vOjo * 0.8);
  }
  if (e === 'angry') {
    pon(g, 0, 0, 0.6);
    pon(g, 4, 0, 0.6);
  }
  if (e === 'fastidio') {
    pon(g, 0, 0, 0.35);
    pon(g, 1, 0, 0.35);
    pon(g, 3, 0, 0.35);
    pon(g, 4, 0, 0.35);
  }
  // BOCA: hablando se abre con la letra que suena; si no, la de la emocion
  if (hablando) {
    const abre = Math.max(0, Math.min(1, boca));
    for (let x = 1; x <= 3; x++) pon(g, x, 3, 0.55 + 0.45 * (1 - abre * 0.4));
    for (let x = 1; x <= 3; x++) pon(g, x, 4, abre * (x === 2 ? 0.55 : 1));
    if (e === 'happy') {
      pon(g, 0, 3, 0.4);
      pon(g, 4, 3, 0.4);
    }
    return;
  }
  dibujo(g, BOCAS[e], 1, 0, 2);
}

// ---------------------------------------------------------------- pensando

export const ANIMS_PENSANDO = ['orbita', 'puntos', 'escaneo', 'espiral', 'lluvia', 'pensativa', 'latido'];

/** Cada herramienta tiene SU animacion: se ve que esta haciendo. */
export const ANIM_HERRAMIENTA: Record<string, string> = {
  pronostico: 'sol',
  ver_clima: 'sol',
  calidad_aire: 'sol',
  leer_temperatura: 'sol',
  donde_queda: 'flecha',
  leer_brujula: 'flecha',
  tocar_melodia: 'nota',
  temporizador: 'reloj',
  wikipedia: 'lupa',
  un_dia_como_hoy: 'lupa',
  buscar_en_web: 'lupa',
  buscar_recuerdos: 'espiral',
  guardar_recuerdo: 'espiral',
  sismos_cercanos: 'sismo',
  estacion_espacial: 'iss',
  calcular: 'calculo',
  reloj: 'reloj',
  mostrar_en_leds: 'latido',
  clima_en: 'sol',
  sol_y_luna: 'sol',
  hora_en: 'reloj',
  dias_hasta: 'calculo',
  jugar_en_placa: 'latido',
  calibrar_brujula: 'flecha',
};

const FLECHAS = [
  ['..#..', '.###.', '#.#.#', '..#..', '..#..'],
  ['..###', '...##', '..#.#', '.#...', '#....'],
  ['..#..', '...#.', '#####', '...#.', '..#..'],
  ['#....', '.#...', '..#.#', '...##', '..###'],
  ['..#..', '..#..', '#.#.#', '.###.', '..#..'],
  ['....#', '...#.', '#.#..', '##...', '###..'],
  ['..#..', '.#...', '#####', '.#...', '..#..'],
  ['###..', '##...', '#.#..', '...#.', '....#'],
];

function animar(g: Rejilla, anim: string, t: number, m: Motor) {
  switch (anim) {
    case 'orbita': {
      const p = t * 11;
      for (let k = 0; k < 6; k++) {
        const [x, y] = ANILLO[(((Math.floor(p) - k) % 16) + 16) % 16];
        pon(g, x, y, 1 - k / 6);
      }
      pon(g, 2, 2, 0.25 + 0.2 * Math.sin(t * 4));
      break;
    }
    case 'puntos':
      for (let i = 0; i < 3; i++) pon(g, i + 1, 2, 0.2 + 0.8 * Math.max(0, Math.sin(t * 6 - i * 0.9)));
      break;
    case 'escaneo': {
      const fase = (t * 0.9) % 2;
      const x = fase < 1 ? fase * 4 : (2 - fase) * 4;
      for (let c = 0; c < 5; c++) {
        const v = Math.max(0, 1 - Math.abs(c - x) * 0.7);
        for (let y = 0; y < 5; y++) pon(g, c, y, v * (y === 2 ? 1 : 0.55));
      }
      break;
    }
    case 'espiral': {
      const n = Math.floor(t * 14) % 50;
      for (let i = 0; i < 25; i++) {
        const on = n < 25 ? i <= n : i > n - 25;
        if (on) pon(g, ESPIRAL[i][0], ESPIRAL[i][1], i === (n % 25) ? 1 : 0.6);
      }
      break;
    }
    case 'lluvia':
      for (let c = 0; c < 5; c++) {
        const vel = 4 + ((c * 7) % 3);
        const y = ((t * vel + c * 1.7) % 7) - 1;
        for (let k = 0; k < 3; k++) pon(g, c, Math.floor(y) - k, 1 - k * 0.38);
      }
      break;
    case 'pensativa': {
      // "mmm...": mira para arriba a un lado y al otro, boca de costado
      const mira = Math.sin(t * 1.6) * 0.6;
      ponSub(g, 1 + mira, 0, 1);
      ponSub(g, 3 + mira, 0, 1);
      pon(g, 2, 3, 0.8);
      pon(g, 3, 3, 0.8);
      const p = Math.floor(t * 3) % 4;
      if (p > 0) pon(g, 4, 4, 0.5);
      break;
    }
    case 'latido': {
      const r = 0.6 + 1.6 * Math.abs(Math.sin(t * 3));
      for (let y = 0; y < 5; y++)
        for (let x = 0; x < 5; x++) {
          const d = Math.abs(x - 2) + Math.abs(y - 2);
          pon(g, x, y, Math.max(0, 1 - Math.abs(d - r) * 0.9));
        }
      break;
    }
    case 'sol': {
      dibujo(g, ['.....', '.+#+.', '.###.', '.+#+.', '.....'], 1);
      const rayos = Math.floor(t * 2.5) % 2 === 0
        ? [[0, 0], [4, 0], [0, 4], [4, 4]]
        : [[2, 0], [0, 2], [4, 2], [2, 4]];
      for (const [x, y] of rayos) pon(g, x, y, 0.9);
      break;
    }
    case 'flecha':
      dibujo(g, FLECHAS[Math.floor(t * 5) % 8], 1);
      break;
    case 'nota': {
      const dy = Math.round(Math.sin(t * 7) * 0.6);
      dibujo(g, ['..###', '..#.#', '..#.#', '##.##', '##.##'], 1, 0, dy);
      break;
    }
    case 'reloj': {
      const q = (t % 2.4) / 2.4; // arena que cae
      dibujo(g, ['#####', '.#.#.', '..#..', '.#.#.', '#####'], 0.55);
      pon(g, 2, 1, 1 - q);
      pon(g, 1, 1, Math.max(0, 1 - q * 2));
      pon(g, 3, 1, Math.max(0, 1 - q * 2));
      pon(g, 2, 2, Math.floor(t * 8) % 2 ? 0.9 : 0.3);
      pon(g, 2, 3, q);
      pon(g, 1, 3, Math.max(0, q * 2 - 1));
      pon(g, 3, 3, Math.max(0, q * 2 - 1));
      break;
    }
    case 'lupa': {
      const dx = Math.round(Math.sin(t * 2.2) * 1);
      dibujo(g, ['.###.', '#...#', '#...#', '.###.', '....#'], 1, dx - 1, 0);
      pon(g, 1 + dx, 1, 0.3 + 0.3 * Math.sin(t * 9));
      break;
    }
    case 'sismo': {
      for (let x = 0; x < 5; x++) {
        const fase = t * 6 - x * 1.1;
        const amp = 0.6 + 1.4 * Math.max(0, Math.sin(t * 1.7 + x));
        const y = 2 + Math.round(Math.sin(fase) * amp);
        pon(g, x, Math.max(0, Math.min(4, y)), 1);
      }
      break;
    }
    case 'iss': {
      dibujo(g, ['.....', '..+..', '.+#+.', '..+..', '.....'], 0.8);
      const [x, y] = ANILLO[Math.floor(t * 8) % 16];
      pon(g, x, y, 1);
      break;
    }
    case 'calculo':
      dibujo(g, Math.floor(t * 2) % 2 ? ['.....', '#####', '.....', '#####', '.....'] : ['..#..', '..#..', '#####', '..#..', '..#..'], 1);
      break;
    default:
      animar(g, 'orbita', t, m);
  }
}

// ---------------------------------------------------------------- cuadro

/** UN cuadro: avanza el motor hasta el segundo `t` y devuelve lo que se ve. */
export function cuadro(m: Motor, ent: EntradaCara, t: number): Rejilla {
  const dt = Math.min(0.1, Math.max(0, t - m.ultimoT));
  m.ultimoT = t;
  const g = m.objetivo;
  g.fill(0);

  // al entrar a "piensa" se elige una animacion (distinta de la anterior)
  if (ent.estado === 'piensa' && m.estadoPrevio !== 'piensa') {
    let nueva = m.anim;
    for (let i = 0; i < 4 && nueva === m.anim; i++) {
      nueva = ANIMS_PENSANDO[Math.floor(m.azar() * ANIMS_PENSANDO.length)];
    }
    m.anim = nueva;
    m.animDesde = t;
  }
  m.estadoPrevio = ent.estado;

  // parpadeos (log-normal a ojo: casi siempre 2-5 s, a veces doble)
  if (t >= m.proximoParpadeo) {
    m.parpadeoHasta = t + 0.13;
    m.proximoParpadeo = t + (m.azar() < 0.15 ? 0.3 : 1.8 + m.azar() * 3.5);
  }
  const ojos = ent.reducido ? 1 : t < m.parpadeoHasta ? 0.08 : 1;

  // la mirada: al puntero si hay, si no saltitos al azar (sacadas)
  if (ent.mirarX !== null) m.miradaObj = Math.max(-0.7, Math.min(0.7, ent.mirarX * 0.8));
  else if (t >= m.proximaMirada) {
    const r = m.azar();
    m.miradaObj = r < 0.5 ? 0 : r < 0.75 ? -0.6 : 0.6;
    m.proximaMirada = t + 1.2 + m.azar() * 3;
  }
  m.mirada += (m.miradaObj - m.mirada) * Math.min(1, dt * 9);
  if (ent.reducido) m.mirada = 0;

  // la boca sigue a la letra con un resorte corto (sin temblequeo)
  m.boca += (ent.boca - m.boca) * Math.min(1, dt * 22);

  const animT = t - m.animDesde;
  switch (ent.estado) {
    case 'piensa': {
      const anim = (ent.herramienta && ANIM_HERRAMIENTA[ent.herramienta]) || m.anim;
      if (ent.reducido) {
        // sin movimiento: tres puntos que respiran suave
        const v = 0.45 + 0.35 * Math.sin(t * 2);
        pon(g, 1, 2, v);
        pon(g, 2, 2, v);
        pon(g, 3, 2, v);
      } else animar(g, anim, animT, m);
      break;
    }
    case 'escucha': {
      cara(g, 'surprised', ojos, m.mirada * 0.4, 0, false);
      // el borde late con el ruido REAL del microfono
      const n = Math.max(0, Math.min(1, ent.nivel));
      for (let i = 0; i < 16; i++) {
        const [x, y] = ANILLO[i];
        if (y === 0 || y === 1) continue; // no tapar los ojos
        const ola = 0.5 + 0.5 * Math.sin(t * 7 + i * 0.8);
        pon(g, x, y, n * (0.35 + 0.65 * ola));
      }
      break;
    }
    case 'habla':
      cara(g, ent.emocion, ojos, m.mirada * 0.5, m.boca, true);
      break;
    default:
      cara(g, ent.emocion, ojos, m.mirada, 0, false);
  }

  // persistencia de fosforo: sube rapido, baja lento
  const mo = m.mostrado;
  const sube = Math.min(1, dt * 26);
  const baja = Math.min(1, dt * (ent.estado === 'piensa' ? 7 : 11));
  for (let i = 0; i < 25; i++) {
    const d = g[i] - mo[i];
    mo[i] += d * (d > 0 ? sube : baja);
  }
  return mo;
}

/** La cara quieta de una emocion (avatares de mensajes viejos: sin motor). */
export function caraQuieta(emocion: string): Rejilla {
  const g = new Float32Array(25);
  cara(g, emocion, 1, 0, 0, false);
  return g;
}
