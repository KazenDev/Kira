// La CARA de Kira: su matriz 5x5 de LEDs rojos, viva, en un canvas.
// Es logo, avatar e indicador de estado a la vez (reposo, escucha, piensa,
// habla). El motor puro esta en caraMotor.ts; aca solo se dibuja.
//
// Rendimiento: el brillo de cada LED es un sprite de glow pre-renderizado UNA
// vez por tamaño (drawImage con alpha, sin shadowBlur por cuadro), se dibuja
// a 30 fps como mucho y el bucle se DETIENE cuando la cara no se ve (fuera de
// pantalla o pestaña oculta). Las caras quietas (mensajes viejos) se pintan
// una sola vez y no tienen bucle.
import { memo, useEffect, useRef } from 'react';
import { caraQuieta, crearMotor, cuadro, type Rejilla } from './caraMotor';
import { kiraVivo, type EstadoCara } from './vivo';

interface Props {
  size: number;
  /** true: lee el estado en vivo de Kira (kiraVivo) en cada cuadro */
  vivo?: boolean;
  /** si no es "vivo": la emocion de esta cara (quieta) */
  emocion?: string;
  /** forzar un estado (ej. el indicador de "pensando") */
  estado?: EstadoCara;
  /** los ojos siguen al puntero/dedo */
  mirarPuntero?: boolean;
  /** sin la placa de fondo (solo LEDs) */
  sinFondo?: boolean;
  className?: string;
  etiqueta?: string;
}

const LED = [255, 59, 47];
const reducido = () =>
  typeof window !== 'undefined' &&
  window.matchMedia?.('(prefers-reduced-motion: reduce)').matches === true;

/** El halo de un LED encendido, pintado una vez por tamaño. */
const spritesGlow = new Map<number, HTMLCanvasElement>();
function spriteGlow(radio: number): HTMLCanvasElement {
  const clave = Math.round(radio);
  const ya = spritesGlow.get(clave);
  if (ya) return ya;
  const c = document.createElement('canvas');
  c.width = c.height = clave * 2;
  const ctx = c.getContext('2d')!;
  const gr = ctx.createRadialGradient(clave, clave, 0, clave, clave, clave);
  gr.addColorStop(0, `rgba(${LED[0]},${LED[1] + 40},${LED[2] + 20},0.55)`);
  gr.addColorStop(0.35, `rgba(${LED[0]},${LED[1]},${LED[2]},0.22)`);
  gr.addColorStop(1, 'rgba(255,40,30,0)');
  ctx.fillStyle = gr;
  ctx.fillRect(0, 0, clave * 2, clave * 2);
  spritesGlow.set(clave, c);
  return c;
}

function rectRedondo(ctx: CanvasRenderingContext2D, x: number, y: number, w: number, h: number, r: number) {
  ctx.beginPath();
  ctx.moveTo(x + r, y);
  ctx.arcTo(x + w, y, x + w, y + h, r);
  ctx.arcTo(x + w, y + h, x, y + h, r);
  ctx.arcTo(x, y + h, x, y, r);
  ctx.arcTo(x, y, x + w, y, r);
  ctx.closePath();
}

