// Avatar de Kira: su cara de LEDs. `emocion` pinta la cara de ese momento.
import KiraCara from './KiraCara';

export default function Avatar({ size = 32, emocion = 'happy' }: { color?: string; size?: number; emocion?: string }) {
  return <KiraCara size={size} emocion={emocion} />;
}
