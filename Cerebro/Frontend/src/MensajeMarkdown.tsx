// Render de markdown SEGURO (sin dangerouslySetInnerHTML -> sin XSS).
// react-markdown convierte la markdown en elementos React reales y escapa
// el HTML crudo; remark-gfm agrega tablas, listas de tareas y tachado;
// rehype-highlight colorea la SINTAXIS de los bloques de código (hljs).
//
// Bloque de código estilo ChatGPT/Claude (patrones vía EXA):
//   - cabecera: nombre legible del lenguaje + conteo de líneas + copiar
//   - números de línea (gutter) a partir de 4 líneas — se logra partiendo
//     los <span> de hljs por línea y numerándolos con un counter de CSS,
//     así se preservan tokens multilínea (comentarios de bloque, etc.)
//   - bloques de más de 40 líneas se colapsan con "Mostrar más";
//     copiar SIEMPRE lleva el código completo (lee el texto crudo, no el DOM)
//   - ```diff pinta líneas + / - (hljs-addition / hljs-deletion)
//   - a11y: role="region" con "Código {lenguaje}, {n} líneas"
// El estado del botón vive EN el bloque, así el memo de arriba no se entera.
// El estilo de código inline vs bloque se resuelve 100% con CSS:
//   .md code       -> inline
//   .md pre > code -> bloque

import { memo, isValidElement, useState } from 'react';
import type { ReactNode } from 'react';
import ReactMarkdown from 'react-markdown';
import type { Components } from 'react-markdown';
import remarkGfm from 'remark-gfm';
import rehypeHighlight from 'rehype-highlight';
import { Check, ChevronDown, Copy } from 'lucide-react';

interface Props {
  contenido: string;
}

/** más de estas líneas => el bloque arranca colapsado (copiar sigue completo) */
const LIMITE_LINEAS = 40;
/** números de línea a partir de esta cantidad (patrón Claude Code) */
const MIN_NUMEROS = 4;

// nombres legibles para la etiqueta de la cabecera (js -> JavaScript, ...)
const NOMBRES: Record<string, string> = {
  js: 'JavaScript',
  javascript: 'JavaScript',
  jsx: 'React JSX',
  ts: 'TypeScript',
  typescript: 'TypeScript',
  tsx: 'React TSX',
  py: 'Python',
  python: 'Python',
  sh: 'Shell',
  bash: 'Shell',
  shell: 'Shell',
  zsh: 'Shell',
  json: 'JSON',
  jsonc: 'JSON',
  html: 'HTML',
  htm: 'HTML',
  css: 'CSS',
  scss: 'SCSS',
  sass: 'Sass',
  less: 'Less',
  sql: 'SQL',
  yml: 'YAML',
  yaml: 'YAML',
  md: 'Markdown',
  markdown: 'Markdown',
  diff: 'Diff',
  patch: 'Diff',
  xml: 'XML',
  svg: 'SVG',
  java: 'Java',
  c: 'C',
  cpp: 'C++',
  'c++': 'C++',
  csharp: 'C#',
  cs: 'C#',
  go: 'Go',
  rust: 'Rust',
  rb: 'Ruby',
  ruby: 'Ruby',
  php: 'PHP',
  kt: 'Kotlin',
  kotlin: 'Kotlin',
  swift: 'Swift',
  ini: 'INI',
  toml: 'TOML',
  dockerfile: 'Dockerfile',
  makefile: 'Makefile',
  vue: 'Vue',
  dart: 'Dart',
  lua: 'Lua',
  r: 'R',
  http: 'HTTP',
  txt: 'texto',
};

// texto PLANO de un subárbol React: para copiar cruza los <span> de hljs
// sin arrastrar la cabecera del bloque.
function textoPlano(nodo: ReactNode): string {
  if (nodo === null || nodo === undefined || typeof nodo === 'boolean') return '';
  if (typeof nodo === 'string' || typeof nodo === 'number') return String(nodo);
  if (Array.isArray(nodo)) return nodo.map(textoPlano).join('');
  if (isValidElement(nodo)) {
    return textoPlano((nodo.props as { children?: ReactNode }).children);
  }
  return '';
}

// el lenguaje vive en el className "language-xxx" que remark pone en <code>
function idiomaDe(children: ReactNode): string {
  const lista = Array.isArray(children) ? children : [children];
  for (const hijo of lista) {
    if (!isValidElement(hijo)) continue;
    const clase = (hijo.props as { className?: string }).className ?? '';
    const m = /language-([\w+-]+)/.exec(clase);
    if (m) return m[1];
  }
  return '';
}

// className COMPLETO del <code> (language-python hljs) para re-renderizarlo tal cual
function claseDe(children: ReactNode): string {
  const lista = Array.isArray(children) ? children : [children];
  for (const hijo of lista) {
    if (isValidElement(hijo)) {
      return (hijo.props as { className?: string }).className ?? '';
    }
  }
  return '';
}

interface Parte {
  clase: string;
  texto: string;
}

