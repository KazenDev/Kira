// sonidos.ts - Sonidos sutiles de la app (WebAudio, sin archivos externos).
// Están DESACTIVADOS por defecto: el usuario los activa en Ajustes.
// Persisten en localStorage ('kira_sonido' = '1' | '0').

let audioCtx: AudioContext | null = null;

function ctx(): AudioContext | null {
  try {
    if (!audioCtx) {
      const AC = window.AudioContext || (window as unknown as { webkitAudioContext?: typeof AudioContext }).webkitAudioContext;
      if (!AC) return null;
      audioCtx = new AC();
    }
    if (audioCtx.state === 'suspended') audioCtx.resume().catch(() => {});
    return audioCtx;
  } catch {
    return null;
  }
}

export function sonidoActivo(): boolean {
  try {
    return localStorage.getItem('kira_sonido') === '1';
  } catch {
    return false;
  }
}

export function setSonidoActivo(v: boolean) {
  try {
    localStorage.setItem('kira_sonido', v ? '1' : '0');
  } catch {
    /* ignore */
  }
}

// un "pop" suave con envolvente (evita clicks)
function tono(frec: number, dur: number, vol: number, delay = 0) {
  const c = ctx();
  if (!c) return;
  const osc = c.createOscillator();
  const gan = c.createGain();
  osc.type = 'sine';
  osc.frequency.value = frec;
  const t0 = c.currentTime + delay;
  gan.gain.setValueAtTime(0.0001, t0);
  gan.gain.exponentialRampToValueAtTime(vol, t0 + 0.012);
  gan.gain.exponentialRampToValueAtTime(0.0001, t0 + dur);
  osc.connect(gan).connect(c.destination);
  osc.start(t0);
  osc.stop(t0 + dur + 0.02);
}

// al ENVIAR tu mensaje: dos notas cortas subiendo (sutil, nada de fanfarria)
export function sonidoEnviar() {
  if (!sonidoActivo()) return;
  tono(520, 0.09, 0.06);
  tono(660, 0.11, 0.05, 0.07);
}

// al RECIBIR respuesta: una nota suave que baja (no molesta)
export function sonidoRecibir() {
  if (!sonidoActivo()) return;
  tono(440, 0.13, 0.06);
  tono(330, 0.15, 0.04, 0.09);
}

// TRANSICIONES de vista: un micro-tono que acompaña el movimiento de la UI
// (abrir/cerrar paneles, cambiar de conversación). Más bajito que
// enviar/recibir para no competir con la conversación. Mismo switch que el
// resto: si el sonido está apagado, no suena nada (opcional de verdad).
export function sonidoUI(tipo: 'abrir' | 'cerrar' | 'cambio') {
  if (!sonidoActivo()) return;
  if (tipo === 'abrir') {
    // el panel SUBE -> las notas suben (misma lógica física que el motion)
    tono(392, 0.09, 0.03);
    tono(523, 0.11, 0.026, 0.05);
  } else if (tipo === 'cerrar') {
    // cierra -> baja
    tono(523, 0.08, 0.026);
    tono(392, 0.1, 0.022, 0.045);
  } else {
    // cambio de chat: un blip neutro, sin dirección
    tono(494, 0.08, 0.028);
  }
}
