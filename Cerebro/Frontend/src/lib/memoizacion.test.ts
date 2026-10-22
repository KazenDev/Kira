import { describe, expect, it } from 'vitest';
import app from '../App.tsx?raw';
import burbuja from '../componentes/Burbuja.tsx?raw';
import composer from '../componentes/Composer.tsx?raw';
import chatHeader from '../componentes/ChatHeader.tsx?raw';
import listaMensajes from '../componentes/ListaMensajes.tsx?raw';
import vistaChat from '../componentes/VistaChat.tsx?raw';
import panelSidebar from '../componentes/PanelSidebar.tsx?raw';
import settings from '../componentes/Settings.tsx?raw';
import memoriaPanel from '../componentes/MemoriaPanel.tsx?raw';
import useChats from '../hooks/useChats.ts?raw';

const MEMOIZADOS: [string, string][] = [
  ['Burbuja', burbuja],
  ['Composer', composer],
  ['ChatHeader', chatHeader],
  ['ListaMensajes', listaMensajes],
  ['VistaChat', vistaChat],
  ['PanelSidebar', panelSidebar],
  ['Settings', settings],
  ['MemoriaPanel', memoriaPanel],
];

/**
 * Contratos de memoización: no prueban comportamiento de usuario, protegen
 * las optimizaciones de render para que un refactor futuro no las borre sin
 * que nadie lo note.
 */
describe('memoización de la vista', () => {
  it('los componentes que reciben props del streaming están memoizados', () => {
    for (const [nombre, codigo] of MEMOIZADOS) {
      expect(codigo, `${nombre} debería usar memo()`).toMatch(/memo\(/);
      expect(codigo, `${nombre} debería exportar por defecto`).toMatch(/export default/);
    }
  });

  it('VistaChat memoiza el JSX de acciones del header', () => {
    expect(vistaChat).toMatch(/const acciones = useMemo\(/);
    expect(vistaChat).toMatch(/acciones=\{acciones\}/);
  });

  it('App no pasa closures inline a los componentes', () => {
    // props tipo: onAlgo={() => ...}  o  onAlgo={(x) => ...}
    const inline = app.match(/\son[A-Z]\w+=\{\s*\(/g) ?? [];
    expect(inline).toEqual([]);
  });

  it('Burbuja calcula sus audioUrls (el padre pasa un handler estable)', () => {
    expect(burbuja).toMatch(/onEscuchar\?: \(urls: string\[\]\) => void/);
    expect(burbuja).toMatch(/const audioUrls = m\.audio_urls/);
  });

  it('useChats escribe diferido, no en cada cambio', () => {
    expect(useChats).toMatch(/crearVolcadoDiferido/);
    expect(useChats).toMatch(/visibilitychange/);
  });
});