// aplana los <span> de hljs en partes plano {clase, texto}, acumulando las
// clases de los spans anidados (hljs mete spans dentro de spans)
function partesDe(nodo: ReactNode, claseHeredada = ''): Parte[] {
  if (nodo === null || nodo === undefined || typeof nodo === 'boolean') return [];
  if (typeof nodo === 'string' || typeof nodo === 'number') {
    const texto = String(nodo);
    return texto ? [{ clase: claseHeredada, texto }] : [];
  }
  if (Array.isArray(nodo)) {
    const out: Parte[] = [];
    for (const hijo of nodo) out.push(...partesDe(hijo, claseHeredada));
    return out;
  }
  if (isValidElement(nodo)) {
    const propia = (nodo.props as { className?: string }).className ?? '';
    const clase = propia ? (claseHeredada ? `${claseHeredada} ${propia}` : propia) : claseHeredada;
    return partesDe((nodo.props as { children?: ReactNode }).children, clase);
  }
  return [];
}

// parte las hljs-pieces en líneas: los \n separan líneas; el \n final del
// fence no debe sumar una línea vacía de más (se descarta la última vacía)
function lineasDe(nodo: ReactNode): Parte[][] {
  const lineas: Parte[][] = [[]];
  for (const parte of partesDe(nodo)) {
    const trozos = parte.texto.split('\n');
    trozos.forEach((trozo, i) => {
      if (i > 0) lineas.push([]);
      if (trozo) lineas[lineas.length - 1].push({ clase: parte.clase, texto: trozo });
    });
  }
  if (lineas.length > 1 && lineas[lineas.length - 1].length === 0) lineas.pop();
  return lineas;
}

function nombreIdioma(idioma: string): string {
  if (!idioma) return 'texto';
  return NOMBRES[idioma.toLowerCase()] ?? idioma;
}

// bloque de código con cabecera: lenguaje + líneas + copiar; cuerpo con
// números de línea y colapso si es largo
function BloqueCodigo({ children }: { children?: ReactNode }) {
  const [copiado, setCopiado] = useState(false);
  const [expandido, setExpandido] = useState(false);

  const idioma = idiomaDe(children);
  const nombre = nombreIdioma(idioma);
  // texto crudo ANTES de trocear: el copiado da siempre el código completo
  const plano = textoPlano(children).replace(/\n$/, '');
  const lineas = lineasDe(children);
  const n = lineas.length;
  const largo = n > LIMITE_LINEAS;

  const copiar = async () => {
    try {
      await navigator.clipboard.writeText(plano);
      setCopiado(true);
      window.setTimeout(() => setCopiado(false), 1600);
    } catch {
      // clipboard no disponible (http:// sin permisos): queda sin copiar
    }
  };

  return (
    <div
      className={`codigo${n >= MIN_NUMEROS ? ' con-numeros' : ''}`}
      role="region"
      aria-label={`Código ${nombre}, ${n} ${n === 1 ? 'línea' : 'líneas'}`}
    >
      <div className="codigo-barra">
        <span className="codigo-idioma">{nombre}</span>
        <span className="codigo-acc">
          <span className="codigo-lineas" aria-hidden="true">
            {n} {n === 1 ? 'línea' : 'líneas'}
          </span>
          <button
            type="button"
            className="codigo-copiar"
            onClick={copiar}
            title="Copiar código"
            aria-label="Copiar código"
          >
            {copiado ? (
              <Check size={13} strokeWidth={2.2} aria-hidden="true" />
            ) : (
              <Copy size={13} strokeWidth={2.2} aria-hidden="true" />
            )}
            {copiado ? 'copiado' : 'copiar'}
          </button>
        </span>
      </div>
      <div className={`codigo-cuerpo${largo && !expandido ? ' recortado' : ''}`}>
        <pre>
          <code className={claseDe(children)}>
            {lineas.map((toks, i) => (
              <span className="md-linea" key={i}>
                {toks.map((t, j) =>
                  t.clase ? (
                    <span className={t.clase} key={j}>
                      {t.texto}
                    </span>
                  ) : (
                    t.texto
                  ),
                )}
              </span>
            ))}
          </code>
        </pre>
      </div>
      {largo && (
        <button
          type="button"
          className="codigo-mas"
          aria-expanded={expandido}
          onClick={() => setExpandido((v) => !v)}
        >
          {expandido ? 'Mostrar menos' : `Mostrar más · ${n} líneas`}
          <ChevronDown size={13} strokeWidth={2.2} aria-hidden="true" />
        </button>
      )}
    </div>
  );
}

const componentes: Components = {
  // links seguros: abren en pestaña nueva
  a: ({ href, children }) => (
    <a href={href} target="_blank" rel="noopener noreferrer">
      {children}
    </a>
  ),
  // el <pre> de react-markdown se convierte en el bloque con su cabecera
  pre: ({ children }) => <BloqueCodigo>{children}</BloqueCodigo>,
  // tabla envuelta en contenedor scrolleable (no rompe el layout en el celu)
  table: ({ children }) => (
    <div className="md-tabla">
      <table>{children}</table>
    </div>
  ),
};

const MensajeMarkdown = memo(function MensajeMarkdown({ contenido }: Props) {
  return (
    <div className="md">
      <ReactMarkdown
        remarkPlugins={[remarkGfm]}
        rehypePlugins={[
          [rehypeHighlight, { aliases: { javascript: ['jsx'], typescript: ['tsx'] } }],
        ]}
        components={componentes}
      >
        {contenido}
      </ReactMarkdown>
    </div>
  );
});

export default MensajeMarkdown;
