/**
 * ui-utils.test.ts — tests de la lógica pura de la interfaz.
 * Correr: npm run test
 */
import { afterEach, describe, expect, it, vi } from 'vitest';
import {
  dimensionesEscaladas,
  estadoMicrobit,
  estaAlFondo,
  formatearFechaCorta,
  indicadorEscribiendo,
  labelHerramienta,
  resaltarPartes,
  SUGERENCIAS_INICIO,
  textoMicrobit,
  uuid,
} from './ui-utils';

// UUID v4: 8-4-4-4-12 con version 4 y variante 8/9/a/b
const RE_UUID = /^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/;

describe('dimensionesEscaladas (la foto del celular antes de subirla)', () => {
  it('foto apaisada 4032x3024 -> 1024x768 (no se deforma)', () => {
    expect(dimensionesEscaladas(4032, 3024, 1024)).toEqual({ ancho: 1024, alto: 768 });
  });

  it('foto vertical 3024x4032 -> 768x1024', () => {
    expect(dimensionesEscaladas(3024, 4032, 1024)).toEqual({ ancho: 768, alto: 1024 });
  });

  it('foto cuadrada 4000x4000 -> 1024x1024', () => {
    expect(dimensionesEscaladas(4000, 4000, 1024)).toEqual({ ancho: 1024, alto: 1024 });
  });

  it('nunca AGRANDA una foto que ya es chica', () => {
    expect(dimensionesEscaladas(640, 480, 1024)).toEqual({ ancho: 640, alto: 480 });
    expect(dimensionesEscaladas(1024, 100, 1024)).toEqual({ ancho: 1024, alto: 100 });
  });

  it('datos raros (0, NaN, negativos) no rompen el canvas', () => {
    expect(dimensionesEscaladas(0, 0, 1024)).toEqual({ ancho: 1, alto: 1 });
    expect(dimensionesEscaladas(NaN, NaN, 1024)).toEqual({ ancho: 1, alto: 1 });
    const raro = dimensionesEscaladas(-100, -50, 1024);
    expect(raro.ancho).toBeGreaterThan(0);
    expect(raro.alto).toBeGreaterThan(0);
  });
});

describe('labelHerramienta (tool trace: estado NOMBRADO en vez de puntos)', () => {
  it('tools del micro:bit y del cerebro -> frase en español con puntos suspensivos', () => {
    expect(labelHerramienta('leer_temperatura')).toBe('leyendo el termómetro…');
    expect(labelHerramienta('leer_sonido')).toBe('escuchando el sonido…');
    expect(labelHerramienta('leer_bateria')).toBe('midiendo la batería…');
    expect(labelHerramienta('buscar_en_web')).toBe('buscando en la web…');
    expect(labelHerramienta('calcular')).toBe('haciendo la cuenta…');
    expect(labelHerramienta('guardar_recuerdo')).toBe('anotándolo en su memoria…');
    expect(labelHerramienta('leer_url')).toBe('leyendo esa página…');
  });

  it('tool desconocida (futura) -> fallback genérico, nunca undefined', () => {
    expect(labelHerramienta('tool_inventada')).toBe('usando una herramienta…');
  });

  it('sin tool -> null (el indicador cae en "pensando…")', () => {
    expect(labelHerramienta(null)).toBeNull();
    expect(labelHerramienta(undefined)).toBeNull();
    expect(labelHerramienta('')).toBeNull();
  });
});

describe('indicador del micro:bit (el "placa por BLE" en rojo)', () => {
  it('la placa contesta -> ok (verde)', () => {
    expect(estadoMicrobit({ respondiendo: true, conectado: true })).toBe('ok');
    expect(textoMicrobit({ respondiendo: true, conectado: true })).toBe('micro:bit en línea');
  });

  it('enlace sin respuesta todavia -> con (ambar)', () => {
    expect(estadoMicrobit({ respondiendo: false, conectado: true })).toBe('con');
    expect(textoMicrobit({ respondiendo: false, conectado: true })).toBe('conectado...');
  });

  it('el CELUAR es el cable (ble_relay) -> relay, NUNCA mal/rojo', () => {
    expect(estadoMicrobit({ respondiendo: false, conectado: false, ble_relay: true })).toBe('relay');
    expect(textoMicrobit({ respondiendo: false, conectado: false, ble_relay: true })).toBe('placa por BLE');
  });

  it('de verdad no hay placa -> mal (rojo)', () => {
    expect(estadoMicrobit({ respondiendo: false, conectado: false, ble_relay: false })).toBe('mal');
    expect(textoMicrobit({ respondiendo: false, conectado: false, ble_relay: false })).toBe('sin micro:bit');
  });

  it('sin datos del server no revienta', () => {
    expect(estadoMicrobit(undefined)).toBe('mal');
    expect(estadoMicrobit(null)).toBe('mal');
    expect(textoMicrobit(undefined)).toBe('sin micro:bit');
  });
});

