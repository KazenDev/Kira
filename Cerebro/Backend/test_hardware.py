"""Pruebas deterministas de parsing y coordinación del hardware."""

from __future__ import annotations

import os
import tempfile
from pathlib import Path

os.environ["KIRA_SIN_SERIAL"] = "1"

from app.hardware.audio_capture import consume_audio_capture  # noqa: E402
from app.hardware.device_state import DeviceState  # noqa: E402
import kira_server as ks  # noqa: E402
from kira_server import SerialManager  # noqa: E402

FALLOS: list[str] = []


def check(condition: bool, name: str, detail: object = "") -> None:
    if condition:
        print(f"  [OK ] {name}")
    else:
        print(f"  [FALLA] {name} -> {detail}")
        FALLOS.append(name)


def test_audio_parser() -> None:
    print("== 1) captura binaria partida entre chunks ==")
    phase, buffer, audio, complete = consume_audio_capture(
        "esperando_start",
        b"",
        b"basura AUDIO:STAR",
    )
    check(phase == "esperando_start" and not audio and not complete, "START incompleto espera", (phase, buffer))
    phase, buffer, audio, complete = consume_audio_capture(phase, buffer, b"T\nabc")
    check(phase == "audio" and audio == b"abc", "el remanente de START se procesa una vez", (phase, audio))

    phase, buffer, audio, complete = consume_audio_capture(
        "esperando_start", b"", b"AUDIO:CANCEL\n"
    )
    check(
        phase == "cancelado" and audio == b"" and complete,
        "CANCEL antes de START cancela la espera",
        (phase, audio, complete),
    )

    phase, buffer, audio, complete = consume_audio_capture("audio", b"", b"defAUDIO:EN")
    check(audio == b"def" and not complete, "audio antes de END incompleto", (audio, complete))
    phase, buffer, audio, complete = consume_audio_capture(phase, buffer, b"D\n")
    check(audio == b"" and complete, "END completes la captura", (audio, complete))

    phase, buffer, audio, complete = consume_audio_capture(
        "audio", b"", b"xyzAUDIO:CANCEL\n"
    )
    check(
        phase == "cancelado" and audio == b"" and complete,
        "CANCEL descarta la captura",
        (phase, audio, complete),
    )


def test_device_state() -> None:
    print("\n== 2) estado central del dispositivo ==")
    state = DeviceState()
    state.mark_connected("/dev/test")
    state.mark_ack("ACK:HAPPY", 10.0)
    state.mark_led("" + "#" * 30, 11.0)
    snapshot = state.snapshot()
    check(snapshot["conectado"] and snapshot["respondiendo"], "snapshot refleja ACK", snapshot)
    check(len(snapshot["leds"]) == 25, "snapshot limita LED a 25 chars", snapshot)
    state.mark_disconnected()
    check(not state.connected, "mark_disconnected", state.snapshot())


def test_exclusive_device_requests() -> None:
    print("\n== 3) una sola operación física excluyente ==")
    manager = SerialManager()
    try:
        manager._exclusive_request.acquire()
        check(manager.grabar(1000, 0.01) is None, "una segunda grabación se rechaza", "no bloquea")
        check(manager.leer_sensor("SENSOR:TEMP", "TEMP:", 0.01) is None, "un sensor no pisa la captura", "no bloquea")
        check(manager.escuchar(lambda _level: None, timeout=0.01) is None, "una escucha no pisa otro request", "no bloquea")
        manager._exclusive_request.release()
    finally:
        manager.close()
    check(manager.hilo is None, "close termina el worker serial", manager.hilo)


def test_unique_audio_files() -> None:
    print("\n== 4) conversiones de audio sin colisiones ==")
    original = ks.GRABACIONES_DIR
    with tempfile.TemporaryDirectory(prefix="kira_audio_test_") as tmp:
        ks.GRABACIONES_DIR = tmp
        try:
            first = ks.samples_a_mp3(bytes([0, 127, 255]) * 700)
            second = ks.samples_a_mp3(bytes([1, 126, 254]) * 700)
        finally:
            ks.GRABACIONES_DIR = original
        check(first is not None and second is not None, "ffmpeg genera ambos MP3", (first, second))
        if first and second:
            check(first != second, "cada grabación tiene nombre único", (first, second))
        check(not list(Path(tmp).glob("*.wav")), "no quedan WAV temporales", list(Path(tmp).iterdir()))


def main() -> int:
    test_audio_parser()
    test_device_state()
    test_exclusive_device_requests()
    test_unique_audio_files()
    print()
    if FALLOS:
        print(f"FALLARON {len(FALLOS)} chequeo(s):")
        for failure in FALLOS:
            print(f"  - {failure}")
        return 1
    print("HARDWARE OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
