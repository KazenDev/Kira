"""Pure parser for the micro:bit's binary AUDIO:START/END capture stream."""

from __future__ import annotations

START = b"AUDIO:START\n"
END = b"AUDIO:END\n"


def consume_audio_capture(
    phase: str,
    buffer: bytes,
    data: bytes,
) -> tuple[str, bytes, bytes, bool]:
    """Consume one serial chunk and return ``phase, buffer, audio, complete``."""

    pending = buffer + data
    if phase == "esperando_start":
        start = pending.find(START)
        if start < 0:
            return phase, pending[-(len(START) - 1) :], b"", False
        pending = pending[start + len(START) :]
        phase = "audio"

    finish = pending.find(END)
    if finish >= 0:
        return phase, b"", pending[:finish], True

    # Retiene sólo un sufijo que pueda ser el comienzo de AUDIO:END.
    keep = 0
    for size in range(min(len(pending), len(END) - 1), 0, -1):
        if END.startswith(pending[-size:]):
            keep = size
            break
    if keep:
        return phase, pending[-keep:], pending[:-keep], False
    return phase, b"", pending, False
