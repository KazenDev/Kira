"""Strong references and lifecycle control for background asyncio tasks."""

from __future__ import annotations

import asyncio
from collections.abc import Coroutine
from typing import Any


class BackgroundTasks:
    def __init__(self) -> None:
        self._tasks: set[asyncio.Task[Any]] = set()

    def spawn(self, name: str, coroutine: Coroutine[Any, Any, Any]) -> asyncio.Task[Any]:
        task = asyncio.create_task(coroutine, name=name)
        self._tasks.add(task)

        def finished(done: asyncio.Task[Any]) -> None:
            self._tasks.discard(done)
            if done.cancelled():
                return
            error = done.exception()
            if error is not None:
                print(f"[BACKGROUND/{name}] fallo: {type(error).__name__}: {error}")

        task.add_done_callback(finished)
        return task

    async def cancel_all(self) -> None:
        tasks = list(self._tasks)
        for task in tasks:
            task.cancel()
        if tasks:
            await asyncio.gather(*tasks, return_exceptions=True)
        self._tasks.clear()
