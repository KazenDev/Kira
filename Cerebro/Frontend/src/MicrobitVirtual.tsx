// Réplica REALISTA del micro:bit v2 en la web.
// - Placa PCB oscura con esquinas redondeadas y biselado sutil
// - Hueco (recess) donde van los LEDs, como el dispositivo físico
// - LEDs CUADRADOS con esquinas redondeadas (no círculos) y glow en capas
//   (núcleo + halo + destello especular), como los diodos reales
// - Conector dorado abajo con los pines de borde
// - Si llega `patronReal` (25 chars del micro:bit REAL) pinta EXACTAMENTE lo
//   que el micro:bit muestra ahora mismo; sino, la cara según la emoción.

const CARAS: Record<string, string[]> = {
  happy: [
    '. . . . .',
    '# . . . #',
    '. . . . .',
    '# # # # #',
    '. . . . .',
  ],
  sad: [
    '. . . . .',
    '# . . . #',
    '. . . . .',
    '. # # # .',
    '# . . . #',
  ],
  angry: [
    '. # . # .',
    '# . . . #',
    '. . . . .',
    '. # # # .',
    '# # # # #',
  ],
  surprised: [
    '# . . . #',
    '# . . . #',
    '. . . . .',
    '. # # # .',
    '. . . . .',
  ],
  neutral: [
    '. . . . .',
    '# # . # #',
    '. . . . .',
    '. # # # .',
    '. . . . .',
  ],
  fastidio: [
    '# # . # #',
    '# . . # .',
    '. . . . .',
    '. # # # .',
    '. . . . .',
  ],
  miedo: [
    '. # . # .',
    '# . . . #',
    '. # # # .',
    '. # . # .',
    '. # # # .',
  ],
  cansado: [
    '. . . . .',
    '# . . . #',
    '# . . . #',
    '. . . . .',
    '. # # # .',
  ],
  talk: [
    '. . . . .',
    '# . . . #',
    '. . . . .',
    '# # # # #',
    '. . . . .',
  ],
};

interface Props {
  emotion: string;
  color: string;
  patronReal?: string | null; // 25 chars: replica del micro:bit real
}

export default function MicrobitVirtual({ emotion, color, patronReal }: Props) {
  // si tenemos la replica real, usamos el patron que viene del micro:bit
  if (patronReal && patronReal.length >= 25) {
    return (
      <div
        className="microbit-board replica"
        style={{ '--color': color } as React.CSSProperties}
        title="Réplica en vivo del micro:bit"
      >
        <div className="microbit-matriz">
          {patronReal.slice(0, 25).split('').map((c, i) => (
            <span
              key={i}
              className={'led' + (c === '#' ? ' led-on' : '')}
              style={{ '--i': i } as React.CSSProperties}
            />
          ))}
        </div>
        <div className="microbit-conector" aria-hidden="true" />
      </div>
    );
  }

  const patron = CARAS[emotion] ?? CARAS.neutral;
  const esLoading = emotion === 'loading';
  return (
    <div
      className="microbit-board"
      style={{ '--color': color } as React.CSSProperties}
      title={`Emoción: ${emotion}`}
    >
      <div className="microbit-matriz">
        {patron.map((fila, y) =>
          fila.split(' ').map((celda, x) => (
            <span
              key={`${y}-${x}`}
              className={
                'led' + (esLoading ? ' led-loading' : celda === '#' ? ' led-on' : '')
              }
              style={{ '--i': y * 5 + x } as React.CSSProperties}
            />
          ))
        )}
      </div>
      <div className="microbit-conector" aria-hidden="true" />
    </div>
  );
}
