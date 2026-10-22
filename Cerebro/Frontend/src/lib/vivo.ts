// El ESTADO EN VIVO de Kira, compartido por todas sus caras en pantalla.
// No es estado de React a proposito: las caras lo leen en cada cuadro de
// animacion (60 veces por segundo) y re-renderizar la app a ese ritmo para
// mover una boca seria un desperdicio. App.tsx lo escribe; KiraCara lo lee.

export type EstadoCara = 'reposo' | 'escucha' | 'piensa' | 'habla';

export const kiraVivo = {
  estado: 'reposo' as EstadoCara,
  emocion: 'neutral',
  herramienta: null as string | null, // la tool que esta usando (para su animacion)
  boca: 0, // 0 cerrada .. 1 abierta: la letra que esta sonando AHORA
  nivel: 0, // 0..1: cuanto ruido escucha el microfono
  risaHasta: 0, // performance.now() hasta el que se rie (le hicieron cosquillas en la pantalla)
};

/** Cosquillas en la pantalla: todas las caras se rien un ratito. */
export function hacerCosquillas(ms = 1300) {
  kiraVivo.risaHasta = performance.now() + ms;
}

const VOCALES = 'aeiouáéíóúAEIOUÁÉÍÓÚ';

/** Cuanto se abre la boca para una letra: vocal grande, consonante a medias,
 *  pausa o puntuacion cerrada. Es lo que se ve "hablar" sincronizado con el
 *  texto que va diciendo (el texto ya viaja al ritmo de la voz). */
export function bocaDeLetra(c: string | undefined): number {
  if (!c) return 0;
  if (VOCALES.includes(c)) return c === 'i' || c === 'í' || c === 'u' || c === 'ú' ? 0.65 : 1;
  if (/[a-zñ]/i.test(c)) return 0.4;
  return 0.05;
}
