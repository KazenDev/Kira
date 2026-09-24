# ================================================================================
#  VAD Silero (ONNX) — deteccion de voz NEURONAL para la escucha GPT
#
#  Reemplaza el corte por energia del firmware: Silero clasifica cada frame
#  de audio como VOZ/SILENCIO real (no se confunde con ruido, respiraciones
#  ni colas de palabra que se apagan). Con eso el server decide cuando
#  TERMINASTE de hablar y manda STOP al micro:bit en ese momento justo.
#
#  - El micro:bit streamea int8 signed @ 11025 Hz (chunks de 128 bytes).
#  - Silero v5 (ONNX) come frames de 512 samples float32 @ 16000 Hz.
#  - Conversion: int8/128 -> [-1,1], resample lineal 11k -> 16k.
#
#  Politica de corte (configurable en config.py):
#    prob >= VAD_PROB_VOZ      -> frame con voz
#    prob <= VAD_PROB_SILENCIO -> silencio real (con histeresis)
#    voz sostenida >= 250ms    -> arranco el turno (evita falsos disparos)
#    silencio >= VAD_SILENCIO_FIN_MS tras hablar -> FIN DEL TURNO
# ================================================================================
import threading

import numpy as np
import onnxruntime as ort

import config

# ---- constantes del modelo (v5) ----
SR_MIC = 11000    # sample rate real del micro:bit
SR_VAD = 16000    # lo que espera Silero
FRAME = 512       # samples por frame @ 16 kHz
MS_FRAME = FRAME * 1000.0 / SR_VAD   # 32 ms por frame

_lock = threading.Lock()
_session: ort.InferenceSession | None = None


def _sesion() -> ort.InferenceSession:
    """Carga el modelo UNA sola vez (lazy, thread-safe)."""
    global _session
    with _lock:
        if _session is None:
            _session = ort.InferenceSession(
                config.VAD_MODELO, providers=["CPUExecutionProvider"]
            )
        return _session


class VADSilero:
    """Maquina de estados por frame: 'nadie hablo' -> 'hablando' -> 'fin'.

    Se alimenta con `procesar(bytes)` desde el hilo del serial (es barato:
    ~1ms de CPU por frame). Cuando detecta el fin del turno deja
    `fin_detectado = True` y deja de trabajar.
    """

    def __init__(self):
        self.sesion = _sesion()
        self.estado = np.zeros((2, 1, 128), dtype=np.float32)  # estado LSTM
        self.buf16 = np.empty(0, dtype=np.float32)   # samples @16k sin procesar
        self.ultimo_x = 0.0                          # ultimo sample 11k (stitch)
        self.t_voz_ms = 0.0        # voz acumulada (para confirmar turno)
        self.hubo_voz = False      # ya confirmamos que el usuario hablo
        self.t_silencio_ms = 0.0   # silencio continuo desde la ultima voz
        self.fin_detectado = False
        self.ultima_prob = 0.0     # para debug/logs

    # ------------------------------------------------------------------
    # API principal: darle un chunk crudo del micro:bit (int8 signed)
    # ------------------------------------------------------------------
    def procesar(self, chunk: bytes) -> float:
        """Alimenta el VAD con samples nuevos. Devuelve la ultima prob de voz."""
        if self.fin_detectado or not chunk:
            return self.ultima_prob
        x = np.frombuffer(chunk, dtype=np.int8).astype(np.float32) / 128.0
        x16 = self._resample16k(x)
        self.buf16 = np.concatenate([self.buf16, x16])
        while len(self.buf16) >= FRAME:
            frame = self.buf16[:FRAME]
            self.buf16 = self.buf16[FRAME:]
            out, self.estado = self.sesion.run(
                None,
                {
                    "input": frame[None, :],
                    "state": self.estado,
                    "sr": np.array(SR_VAD, dtype=np.int64),
                },
            )
            self._paso(float(out[0][0]))
        return self.ultima_prob

    # ------------------------------------------------------------------
    # Resample 11k -> 16k (lineal, con carry del ultimo sample para que
    # los bordes de chunk no metan discontinuidades)
    # ------------------------------------------------------------------
    def _resample16k(self, x: np.ndarray) -> np.ndarray:
        n = len(x)
        if n == 0:
            return x
        # prepend el ultimo sample anterior: costura entre chunks
        x = np.concatenate([[self.ultimo_x], x])
        self.ultimo_x = float(x[-1])
        dur = (n) / SR_MIC  # duracion de los samples NUEVOS
        pasos = max(1, int(round(dur * SR_VAD)))
        t_new = np.arange(pasos) / SR_VAD
        t_old = np.arange(n + 1) / SR_MIC
        return np.interp(t_new, t_old, x).astype(np.float32)

    # ------------------------------------------------------------------
    # Un frame (32ms) -> actualizar la maquina de estados
    # ------------------------------------------------------------------
    def _paso(self, prob: float) -> None:
        self.ultima_prob = prob
        if prob >= config.VAD_PROB_VOZ:
            # voz: acumula y corta la racha de silencio
            self.t_voz_ms += MS_FRAME
            self.t_silencio_ms = 0.0
            if not self.hubo_voz and self.t_voz_ms >= config.VAD_MIN_VOZ_MS:
                self.hubo_voz = True  # turno confirmado
        elif prob <= config.VAD_PROB_SILENCIO:
            # silencio real: solo cuenta DESPUES de que hubo habla
            if self.hubo_voz:
                self.t_silencio_ms += MS_FRAME
                if self.t_silencio_ms >= config.VAD_SILENCIO_FIN_MS:
                    self.fin_detectado = True
        else:
            # zona gris (0.35..0.5): cola de palabra, respiracion suave:
            # no cuenta como silencio (no cortar palabra) ni como voz
            self.t_silencio_ms = 0.0


# ================================================================================
#  Test rapido: python3 vad.py  (mecanica con audio sintetico, sin voz real)
# ================================================================================
if __name__ == "__main__":
    import time

    v = VADSilero()
    print(f"modelo cargado, frame = {MS_FRAME:.0f} ms")

    def seg(segundos: float, amplitud: float) -> bytes:
        n = int(segundos * SR_MIC)
        # ruido "tipo voz" (banda de energia variando) o silencio puro
        if amplitud == 0:
            return (np.zeros(n, dtype=np.int8)).tobytes()
        t = np.arange(n) / SR_MIC
        senal = amplitud * np.sin(2 * np.pi * 200 * t) * (0.6 + 0.4 * np.sin(2 * np.pi * 3 * t))
        return senal.astype(np.int8).tobytes()

    t0 = time.time()
    # 1s de silencio inicial -> no debe pasar nada
    v.procesar(seg(1.0, 0))
    print(f"1s silencio: hubo_voz={v.hubo_voz} fin={v.fin_detectado} prob={v.ultima_prob:.4f}")
    # con prob umbral bajado a mano simulamos "voz" para probar la mecanica:
    config.VAD_PROB_VOZ = 0.0005  # trampa para el test sintetico
    v.procesar(seg(0.5, 40))
    print(f"0.5s 'voz': hubo_voz={v.hubo_voz} fin={v.fin_detectado}")
    config.VAD_PROB_VOZ = 0.5
    config.VAD_PROB_SILENCIO = 0.5  # ahora el silencio (prob~0) dispara
    v.procesar(seg(1.0, 0))
    print(f"1s silencio post-voz: fin={v.fin_detectado} (silencio {v.t_silencio_ms:.0f}ms)")
    print(f"tiempo total de CPU: {(time.time()-t0)*1000:.0f} ms")
