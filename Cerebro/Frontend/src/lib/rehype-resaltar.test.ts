import { describe, expect, it } from 'vitest';
import rehypeResaltar from './rehype-resaltar';

type Nodo = {
  type: string;
  tagName?: string;
  value?: string;
  properties?: Record<string, unknown>;
  children?: Nodo[];
};

function arbol(lenguaje: string, codigo: string): Nodo {
  return {
    type: 'root',
    children: [
      {
        type: 'element',
        tagName: 'pre',
        children: [
          {
            type: 'element',
            tagName: 'code',
            properties: { className: [`language-${lenguaje}`] },
            children: [{ type: 'text', value: codigo }],
          },
        ],
      },
    ],
  };
}

function resaltar(lenguaje: string, codigo: string): Nodo {
  const tree = arbol(lenguaje, codigo);
  // el plugin es sincrónico y muta el árbol
  (rehypeResaltar() as (t: unknown) => void)(tree);
  return tree.children![0].children![0];
}

describe('rehypeResaltar', () => {
  it('resalta JavaScript y marca el bloque con hljs', () => {
    const code = resaltar('javascript', 'const x = 1;');
    expect(code.properties?.className).toContain('hljs');
    expect((code.children ?? []).length).toBeGreaterThan(0);
    expect((code.children ?? [])[0].type).toBe('element');
  });

  it('respeta los alias (js, tsx, html, yml, sh)', () => {
    for (const [alias, codigo] of [
      ['js', 'const x = 1;'],
      ['tsx', 'const x: number = 1;'],
      ['html', '<div>hola</div>'],
      ['yml', 'clave: valor'],
      ['sh', 'echo hola'],
    ] as const) {
      const code = resaltar(alias, codigo);
      expect(code.properties?.className).toContain('hljs');
    }
  });

  it('deja intacto un lenguaje desconocido', () => {
    const code = resaltar('cobol', 'DISPLAY "hola".');
    expect(code.properties?.className).toEqual(['language-cobol']);
    expect(code.children).toEqual([{ type: 'text', value: 'DISPLAY "hola".' }]);
  });

  it('no toca bloques sin lenguaje (no adivina)', () => {
    const tree: Nodo = {
      type: 'root',
      children: [
        {
          type: 'element',
          tagName: 'pre',
          children: [
            {
              type: 'element',
              tagName: 'code',
              properties: {},
              children: [{ type: 'text', value: 'texto' }],
            },
          ],
        },
      ],
    };
    (rehypeResaltar() as (t: unknown) => void)(tree);
    expect(tree.children![0].children![0].children).toEqual([{ type: 'text', value: 'texto' }]);
  });

  it('respeta no-highlight', () => {
    const tree: Nodo = {
      type: 'root',
      children: [
        {
          type: 'element',
          tagName: 'pre',
          children: [
            {
              type: 'element',
              tagName: 'code',
              properties: { className: ['language-js', 'no-highlight'] },
              children: [{ type: 'text', value: 'const x = 1;' }],
            },
          ],
        },
      ],
    };
    (rehypeResaltar() as (t: unknown) => void)(tree);
    expect(tree.children![0].children![0].children).toEqual([
      { type: 'text', value: 'const x = 1;' },
    ]);
  });
});