function pintar(ctx: CanvasRenderingContext2D, px: number, g: Rejilla, fondo: boolean) {
  ctx.clearRect(0, 0, px, px);
  if (fondo) {
    const gr = ctx.createLinearGradient(0, 0, 0, px);
    gr.addColorStop(0, '#24130f');
    gr.addColorStop(1, '#140a08');
    ctx.fillStyle = gr;
    rectRedondo(ctx, 0, 0, px, px, px * 0.26);
    ctx.fill();
  }
  const pad = px * (fondo ? 0.17 : 0.06);
  const celda = (px - pad * 2) / 5;
  const lado = celda * 0.6;
  const radio = lado * 0.24;
  const glow = spriteGlow(celda * 1.15);
  // LEDs apagados (se ven, como en la placa real)
  ctx.fillStyle = 'rgba(255,90,70,0.075)';
  for (let i = 0; i < 25; i++) {
    const cx = pad + (i % 5) * celda + celda / 2;
    const cy = pad + Math.floor(i / 5) * celda + celda / 2;
    rectRedondo(ctx, cx - lado / 2, cy - lado / 2, lado, lado, radio);
    ctx.fill();
  }
  // halos (sumados: dos LEDs vecinos brillan mas juntos)
  ctx.globalCompositeOperation = 'lighter';
  for (let i = 0; i < 25; i++) {
    const v = g[i];
    if (v < 0.03) continue;
    const cx = pad + (i % 5) * celda + celda / 2;
    const cy = pad + Math.floor(i / 5) * celda + celda / 2;
    ctx.globalAlpha = Math.min(1, v * 0.95);
    ctx.drawImage(glow, cx - glow.width / 2, cy - glow.height / 2);
  }
  ctx.globalAlpha = 1;
  ctx.globalCompositeOperation = 'source-over';
  // nucleos: rojo que se aclara cuando brilla fuerte
  for (let i = 0; i < 25; i++) {
    const v = g[i];
    if (v < 0.03) continue;
    const cx = pad + (i % 5) * celda + celda / 2;
    const cy = pad + Math.floor(i / 5) * celda + celda / 2;
    const k = v * v;
    ctx.fillStyle = `rgba(${LED[0]},${Math.round(LED[1] + 110 * k)},${Math.round(LED[2] + 90 * k)},${Math.min(1, 0.15 + v)})`;
    rectRedondo(ctx, cx - lado / 2, cy - lado / 2, lado, lado, radio);
    ctx.fill();
  }
}

function KiraCaraInner({
  size,
  vivo = false,
  emocion = 'neutral',
  estado,
  mirarPuntero = false,
  sinFondo = false,
  className,
  etiqueta = 'Kira',
}: Props) {
  const canvasRef = useRef<HTMLCanvasElement | null>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;
    const dpr = Math.min(2, window.devicePixelRatio || 1);
    const px = Math.round(size * dpr);
    canvas.width = px;
    canvas.height = px;
    const ctx = canvas.getContext('2d');
    if (!ctx) return;

    const animada = vivo || estado !== undefined;
    if (!animada) {
      pintar(ctx, px, caraQuieta(emocion), !sinFondo);
      return;
    }

    const motor = crearMotor(Math.floor(Math.random() * 1e9));
    let mirarX: number | null = null;
    const alMover = (e: PointerEvent) => {
      const r = canvas.getBoundingClientRect();
      const cx = r.left + r.width / 2;
      mirarX = Math.max(-1, Math.min(1, (e.clientX - cx) / (window.innerWidth * 0.4)));
    };
    if (mirarPuntero) window.addEventListener('pointermove', alMover, { passive: true });

    let visible = true;
    let raf = 0;
    let ultimo = 0;
    const t0 = performance.now();
    const paso = (ahora: number) => {
      raf = 0;
      if (!visible || document.hidden) return;
      raf = requestAnimationFrame(paso);
      if (ahora - ultimo < 33) return; // 30 fps alcanzan para LEDs
      ultimo = ahora;
      const g = cuadro(
        motor,
        {
          estado: estado ?? kiraVivo.estado,
          emocion: vivo ? kiraVivo.emocion : emocion,
          herramienta: vivo || estado === 'piensa' ? kiraVivo.herramienta : null,
          boca: kiraVivo.boca,
          nivel: kiraVivo.nivel,
          mirarX,
          reducido: reducido(),
          risa: vivo && kiraVivo.risaHasta > ahora,
        },
        (ahora - t0) / 1000
      );
      pintar(ctx, px, g, !sinFondo);
    };
    const arrancar = () => {
      if (!raf && visible && !document.hidden) raf = requestAnimationFrame(paso);
    };
    const io = new IntersectionObserver((entradas) => {
      visible = entradas.some((e) => e.isIntersecting);
      arrancar();
    });
    io.observe(canvas);
    document.addEventListener('visibilitychange', arrancar);
    arrancar();
    return () => {
      if (raf) cancelAnimationFrame(raf);
      io.disconnect();
      document.removeEventListener('visibilitychange', arrancar);
      if (mirarPuntero) window.removeEventListener('pointermove', alMover);
    };
  }, [size, vivo, emocion, estado, mirarPuntero, sinFondo]);

  return (
    <canvas
      ref={canvasRef}
      className={'kira-cara' + (className ? ' ' + className : '')}
      style={{ width: size, height: size }}
      role="img"
      aria-label={etiqueta}
    />
  );
}

const KiraCara = memo(KiraCaraInner);
export default KiraCara;
