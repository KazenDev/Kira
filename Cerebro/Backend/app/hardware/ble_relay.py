"""Thread-safe browser BLE relay ownership and command queue policy."""

from __future__ import annotations

import threading
import time
from collections.abc import Callable

RELAY_GRACIA_SEG = 120.0
RELAY_TX_VIDA_SEG = 15.0
RELAY_TX_MAX = 12
RELAY_REGISTRO_VENTANA = 10.0
RELAY_REGISTRO_MAX = 20


class RelayBroker:
    def __init__(
        self,
        lock: threading.RLock | None = None,
        now: Callable[[], float] = time.time,
    ) -> None:
        self.lock = lock or threading.RLock()
        self.now = now
        self.connected = False
        self.queue: list[tuple[float, str]] = []
        self.token: str | None = None
        self.last_poll = 0.0
        self.registrations: list[float] = []
        self.registration_token: str | None = None
        self.loop_warned = False
        self.error: str | None = None

    def _expire(self) -> None:
        if self.connected and (self.now() - self.last_poll) > RELAY_GRACIA_SEG:
            print("[BLE RELAY] el puente dejo de latir: puesto liberado")
            self.connected = False
            self.token = None

    def alive(self) -> bool:
        with self.lock:
            self._expire()
            return self.connected

    def register(self, token: str) -> str:
        with self.lock:
            self._expire()
            if self.connected and self.token != token:
                return "ocupado"
            now = self.now()
            if self.registration_token != token:
                self.registration_token = token
                self.registrations = []
                self.loop_warned = False
            self.registrations = [
                stamp for stamp in self.registrations
                if now - stamp < RELAY_REGISTRO_VENTANA
            ]
            self.registrations.append(now)
            if len(self.registrations) > RELAY_REGISTRO_MAX:
                if not self.loop_warned:
                    print(
                        f"[BLE RELAY] AVISO: el token {token[:8]}... pidio el puesto "
                        f"{len(self.registrations)}+ veces en {RELAY_REGISTRO_VENTANA:.0f}s "
                        "-> es un BUCLE del navegador (bundle viejo cacheado?). "
                        "Lo freno con 429: hay que RECARGAR la app en ese dispositivo."
                    )
                    self.loop_warned = True
                return "bucle"
            self.connected = True
            self.token = token
            self.last_poll = self.now()
            return "ok"

    def heartbeat(self, token: str) -> str:
        with self.lock:
            self._expire()
            if self.connected and self.token == token:
                self.last_poll = self.now()
                return "ok"
            if not self.connected:
                return "libre"
            return "ocupado"

    def release(self, token: str) -> None:
        with self.lock:
            if self.token == token:
                self.connected = False
                self.token = None

    def enqueue(self, line: str) -> None:
        with self.lock:
            self.queue.append((self.now(), line))
            if len(self.queue) > RELAY_TX_MAX:
                self.queue[:] = self.queue[-RELAY_TX_MAX:]

    def take(self) -> list[str]:
        with self.lock:
            pending = self.queue[:]
            self.queue.clear()
        now = self.now()
        fresh = [line for stamp, line in pending if now - stamp <= RELAY_TX_VIDA_SEG]
        expired = len(pending) - len(fresh)
        if expired:
            print(
                f"[BLE RELAY] descarto {expired} comando(s) vencido(s) "
                f"(> {RELAY_TX_VIDA_SEG:.0f}s)"
            )
        if fresh:
            print(f"[BLE RELAY] >> {''.join(fresh).strip()}")
        return fresh

    def return_lines(self, lines: list[str]) -> None:
        with self.lock:
            now = self.now()
            existing = {line for _stamp, line in self.queue}
            for line in reversed(list(lines)):
                clean = str(line).strip().upper()[:24]
                if not clean or "\n" in clean or "\r" in clean:
                    continue
                normalized = clean + "\n"
                if normalized in existing:
                    continue
                existing.add(normalized)
                self.queue.insert(0, (now, normalized))
            if len(self.queue) > RELAY_TX_MAX:
                self.queue[:] = self.queue[-RELAY_TX_MAX:]
        print(f"[BLE RELAY] devueltos a la cola: {[str(line).strip() for line in lines]}")


# Name used by the architecture plan; RelayBroker remains the compatibility name.
BleRelayHub = RelayBroker
