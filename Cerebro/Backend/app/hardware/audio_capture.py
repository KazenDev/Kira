"""Pure parser for the micro:bit's binary audio capture stream."""

from __future__ import annotations

START = b"AUDIO:START\n"
END = b"AUDIO:END\n"
CANCEL = b"AUDIO:CANCEL\n"


def consume_audio_capture(
    phase: str,
    buffer: bytes,
    data: bytes,
) -> tuple[str, bytes, bytes, bool]:
    """Consume one serial chunk and return ``phase, buffer, audio, complete``.

    ``AUDIO:CANCEL`` completes the capture like ``AUDIO:END`` but moves to the
    ``cancelado`` phase and returns no audio. The caller can therefore discard
    a push-to-talk recording without sending it to a transcriber.
    """

    pending = buffer + data
    if phase == "esperando_start":
        cancel = pending.find(CANCEL)
        if cancel >= 0:
            return "cancelado", b"", b"", True
        start = pending.find(START)
        if start < 0:
            keep = max(len(START), len(CANCEL)) - 1
            return phase, pending[-keep:], b"", False
        pending = pending[start + len(START) :]
        phase = "audio"

    cancel = pending.find(CANCEL)
    if cancel >= 0:
        # Discard everything before the marker: a cancelled recording must not
        # accidentally be treated as a valid file by the caller.
        return "cancelado", b"", b"", True

    finish = pending.find(END)
    if finish >= 0:
        return phase, b"", pending[:finish], True

    # Retiene sólo un sufijo que pueda ser el comienzo de uno de los markers.
    keep = 0
    for marker in (END, CANCEL):
        for size in range(min(len(pending), len(marker) - 1), 0, -1):
            if marker.startswith(pending[-size:]):
                keep = max(keep, size)
                break
    if keep:
        return phase, pending[-keep:], pending[:-keep], False
    return phase, b"", pending, False
