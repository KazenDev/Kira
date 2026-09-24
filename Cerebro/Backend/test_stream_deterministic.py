"""Wrapper determinista del stream: no llama proveedores reales."""

from __future__ import annotations

import asyncio
import os

os.environ["KIRA_SIN_SERIAL"] = "1"

import test_stream as suite  # noqa: E402


async def run() -> None:
    for name in (
        "test_unitarios",
        "test_endpoint_no_muere_mudo",
        "test_herramienta_con_texto",
        "test_stream_vacio_reintenta",
    ):
        function = getattr(suite, name)
        result = function()
        if asyncio.iscoroutine(result):
            await result


if __name__ == "__main__":
    asyncio.run(run())
    print("STREAM DETERMINISTA OK")
