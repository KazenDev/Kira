import { Mic2, Palette, Sparkles, Volume2, VolumeX } from 'lucide-react';
import { Interruptor } from './Interruptor';

type Props = {
  sonido: boolean;
  onCambiarSonido: (v: boolean) => void;
  voz: boolean;
  onCambiarVoz: (v: boolean) => void;
  autonomia: boolean;
  onCambiarAutonomia: (v: boolean) => void;
  fondoVivo: boolean;
  onCambiarFondoVivo: (v: boolean) => void;
  volumenVoz: number;
  onCambiarVolumenVoz: (v: number) => void;
};

export function SeccionSonido({
  sonido,
  onCambiarSonido,
  voz,
  onCambiarVoz,
  autonomia,
  onCambiarAutonomia,
  fondoVivo,
  onCambiarFondoVivo,
  volumenVoz,
  onCambiarVolumenVoz,
}: Props) {
  return (
    <section className="ajustes-seccion">
      <div className="ajustes-titulo">
        <Volume2 size={16} strokeWidth={2.2} aria-hidden="true" />
        <h3>Sonido</h3>
      </div>
      <Interruptor
        activo={sonido}
        onClick={() => onCambiarSonido(!sonido)}
        etiqueta="Sonidos de la app"
        descripcion={
          sonido
            ? 'Pop suave al enviar, recibir y al cambiar de vista'
            : 'Todo silencioso (ideal para la feria)'
        }
        icono={
          sonido ? (
            <Volume2 size={18} aria-hidden="true" />
          ) : (
            <VolumeX size={18} aria-hidden="true" />
          )
        }
      />
      <Interruptor
        activo={voz}
        onClick={() => onCambiarVoz(!voz)}
        etiqueta="Voz de Kira"
        descripcion={
          voz
            ? 'Reproducir la voz y sincronizar la boca de la micro:bit'
            : 'No reproducir audio; la placa queda muda'
        }
        icono={<Mic2 size={18} aria-hidden="true" />}
      />
      <Interruptor
        activo={autonomia}
        onClick={() => onCambiarAutonomia(!autonomia)}
        etiqueta="Kira habla sola"
        descripcion={
          autonomia
            ? 'Reacciona en voz alta cuando la tocan, la sacuden o termina un juego, y saca charla si nadie le habla'
            : 'Solo habla cuando le escribís'
        }
        icono={<Sparkles size={18} aria-hidden="true" />}
      />
      <Interruptor
        activo={fondoVivo}
        onClick={() => onCambiarFondoVivo(!fondoVivo)}
        etiqueta="Fondo vivo"
        descripcion={
          fondoVivo
            ? 'Un resplandor que cambia con lo que siente Kira y late cuando habla'
            : 'Fondo quieto (gasta menos batería)'
        }
        icono={<Palette size={18} aria-hidden="true" />}
      />
      <div className={'ajustes-slider' + (voz ? '' : ' deshabilitado')}>
        <div className="ajustes-slider-cabecera">
          <label htmlFor="volumen-voz">Volumen de la voz</label>
          <output htmlFor="volumen-voz">{Math.round(volumenVoz * 100)}%</output>
        </div>
        <input
          id="volumen-voz"
          type="range"
          min="0"
          max="1"
          step="0.05"
          value={volumenVoz}
          disabled={!voz}
          onChange={(e) => onCambiarVolumenVoz(Number(e.target.value))}
          aria-label="Volumen de la voz de Kira"
        />
      </div>
    </section>
  );
}