describe('uuid (el crash de http:// sin contexto seguro)', () => {
  afterEach(() => {
    vi.unstubAllGlobals();
  });

  it('devuelve un v4 valido (camino normal, con randomUUID)', () => {
    expect(uuid()).toMatch(RE_UUID);
  });

  it('no repite ids (200 seguidos)', () => {
    const ids = new Set(Array.from({ length: 200 }, () => uuid()));
    expect(ids.size).toBe(200);
  });

  it('funciona con getRandomValues cuando randomUUID NO existe (http:// en una IP)', () => {
    vi.stubGlobal('crypto', {
      getRandomValues: (arr: Uint8Array) => {
        for (let i = 0; i < arr.length; i++) arr[i] = Math.floor(Math.random() * 256);
        return arr;
      },
    });
    expect(uuid()).toMatch(RE_UUID);
  });

  it('ultimo recurso: sin Web Crypto igual devuelve un id usable', () => {
    vi.stubGlobal('crypto', undefined);
    expect(uuid()).toMatch(RE_UUID);
  });
});

describe('estaAlFondo (scroll inteligente)', () => {
  it('al fondo exacto', () => {
    expect(estaAlFondo(1000, 800, 200)).toBe(true);
  });

  it('dentro del umbral de 90px', () => {
    expect(estaAlFondo(1000, 720, 200)).toBe(true); // 80px del fondo
  });

  it('fuera del umbral (leyendo historia)', () => {
    expect(estaAlFondo(1000, 600, 200)).toBe(false); // 200px del fondo
  });

  it('contenido que no scrollea (corto)', () => {
    expect(estaAlFondo(300, 0, 500)).toBe(true);
  });
});

describe('SUGERENCIAS_INICIO', () => {
  it('3 sugerencias no vacías', () => {
    expect(SUGERENCIAS_INICIO.length).toBe(3);
    expect(SUGERENCIAS_INICIO.every((s) => s.trim().length > 3)).toBe(true);
  });

  it('toca las 3 superpotencias (charla, micro:bit, sensores)', () => {
    const juntas = SUGERENCIAS_INICIO.join(' ').toLowerCase();
    expect(juntas).toMatch(/hola|quién|quien/);
    expect(juntas).toMatch(/metrónomo|metronomo/);
    expect(juntas).toMatch(/temperatura/);
  });
});

