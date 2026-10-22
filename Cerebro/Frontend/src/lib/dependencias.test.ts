// Regresion: un useCallback con deps vacias captura el valor del PRIMER
// render para siempre. Paso de verdad: `comenzarDesdeInicio` se memorizaba
// con [] y el boton "Empezar a hablar" del inicio no hacia nada, porque
// `personajes` arranca vacio y el callback se quedaba con esa lista vacia
// (el `if (!kira) return` cortaba en silencio, sin error ni log).
//
// Este test lee el codigo fuente y verifica que los callbacks que tocan
// estado que cambia declaren sus dependencias.
import { describe, expect, it } from 'vitest';
import app from '../App.tsx?raw';

/** Extrae el cuerpo balanceado del useCallback que empieza en `desde`. */
function cuerpoCallback(src: string, desde: number): string {
  let prof = 0;
  for (let i = desde; i < src.length; i++) {
    if (src[i] === '(') prof++;
    else if (src[i] === ')') {
      prof--;
      if (prof === 0) return src.slice(desde, i + 1);
    }
  }
  return src.slice(desde);
}

function callbacksConDepsVacias(src: string): { nombre: string; cuerpo: string }[] {
  const salida: { nombre: string; cuerpo: string }[] = [];
  const re = /const (\w+) = useCallback\(/g;
  let m: RegExpExecArray | null;
  while ((m = re.exec(src))) {
    const cuerpo = cuerpoCallback(src, m.index + m[0].length - 1);
    if (!/\},\s*\[\s*\]\s*\)\s*;?\s*$/.test(cuerpo.trim())) continue;
    salida.push({ nombre: m[1], cuerpo });
  }
  return salida;
}

// Estado que existe desde el primer render pero cambia despues (respuestas
// de la API, interaccion del usuario). Congelarlo es el bug.
const ESTADO_QUE_CAMBIA = [
  'personajes',
  'mensajes',
  'chats',
  'elegido',
  'input',
  'error',
  'escuchando',
  'escribiendo',
  'streamPendiente',
  'ajustesLocales',
  'foto',
  'sidebarColapsada',
];

describe('dependencias de useCallback en App', () => {
  it('ningun callback con deps [] toca estado que cambia', () => {
    const culpables: string[] = [];
    for (const { nombre, cuerpo } of callbacksConDepsVacias(app)) {
      const tocados = ESTADO_QUE_CAMBIA.filter((v) =>
        new RegExp(`\\b${v}\\b`).test(cuerpo)
      );
      if (tocados.length) culpables.push(`${nombre} (usa ${tocados.join(', ')})`);
    }
    expect(
      culpables,
      `Estos callbacks se memorizan con deps [] pero leen estado que cambia:` +
        `\n  ${culpables.join('\n  ')}` +
        `\nAgregá las dependencias o usá un ref.`
    ).toEqual([]);
  });

  it('comenzarDesdeInicio depende de personajes', () => {
    // El caso concreto que rompio el boton de empezar a hablar.
    const m = /const comenzarDesdeInicio = useCallback\(/.exec(app);
    expect(m, 'no encuentro comenzarDesdeInicio').toBeTruthy();
    const cuerpo = cuerpoCallback(app, m!.index + m![0].length - 1);
    const deps = /\},\s*\[([^\]]*)\]\s*\)/.exec(cuerpo)?.[1] ?? '';
    expect(deps).toContain('personajes');
  });
});
