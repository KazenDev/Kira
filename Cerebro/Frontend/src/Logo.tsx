// Marca de Kira: su propia cara de LEDs (la de la micro:bit), quieta y feliz.
import KiraCara from './KiraCara';

export default function Logo({ size = 40 }: { size?: number }) {
  return <KiraCara size={size} emocion="happy" etiqueta="Kira" />;
}
