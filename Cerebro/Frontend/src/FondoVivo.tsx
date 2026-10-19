// FONDO VIVO: un resplandor suave detras de todo, tenido por la emocion de
// Kira y que late cuando habla. Es un fragment shader chiquito (ruido fbm con
// el dominio deformado, la tecnica clasica de Inigo Quilez).
//
// Barato a proposito, porque en la feria corre en celulares:
//   - se renderiza a 1/5 de la resolucion y el CSS lo estira (es un blur:
//     no necesita pixeles)
//   - 24 cuadros por segundo como mucho, y CERO si la pestaña no se ve
//   - con "reducir movimiento" pinta un solo cuadro y se queda quieto
//   - si no hay WebGL no pasa nada: queda el degradado CSS de abajo
import { useEffect, useRef } from 'react';
import { kiraVivo } from './vivo';

const VERT = `attribute vec2 p; void main(){ gl_Position = vec4(p, 0.0, 1.0); }`;

const FRAG = `precision mediump float;
uniform vec2 res; uniform float t; uniform vec3 tinte; uniform float energia;
float h(vec2 p){ return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }
float n(vec2 p){ vec2 i = floor(p), f = fract(p); f = f*f*(3.0-2.0*f);
  return mix(mix(h(i), h(i+vec2(1,0)), f.x), mix(h(i+vec2(0,1)), h(i+vec2(1,1)), f.x), f.y); }
float fbm(vec2 p){ float v = 0.0, a = 0.5; for (int i = 0; i < 4; i++){ v += a*n(p); p *= 2.03; a *= 0.5; } return v; }
void main(){
  vec2 uv = gl_FragCoord.xy / res; vec2 p = uv * vec2(res.x/res.y, 1.0) * 1.6;
  float tt = t * 0.045;
  vec2 q = vec2(fbm(p + tt), fbm(p + vec2(5.2, 1.3) - tt));
  float f = fbm(p + 2.2*q + vec2(tt*1.7, -tt));
  // luz que nace de arriba al centro (donde esta la cara) y se apaga abajo
  float foco = smoothstep(1.25, 0.05, distance(uv, vec2(0.5, 0.72)));
  float luz = (0.35 + 0.65*f*f) * (0.25 + 0.75*foco) * (0.55 + 0.45*energia);
  vec3 base = vec3(0.098, 0.094, 0.090);
  gl_FragColor = vec4(base + tinte * luz * luz * 0.85, 1.0);
}`;

/** Color del resplandor por emocion (rojo de Kira de base). */
const TINTES: Record<string, [number, number, number]> = {
  happy: [1.0, 0.36, 0.25],
  neutral: [0.9, 0.3, 0.26],
  surprised: [1.0, 0.55, 0.2],
  sad: [0.35, 0.42, 0.9],
  angry: [1.0, 0.12, 0.08],
  fastidio: [0.85, 0.35, 0.5],
  miedo: [0.55, 0.35, 0.95],
  cansado: [0.6, 0.45, 0.4],
};

export default function FondoVivo() {
  const ref = useRef<HTMLCanvasElement | null>(null);

  useEffect(() => {
    const canvas = ref.current;
    if (!canvas) return;
    const gl = canvas.getContext('webgl', { antialias: false, alpha: false, powerPreference: 'low-power' });
    if (!gl) return; // sin WebGL: queda el fondo CSS

    const shader = (tipo: number, src: string) => {
      const s = gl.createShader(tipo)!;
      gl.shaderSource(s, src);
      gl.compileShader(s);
      return s;
    };
    const prog = gl.createProgram()!;
    gl.attachShader(prog, shader(gl.VERTEX_SHADER, VERT));
    gl.attachShader(prog, shader(gl.FRAGMENT_SHADER, FRAG));
    gl.linkProgram(prog);
    if (!gl.getProgramParameter(prog, gl.LINK_STATUS)) return;
    gl.useProgram(prog);
    const buf = gl.createBuffer();
    gl.bindBuffer(gl.ARRAY_BUFFER, buf);
    gl.bufferData(gl.ARRAY_BUFFER, new Float32Array([-1, -1, 3, -1, -1, 3]), gl.STATIC_DRAW);
    const loc = gl.getAttribLocation(prog, 'p');
    gl.enableVertexAttribArray(loc);
    gl.vertexAttribPointer(loc, 2, gl.FLOAT, false, 0, 0);
    const uRes = gl.getUniformLocation(prog, 'res');
    const uT = gl.getUniformLocation(prog, 't');
    const uTinte = gl.getUniformLocation(prog, 'tinte');
    const uEnergia = gl.getUniformLocation(prog, 'energia');

    const ESCALA = 0.2;
    const medir = () => {
      const w = Math.max(32, Math.round(window.innerWidth * ESCALA));
      const hgt = Math.max(32, Math.round(window.innerHeight * ESCALA));
      if (canvas.width !== w || canvas.height !== hgt) {
        canvas.width = w;
        canvas.height = hgt;
        gl.viewport(0, 0, w, hgt);
      }
    };
    medir();
    window.addEventListener('resize', medir);

    const quieto = window.matchMedia?.('(prefers-reduced-motion: reduce)').matches === true;
    const tinte = [...TINTES.happy] as [number, number, number];
    let energia = 0.3;
    let raf = 0;
    let ultimo = 0;
    const t0 = performance.now();
    const cuadro = (ahora: number) => {
      raf = 0;
      if (document.hidden) return;
      if (!quieto) raf = requestAnimationFrame(cuadro);
      if (ahora - ultimo < 41 && !quieto) return; // ~24 fps
      ultimo = ahora;
      // el color y la energia viajan suave hacia lo que siente Kira
      const obj = TINTES[kiraVivo.emocion] ?? TINTES.neutral;
      for (let i = 0; i < 3; i++) tinte[i] += (obj[i] - tinte[i]) * 0.04;
      const objE =
        kiraVivo.estado === 'habla' ? 0.6 + 0.4 * kiraVivo.boca
          : kiraVivo.estado === 'piensa' ? 0.55
            : kiraVivo.estado === 'escucha' ? 0.45 + 0.55 * kiraVivo.nivel
              : 0.3;
      energia += (objE - energia) * 0.12;
      gl.uniform2f(uRes, canvas.width, canvas.height);
      gl.uniform1f(uT, quieto ? 0 : (ahora - t0) / 1000);
      gl.uniform3f(uTinte, tinte[0], tinte[1], tinte[2]);
      gl.uniform1f(uEnergia, energia);
      gl.drawArrays(gl.TRIANGLES, 0, 3);
    };
    const arrancar = () => {
      if (!raf && !document.hidden) raf = requestAnimationFrame(cuadro);
    };
    document.addEventListener('visibilitychange', arrancar);
    arrancar();
    return () => {
      if (raf) cancelAnimationFrame(raf);
      window.removeEventListener('resize', medir);
      document.removeEventListener('visibilitychange', arrancar);
      gl.getExtension('WEBGL_lose_context')?.loseContext();
    };
  }, []);

  return <canvas ref={ref} className="fondo-vivo" aria-hidden="true" />;
}
