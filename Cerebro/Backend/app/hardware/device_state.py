"""Mutable runtime state exposed by the micro:bit device gateway."""

from __future__ import annotations

from dataclasses import dataclass


@dataclass(slots=True)
class DeviceState:
    connected: bool = False
    responding: bool = False
    port: str | None = None
    last_command: str = ""
    last_ack: str | None = None
    last_ack_time: float = 0.0
    pending: str = ""
    led_pattern: str | None = None
    led_pattern_time: float = 0.0

    def mark_disconnected(self) -> None:
        self.connected = False

    def mark_connected(self, port: str) -> None:
        self.connected = True
        self.port = port

    def mark_ack(self, ack: str, now: float) -> None:
        self.last_ack = ack
        self.last_ack_time = now
        self.responding = True

    def mark_led(self, pattern: str, now: float) -> None:
        self.led_pattern = pattern[:25]
        self.led_pattern_time = now

    def snapshot(self) -> dict:
        return {
            "conectado": self.connected,
            "respondiendo": self.responding,
            "puerto": self.port,
            "ultimo_comando": self.last_command,
            "ultimo_ack": self.last_ack,
            "leds": self.led_pattern,
        }
