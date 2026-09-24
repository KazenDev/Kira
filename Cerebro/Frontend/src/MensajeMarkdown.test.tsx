/**
 * MensajeMarkdown.test.tsx — smoke test del pipeline de markdown:
 * react-markdown + rehype-highlight + la cabecera del bloque (lenguaje/copiar).
 * Correr: npm run test
 */
import { describe, expect, it } from 'vitest';
import { renderToStaticMarkup } from 'react-dom/server';
import MensajeMarkdown from './MensajeMarkdown';

const md = (contenido: string) => renderToStaticMarkup(<MensajeMarkdown contenido={contenido} />);

describe('MensajeMarkdown (bloques de código estilo ChatGPT)', () => {
  it('renderiza markdown básico (negritas, listas)', () => {
    const html = md('**hola** mundo\n\n- uno\n- dos');
    expect(html).toContain('<strong>hola</strong>');
    expect(html).toContain('<li>uno</li>');
  });

  it('el bloque de código lleva cabecera con lenguaje + botón copiar + hljs activo', () => {
    const html = md('```python\nprint("hola")\n```');
    expect(html).toContain('codigo-barra');
    expect(html).toContain('python');
    expect(html).toContain('copiar');
    expect(html).toContain('hljs'); // resaltado de sintaxis aplicado
    expect(html).toContain('print');
  });

  it('fence SIN lenguaje: cabecera dice "texto" y el contenido no se pierde', () => {
    const html = md('```\nsolo texto\n```');
    expect(html).toContain('codigo-idioma');
    expect(html).toContain('texto');
    expect(html).toContain('solo texto');
  });

  it('jsx/tsx (aliases registrados) se resaltan sin romper', () => {
    const html = md('```jsx\nconst a = <div>;\n```');
    expect(html).toContain('jsx');
    expect(html).toContain('hljs');
  });

  it('lenguaje desconocido: no revienta, el texto queda plano', () => {
    const html = md('```zzz-no-existe\nabc = 1\n```');
    expect(html).toContain('abc = 1');
    expect(html).toContain('zzz-no-existe');
  });

  it('el código inline sigue siendo inline (sin cabecera)', () => {
    const html = md('usa `const` acá');
    expect(html).toContain('<code>const</code>');
    expect(html).not.toContain('codigo-barra');
  });

  it('tablas y links GFM siguen funcionando (tabla envuelta en contenedor scrolleable)', () => {
    const html = md('| a |\n| - |\n| 1 |\n\n[link](https://example.com)');
    expect(html).toContain('<table>');
    expect(html).toContain('md-tabla');
    expect(html).toContain('href="https://example.com"');
    expect(html).toContain('target="_blank"');
  });
});

describe('bloque de código: números de línea, colapso y diff', () => {
  it('números de línea a partir de 4 líneas, no en bloques cortos', () => {
    const corto = md('```js\nconst a = 1;\n```');
    expect(corto).toContain('md-linea');
    expect(corto).not.toContain('con-numeros');

    const largo =
      '```js\n' +
      [1, 2, 3, 4].map((i) => `const x${i} = ${i};`).join('\n') +
      '\n```';
    const html = md(largo);
    expect(html).toContain('con-numeros');
    expect((html.match(/md-linea/g) ?? []).length).toBe(4);
  });

  it('la cabecera muestra el nombre legible del lenguaje y el conteo de líneas', () => {
    const html = md('```js\nconst a = 1;\nconst b = 2;\n```');
    expect(html).toContain('JavaScript');
    expect(html).toContain('2 líneas');
    expect(html).toContain('role="region"');
  });

  it('bloque de más de 40 líneas nace colapsado con «Mostrar más»', () => {
    const lineas = Array.from({ length: 45 }, (_, i) => `const v${i} = ${i};`).join('\n');
    const html = md('```js\n' + lineas + '\n```');
    expect(html).toContain('codigo-mas');
    expect(html).toContain('Mostrar más');
    expect(html).toContain('45 líneas');

    const corto = md('```js\nconst a = 1;\n```');
    expect(corto).not.toContain('codigo-mas');
  });

  it('```diff pinta las líneas agregadas y quitadas', () => {
    const html = md('```diff\n+const nuevo = 1;\n-const viejo = 2;\n```');
    expect(html).toContain('hljs-addition');
    expect(html).toContain('hljs-deletion');
  });

  it('las líneas vacías del código no rompen el render', () => {
    const html = md('```js\nconst a = 1;\n\nconst b = 2;\n```');
    expect((html.match(/md-linea/g) ?? []).length).toBe(3);
  });
});