// El parcial nace VACÍO al mandar el mensaje. Antes se dibujaba igual (burbuja
// con el cursor "|" + pie "escribiendo…") Y ADEMÁS los puntitos: dos
// indicadores en pantalla. Regla: NUNCA los dos juntos.
describe('indicadorEscribiendo (el bug de los dos "escribiendo…")', () => {
  const msg = (id: string, contenido: string, rol = 'kira') => ({ id, rol, contenido });

  it('parcial VACIO -> puntitos (y la burbuja vacia se oculta: un solo indicador)', () => {
    expect(indicadorEscribiendo(true, 'p1', [msg('p1', '')], 'kira')).toBe('puntos');
  });

  it('apenas llega texto -> burbuja con cursor (los puntitos se apagan)', () => {
    expect(indicadorEscribiendo(true, 'p1', [msg('p1', 'Hola')], 'kira')).toBe('burbuja');
  });

  it('espacios en blanco NO cuentan como texto', () => {
    expect(indicadorEscribiendo(true, 'p1', [msg('p1', '   \n  ')], 'kira')).toBe('puntos');
  });

  it('sin respuesta en curso -> ningun indicador', () => {
    expect(indicadorEscribiendo(false, 'p1', [msg('p1', 'Hola')], 'kira')).toBeNull();
    expect(indicadorEscribiendo(true, null, [msg('p1', 'Hola')], 'kira')).toBeNull();
  });

  it('regenerar: el parcial en medio de la lista sigue dando UN solo indicador', () => {
    const lista = [msg('p1', ''), msg('u1', 'hola', 'usuario'), msg('p9', 'vieja')];
    expect(indicadorEscribiendo(true, 'p1', lista, 'kira')).toBe('puntos');
  });

  it('parcial que no esta en la lista -> nada (nada de indicadores fantasma)', () => {
    expect(indicadorEscribiendo(true, 'zz', [msg('p1', 'Hola')], 'kira')).toBeNull();
  });

  it('el parcial de OTRO rol no cuenta', () => {
    expect(indicadorEscribiendo(true, 'p1', [msg('p1', '', 'usuario')], 'kira')).toBeNull();
  });

  it('REGLA DE ORO: en ningun caso se encienden los dos a la vez', () => {
    const casos = [[msg('p1', '')], [msg('p1', 'Hola')], [msg('p1', '   ')], []];
    for (const lista of casos) {
      const ind = indicadorEscribiendo(true, 'p1', lista, 'kira');
      const burbujaVisible = ind === 'burbuja'; // en App: el parcial se dibuja salvo en 'puntos'
      const puntitos = ind === 'puntos';
      expect(Number(burbujaVisible) + Number(puntitos)).toBeLessThanOrEqual(1);
    }
  });
});

describe('formatearFechaCorta', () => {
  it('formatea ISO a "7 sep, 13:34"', () => {
    const f = formatearFechaCorta('2026-09-07T13:34:48');
    expect(f).toMatch(/7 sep/);
    expect(f).toMatch(/13:34/);
  });

  it('ISO inválido se devuelve tal cual (no rompe)', () => {
    expect(formatearFechaCorta('no-fecha')).toBe('no-fecha');
  });
});

describe('resaltarPartes (resaltado del buscador del sidebar)', () => {
  it('sin búsqueda -> un solo fragmento, sin marcar', () => {
    expect(resaltarPartes('Hola Kira', '')).toEqual([{ t: 'Hola Kira', coincide: false }]);
  });

  it('marca todas las coincidencias y el orden reconstruye el texto exacto', () => {
    const texto = 'Kira conversa, Nico contesta y kira vuelve';
    const partes = resaltarPartes(texto, 'kira');
    expect(partes.filter((p) => p.coincide)).toHaveLength(2);
    expect(partes.map((p) => p.t).join('')).toBe(texto);
  });

  it('es case-insensitive (la búsqueda del sidebar filtra en minúsculas)', () => {
    const partes = resaltarPartes('Temperatura y METRÓNOMO', 'metrónomo');
    expect(partes).toHaveLength(2);
    expect(partes[0].coincide).toBe(false);
    expect(partes[1]).toEqual({ t: 'METRÓNOMO', coincide: true });
  });

  it('coincidencia al inicio: no pierde letras del resto', () => {
    const texto = 'Kira y Nico al final';
    const partes = resaltarPartes(texto, 'ki');
    expect(partes[0]).toEqual({ t: 'Ki', coincide: true });
    expect(partes.map((p) => p.t).join('')).toBe(texto);
  });

  it('búsqueda con caracteres de regex es texto literal (no revienta)', () => {
    const texto = 'calculo (2+2) = 4 y a.b';
    for (const q of ['(2+2)', 'a.b', '$$', '[x']) {
      const partes = resaltarPartes(texto, q);
      expect(partes.map((p) => p.t).join('')).toBe(texto);
    }
    expect(resaltarPartes(texto, '(2+2)').some((p) => p.coincide)).toBe(true);
  });

  it('texto vacío o solo espacios no genera fragmentos vacíos raros', () => {
    expect(resaltarPartes('', 'kira')).toEqual([{ t: '', coincide: false }]);
    expect(resaltarPartes('   ', '  ')).toEqual([{ t: '   ', coincide: false }]);
  });
});
