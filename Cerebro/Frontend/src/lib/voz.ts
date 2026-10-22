// Tipos y cálculo puro del tope de texto sincronizado con la voz.

export type AudioEnCola = { url: string; orden: number; texto?: string };

// [segundo, letras visibles ya dichas] de un clip (ver /api/tts-tiempos)
export type MarcasVoz = [number, number][];

// El clip que suena AHORA, para escribir el texto al ritmo de la voz: arranca
// en la letra `base` del mensaje y cubre `largo` letras.
export type ClipVoz = { base: number; largo: number; marcas: MarcasVoz | null };

// Letras por segundo de la voz de Kira (medido: 17-18): mientras el primer
// clip suena en streaming todavía no hay marcas, y se estima un pelín abajo.
export const LETRAS_POR_SEG = 16;

/** Hasta qué letra del mensaje puede avanzar el typewriter según el audio.
 *  `duracion` puede ser NaN/Infinity en streaming (duración desconocida). */
export function topeDeClip(
  clip: ClipVoz,
  tiempo: number,
  duracion: number
): number {
  let frac = 0;
  const marcas = clip.marcas;
  if (marcas && marcas.length > 1 && marcas[marcas.length - 1][1] > 0) {
    // interpolado entre marcas (una por palabra): va letra por letra
    let letras = marcas[marcas.length - 1][1];
    for (let i = 0; i < marcas.length - 1; i++) {
      if (tiempo < marcas[i + 1][0]) {
        const tramo = marcas[i + 1][0] - marcas[i][0];
        const k = tramo > 0 ? (tiempo - marcas[i][0]) / tramo : 1;
        letras = marcas[i][1] + (marcas[i + 1][1] - marcas[i][1]) * Math.max(0, k);
        break;
      }
    }
    frac = letras / marcas[marcas.length - 1][1];
  } else if (Number.isFinite(duracion) && duracion > 0) {
    frac = tiempo / duracion;
  } else if (clip.largo > 0) {
    frac = (tiempo * LETRAS_POR_SEG) / clip.largo; // streaming: duración desconocida
  }
  return clip.base + Math.round(Math.min(1, frac) * clip.largo);
}
