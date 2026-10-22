// Plugin de resaltado propio: usa highlight.js SOLO con los lenguajes que
// Kira escribe. `rehype-highlight` importa el paquete `common` (37 gramáticas)
// y eso inflaba el chunk de markdown ~3x.
import { createLowlight } from 'lowlight';
import bash from 'highlight.js/lib/languages/bash';
import css from 'highlight.js/lib/languages/css';
import diff from 'highlight.js/lib/languages/diff';
import javascript from 'highlight.js/lib/languages/javascript';
import json from 'highlight.js/lib/languages/json';
import python from 'highlight.js/lib/languages/python';
import sql from 'highlight.js/lib/languages/sql';
import typescript from 'highlight.js/lib/languages/typescript';
import xml from 'highlight.js/lib/languages/xml';
import yaml from 'highlight.js/lib/languages/yaml';

const lowlight = createLowlight({
  bash,
  css,
  diff,
  javascript,
  json,
  python,
  sql,
  typescript,
  xml,
  yaml,
});

// alias -> lenguaje registrado (los mismos que usa la cabecera del bloque)
const ALIAS: Record<string, string> = {
  js: 'javascript',
  jsx: 'javascript',
  ts: 'typescript',
  tsx: 'typescript',
  sh: 'bash',
  shell: 'bash',
  zsh: 'bash',
  html: 'xml',
  htm: 'xml',
  yml: 'yaml',
};

type Nodo = {
  type?: string;
  tagName?: string;
  value?: string;
  properties?: Record<string, unknown>;
  children?: Nodo[];
  data?: unknown;
};

function textoDe(nodo: Nodo): string {
  if (nodo.type === 'text') return nodo.value ?? '';
  return (nodo.children ?? []).map(textoDe).join('');
}

function recorrer(nodo: Nodo, padre: Nodo | null, visita: (n: Nodo, p: Nodo | null) => void) {
  visita(nodo, padre);
  for (const hijo of nodo.children ?? []) recorrer(hijo, nodo, visita);
}

/** Rehype plugin: resalta los <pre><code class="language-x"> del árbol. */
export default function rehypeResaltar() {
  return (tree: unknown) => {
    recorrer(tree as Nodo, null, (nodo, padre) => {
      if (nodo.tagName !== 'code' || padre?.tagName !== 'pre') return;
      const clases = Array.isArray(nodo.properties?.className)
        ? (nodo.properties.className as unknown[]).map(String)
        : [];
      if (clases.includes('no-highlight') || clases.includes('nohighlight')) return;

      const conLenguaje =
        clases.find((c) => c.startsWith('language-')) ?? clases.find((c) => c.startsWith('lang-'));
      if (!conLenguaje) return; // sin lenguaje explícito no adivinamos
      const crudo = conLenguaje.replace(/^(language-|lang-)/, '').toLowerCase();
      const lenguaje = ALIAS[crudo] ?? crudo;
      if (!lowlight.registered(lenguaje)) return; // desconocido: queda texto plano

      const resultado = lowlight.highlight(lenguaje, textoDe(nodo), { prefix: 'hljs-' });
      if (resultado.children.length > 0) {
        nodo.children = resultado.children as Nodo[];
        nodo.properties = {
          ...nodo.properties,
          className: ['hljs', ...clases],
        };
      }
    });
  };
}
