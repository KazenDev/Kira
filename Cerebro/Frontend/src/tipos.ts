// Tipos y helpers compartidos entre los componentes del chat.
import { MensajeChat, PersonajeInfo, horaActual } from './api';
import { uuid } from './ui-utils';

export interface ChatSesion {
  id: string;
  titulo: string;
  fecha: string;
  mensajes: MensajeChat[];
}

// quita el markdown para el preview del sidebar (sin mutilar la puntuacion humana)
export function limpiarMarkdown(texto: string): string {
  return texto
    .replace(/[#*_`>~|]/g, '')
    .replace(/\s+/g, ' ')
    .trim();
}

// Intl.Segmenter puede no estar en los types del TS viejo: lo declaramos
// para poder usarlo (el runtime lo chequea con 'in' antes).
declare class IntlSegmenter {
  segment(texto: string): { containing(index: number): { segment: string } | undefined };
}

// Extrae el EMOJI del inicio del titulo (si lo hay), de forma ROBUSTA:
// - Intl.Segmenter agrupa el primer grapheme (maneja emojis multi-codepoint:
//   familias con ZWJ 👨\u200d👩\u200d👧, banderas 🇺🇳, tonos de piel)
// - Regex \p{RGI_Emoji} con flag /v confirma que ES un emoji (no una letra)
// - try/catch: si el navegador es viejo, fallback simple (primer codepoint)
const segmentador: IntlSegmenter | null =
  typeof Intl !== 'undefined' && 'Segmenter' in Intl
    ? new (Intl as unknown as {
        Segmenter: new (locales?: string | string[], opciones?: { granularity?: string }) => IntlSegmenter;
      }).Segmenter(undefined, { granularity: 'grapheme' })
    : null;

let regexEmoji: RegExp | null = null;
try {
  regexEmoji = new RegExp('^\\p{RGI_Emoji}$', 'v');
} catch {
  regexEmoji = null;
}

export function extraerEmoji(titulo: string): string | null {
  if (!titulo) return null;
  let primero = titulo.trim().charAt(0);
  if (segmentador) {
    const it = segmentador.segment(titulo.trim());
    primero = it.containing(0)?.segment ?? primero;
  }
  if (!primero) return null;
  if (regexEmoji && regexEmoji.test(primero)) return primero;
  // fallback sin regex (navegador viejo): solo rangos Unicode de emoji conocidos
  if (!regexEmoji && /[\u{1F000}-\u{1FAFF}\u{2600}-\u{27BF}\u{2B00}-\u{2BFF}\u{FE0F}]/u.test(primero)) return primero;
  return null;
}

// Devuelve el titulo SIN el emoji (para mostrarlo limpio al lado del emoji)
export function tituloSinEmoji(titulo: string): string {
  const emoji = extraerEmoji(titulo);
  if (!emoji) return titulo;
  return titulo.trim().slice(emoji.length).trim();
}

export function horaLista(fechaIso: string): string {
  const d = new Date(fechaIso);
  const hoy = new Date().toDateString() === d.toDateString();
  return hoy
    ? d.toLocaleTimeString('es-AR', { hour: '2-digit', minute: '2-digit' })
    : d.toLocaleDateString('es-AR', { day: '2-digit', month: '2-digit' });
}

export function nuevoSaludo(pj: PersonajeInfo): MensajeChat {
  return {
    id: uuid(),
    rol: pj.id as 'kira',
    contenido: pj.saludo,
    hora: horaActual(),
  };
}
