"""Serial micro:bit transport with worker, ACK, sensors and audio capture."""

from __future__ import annotations

import os
import queue
import threading
import time

import serial
import serial.tools.list_ports

from app.hardware.audio_capture import consume_audio_capture
from app.hardware.ble_relay import RelayBroker
from app.hardware.device_state import DeviceState


PREFIJOS_SENSOR = frozenset({"TEMP:", "LUZ:", "BOTON:", "ACCEL:", "MIC:", "BAT:"})


class SerialTransport:
    def __init__(self, baud: int = 115200):
        self.baud = baud
        self._cola = queue.Queue()
        self._lock = threading.RLock()
        self._exclusive_request = threading.Lock()
        self._stop_event = threading.Event()
        self._ser: serial.Serial | None = None
        self._buf = b""

        # Estado central del dispositivo; las properties de más abajo conservan
        # el acceso legacy que usan tools, status y tests.
        self.state = DeviceState()
        # request-response de sensores (tool calling de la IA):
        # (prefijo esperado, threading.Event, lista donde guardar la respuesta)
        self._respuesta_esperada: tuple | None = None
        # GRABACION de audio: modo binario (sin parsear lineas).
        # dict con datos, evento, fase ('esperando_start'|'audio'), buf
        self._captura: dict | None = None
        self._ultima_captura_cancelada = False
        # Relay BLE extraído. Los aliases siguientes mantienen compatibilidad
        # con diagnóstico y tests legacy.
        self.relay = RelayBroker(lock=self._lock)
        self._relay_tx = self.relay.queue

        self.hilo: threading.Thread | None = None
        self.start()

    def start(self) -> None:
        with self._lock:
            if self.hilo is not None and self.hilo.is_alive():
                return
            self._stop_event.clear()
            self.hilo = threading.Thread(target=self._worker, daemon=True)
            self.hilo.start()

    def close(self) -> None:
        """Cierra el serial y termina el worker; permite reiniciar en tests."""
        self._stop_event.set()
        with self._lock:
            port = self._ser
            self._ser = None
        if port is not None:
            try:
                port.close()
            except Exception:
                pass
        self.state.mark_disconnected()
        thread = self.hilo
        if thread is not None and thread.is_alive():
            thread.join(timeout=1.0)
        self.hilo = None

    @property
    def conectado(self) -> bool:
        return self.state.connected

    @conectado.setter
    def conectado(self, value: bool) -> None:
        self.state.connected = bool(value)

    @property
    def respondiendo(self) -> bool:
        return self.state.responding

    @respondiendo.setter
    def respondiendo(self, value: bool) -> None:
        self.state.responding = bool(value)

    @property
    def puerto_actual(self) -> str | None:
        return self.state.port

    @puerto_actual.setter
    def puerto_actual(self, value: str | None) -> None:
        self.state.port = value

    @property
    def ultimo_comando(self) -> str:
        return self.state.last_command

    @ultimo_comando.setter
    def ultimo_comando(self, value: str) -> None:
        self.state.last_command = value

    @property
    def ultimo_ack(self) -> str | None:
        return self.state.last_ack

    @ultimo_ack.setter
    def ultimo_ack(self, value: str | None) -> None:
        self.state.last_ack = value

    @property
    def ultimo_ack_time(self) -> float:
        return self.state.last_ack_time

    @ultimo_ack_time.setter
    def ultimo_ack_time(self, value: float) -> None:
        self.state.last_ack_time = float(value)

    @property
    def pendiente(self) -> str:
        return self.state.pending

    @pendiente.setter
    def pendiente(self, value: str) -> None:
        self.state.pending = value

    @property
    def ultima_captura_cancelada(self) -> bool:
        return self._ultima_captura_cancelada

    @property
    def patron_leds(self) -> str | None:
        return self.state.led_pattern

    @patron_leds.setter
    def patron_leds(self, value: str | None) -> None:
        self.state.led_pattern = value

    @property
    def patron_leds_time(self) -> float:
        return self.state.led_pattern_time

    @patron_leds_time.setter
    def patron_leds_time(self, value: float) -> None:
        self.state.led_pattern_time = float(value)

    @property
    def relay_conectado(self) -> bool:
        return self.relay.connected

    @relay_conectado.setter
    def relay_conectado(self, value: bool) -> None:
        self.relay.connected = bool(value)

    @property
    def relay_token(self) -> str | None:
        return self.relay.token

    @relay_token.setter
    def relay_token(self, value: str | None) -> None:
        self.relay.token = value

    @property
    def relay_ultimo_poll(self) -> float:
        return self.relay.last_poll

    @relay_ultimo_poll.setter
    def relay_ultimo_poll(self, value: float) -> None:
        self.relay.last_poll = float(value)

    @property
    def _relay_registros(self) -> list[float]:
        return self.relay.registrations

    @_relay_registros.setter
    def _relay_registros(self, value: list[float]) -> None:
        self.relay.registrations = value

    @property
    def _relay_registros_token(self) -> str | None:
        return self.relay.registration_token

    @_relay_registros_token.setter
    def _relay_registros_token(self, value: str | None) -> None:
        self.relay.registration_token = value

    @property
    def _relay_bucle_avisado(self) -> bool:
        return self.relay.loop_warned

    @_relay_bucle_avisado.setter
    def _relay_bucle_avisado(self, value: bool) -> None:
        self.relay.loop_warned = bool(value)

    @property
    def ble_error(self) -> str | None:
        return self.relay.error

    @ble_error.setter
    def ble_error(self, value: str | None) -> None:
        self.relay.error = value

    def _detectar_puerto(self) -> str | None:
        """Encuentra el micro:bit solo, en Linux (/dev/ttyACM*) o Windows (COM*).
        Con KIRA_SIN_SERIAL=1 devuelve None (nunca abre nada): util para correr
        los tests o el server sin placa — abrir el puerto RESETEA el micro:bit,
        asi que un test no puede permitirse tocar el USB de un equipo ajeno."""
        if os.environ.get("KIRA_SIN_SERIAL") == "1":
            return None
        try:
            for p in serial.tools.list_ports.comports():
                desc = (p.description or "").lower()
                if "micro" in desc or "mbed" in desc or "acm" in desc:
                    return p.device
        except Exception:
            pass
        # fallback Linux
        for d in ("/dev/ttyACM0", "/dev/ttyACM1"):
            if os.path.exists(d):
                return d
        return None

    def _worker(self):
        ultimo_intento = 0.0
        while not self._stop_event.is_set():
            # 1) tomar comando de la cola si hay (sin bloquear mucho;
            #    en modo relay reacciona mas rapido: la IA esta esperando)
            try:
                cmd = self._cola.get(timeout=0.05 if self.relay_conectado else 0.3)
                self.pendiente = cmd
            except queue.Empty:
                cmd = None

            ahora = time.time()

            # 1.5) MODO RELAY BLE: sin serial y con el navegador-puente VIVO
            # (dueño latiendo), los comandos no van a un puerto: van a la
            # cola que el navegador toma (GET /api/ble/tx) y lleva por aire.
            if self.relay_vivo() and self._ser is None:
                a_enviar = cmd if cmd is not None else self.pendiente
                if a_enviar:
                    self._relay_encolar(a_enviar + "\n")
                    self.ultimo_comando = a_enviar
                    self.pendiente = ""
                if ahora - self.ultimo_ack_time > 10:
                    self.respondiendo = False
                self._stop_event.wait(0.05)
                continue

            # 2) si no hay conexion, intentar conectar (cada ~3s)
            if self._ser is None or not self._ser.is_open:
                if ahora - ultimo_intento < 3:
                    self._stop_event.wait(0.2)
                    continue
                ultimo_intento = ahora
                puerto = self._detectar_puerto()
                if puerto is None:
                    self.conectado = False
                    self._stop_event.wait(0.5)
                    continue
                try:
                    self._ser = serial.Serial(puerto, self.baud, timeout=0.1)
                    self._stop_event.wait(2)  # el micro:bit se reinicia al abrir el puerto
                    if self._stop_event.is_set():
                        try:
                            self._ser.close()
                        except Exception:
                            pass
                        self._ser = None
                        self.conectado = False
                        break
                    # descartar la basura del boot (bytes viejos/desync):
                    # si no, el parser de lineas los tragaba como ACKs
                    self._ser.reset_input_buffer()
                    self._buf = b""
                    self.puerto_actual = puerto
                    self.conectado = True
                    print(f"[SERIAL] micro:bit conectado en {puerto}")
                except Exception:
                    self._ser = None
                    self.conectado = False
                    self._stop_event.wait(1)
                    continue

            # 3) mandar el pendiente (o el ultimo conocido) y leer la respuesta
            try:
                a_enviar = cmd if cmd is not None else self.pendiente
                if a_enviar:
                    with self._lock:
                        self._ser.write((a_enviar + "\n").encode())
                    self.ultimo_comando = a_enviar
                    self.pendiente = ""
                self._leer_respuesta()
            except Exception:
                try:
                    if self._ser:
                        self._ser.close()
                except Exception:
                    pass
                self._ser = None
                self.conectado = False
                self._stop_event.wait(1)

            # 4) si hace mas de 10s sin ACK, ya no decimos que responde
            if ahora - self.ultimo_ack_time > 10:
                self.respondiendo = False

            self._stop_event.wait(0.05)

    def _leer_captura(self):
        """Lee audio binario entre START/END sin duplicar el remanente."""
        try:
            datos = self._ser.read(4096)
        except Exception:
            return
        if not datos:
            return
        cap = self._captura
        if cap is None:
            return

        phase, buffer, audio, complete = consume_audio_capture(
            cap["fase"],
            cap["buf"],
            datos,
        )
        cap["fase"] = phase
        cap["buf"] = buffer
        if phase == "cancelado":
            cap["cancelado"] = True
            cap["datos"].clear()
            cap["evento"].set()
            return
        if complete:
            cap["evento"].set()

        if not audio:
            return
        cap["datos"].extend(audio)

        cb_chunk = cap.get("cb_chunk")
        if cb_chunk is not None:
            try:
                cb_chunk(bytes(audio))
            except Exception:
                pass

        cb = cap.get("cb_nivel")
        if cb is not None:
            count = len(audio)
            media = sum(audio) / count
            deviation = sum(abs(value - media) for value in audio) / count
            try:
                cb(deviation)
            except Exception:
                pass

    def _leer_respuesta(self):
        """Lee lo que el micro:bit devuelve.
        - ACK:... = esta VIVO
        - LED:<25 chars> = replica del display (lo guardamos para /api/status)
        - Durante RECORD: los bytes crudos del audio (modo binario)
        """
        # MODO GRABACION: no parsear lineas, juntar el audio crudo
        if self._captura is not None:
            self._leer_captura()
            return
        try:
            datos = self._ser.read(512)
        except Exception:
            return
        if not datos:
            return
        self._buf += datos
        while b"\n" in self._buf:
            linea, self._buf = self._buf.split(b"\n", 1)
            texto = linea.decode(errors="replace").strip()
            if not texto:
                continue
            if texto.startswith("LED:"):
                # frame de la replica: lo guardamos sin imprimirlo (es ruido)
                patron = texto[4:]
                if len(patron) >= 25:
                    self.patron_leds = patron[:25]
                    self.patron_leds_time = time.time()
                continue
            self.ultimo_ack = texto
            self.ultimo_ack_time = time.time()
            self.respondiendo = True
            print(f"[SERIAL] << {texto}")
            # si la IA pidio leer un sensor, avisarle al que esta esperando
            esperado = self._respuesta_esperada
            if esperado:
                prefijo, evento, resultado = esperado
                if texto.startswith(prefijo) or (
                    prefijo in PREFIJOS_SENSOR and texto == "SENSOR:?"
                ):
                    resultado.append(texto)
                    evento.set()

    def enviar(self, cmd: str):
        self._cola.put(cmd)

    def escuchar(
        self,
        callback_nivel,
        callback_chunk=None,
        timeout: float = 40.0,
    ) -> bytes | None:
        """Captura serializada de audio; otra operación física la rechaza."""
        if not self._exclusive_request.acquire(blocking=False):
            return None
        try:
            evento = threading.Event()
            datos = bytearray()
            with self._lock:
                self._captura = {
                    "datos": datos,
                    "evento": evento,
                    "fase": "esperando_start",
                    "buf": b"",
                    "cb_nivel": callback_nivel,
                    "cb_chunk": callback_chunk,
                    "cancelado": False,
                }
            self._ultima_captura_cancelada = False
            self.enviar("ESCUCHAR")
            if not evento.wait(timeout):
                return None
            cancelada = bool(cap.get("cancelado"))
            self._ultima_captura_cancelada = cancelada
            return None if cancelada else bytes(datos)
        finally:
            with self._lock:
                self._captura = None
            self._exclusive_request.release()

    def grabar(self, duracion_ms: int, timeout: float = 12.0) -> bytes | None:
        """Graba en el micro:bit bajo el lock exclusivo del dispositivo."""
        if not self._exclusive_request.acquire(blocking=False):
            return None
        try:
            evento = threading.Event()
            datos = bytearray()
            with self._lock:
                self._captura = {
                    "datos": datos,
                    "evento": evento,
                    "fase": "esperando_start",
                    "buf": b"",
                    "cancelado": False,
                }
            self._ultima_captura_cancelada = False
            self.enviar(f"RECORD:{duracion_ms}")
            if not evento.wait(timeout):
                return None
            return bytes(datos)
        finally:
            with self._lock:
                self._captura = None
            self._exclusive_request.release()

    def leer_sensor(self, comando: str, prefijo: str, timeout: float = 4.0) -> str | None:
        """Request-response serializado; nunca pisa otra espera activa."""
        if not self._exclusive_request.acquire(blocking=False):
            return None
        try:
            if self.relay_conectado:
                timeout = max(timeout, 9.0)
            evento = threading.Event()
            resultado: list[str] = []
            with self._lock:
                self._respuesta_esperada = (prefijo, evento, resultado)
            self.enviar(comando)
            evento.wait(timeout)
            return resultado[0] if resultado else None
        finally:
            with self._lock:
                self._respuesta_esperada = None
            self._exclusive_request.release()

    # ------------------------------------------------------------------
    # MODO RELAY BLE: el navegador es el "cable" (Web Bluetooth)
    # ------------------------------------------------------------------
    def _relay_depurar(self):
        self.relay._expire()

    def relay_vivo(self) -> bool:
        return self.relay.alive()

    def relay_registrar(self, token: str) -> str:
        return self.relay.register(token)

    def relay_latido(self, token: str) -> str:
        return self.relay.heartbeat(token)

    def relay_liberar(self, token: str):
        self.relay.release(token)

    def _relay_encolar(self, linea: str):
        self.relay.enqueue(linea)

    def relay_tomar_tx(self) -> list[str]:
        return self.relay.take()

    def relay_devolver(self, lineas: list[str]):
        self.relay.return_lines(lineas)

    def relay_linea_entrante(self, texto: str):
        """Linea que llega DEL micro:bit via el navegador (ACK, TEMP:, LED:...).
        Espejo exacto del parseo de _leer_respuesta, pero alimentado por HTTP."""
        texto = texto.strip()
        if not texto:
            return
        if texto.startswith("LED:"):
            patron = texto[4:]
            if len(patron) >= 25:
                self.patron_leds = patron[:25]
                self.patron_leds_time = time.time()
            return
        # log del viaje placa -> server (los LED: son ruido, no se imprimen)
        print(f"[BLE RELAY] << {texto}")
        self.ultimo_ack = texto
        self.ultimo_ack_time = time.time()
        self.respondiendo = True
        esperado = self._respuesta_esperada
        if esperado:
            prefijo, evento, resultado = esperado
            if texto.startswith(prefijo) or (
                prefijo in PREFIJOS_SENSOR and texto == "SENSOR:?"
            ):
                resultado.append(texto)
                evento.set()


# Compatibility name retained while callers migrate to SerialTransport.
SerialManager = SerialTransport
