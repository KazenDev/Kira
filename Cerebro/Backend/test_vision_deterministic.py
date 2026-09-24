"""Wrapper determinista de visión: no llama proveedores reales."""

from __future__ import annotations

import asyncio
import os

os.environ["KIRA_SIN_SERIAL"] = "1"

import test_vision as suite  # noqa: E402


async def run() -> None:
    for name in ("test_unitarios", "test_endpoint_con_foto"):
        function = getattr(suite, name)
        result = function()
        if asyncio.iscoroutine(result):
            await result


if __name__ == "__main__":
    asyncio.run(run())
    print("VISION DETERMINISTA OK")
