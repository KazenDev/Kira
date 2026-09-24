"""diag_stream.py — reproduce la llamada que falló y muestra los eventos SSE.

Correr: .venv/bin/python diag_stream.py "mensaje a probar"
"""
import asyncio
import json
import sys

import httpx


async def main(mensaje: str):
    url = "http://127.0.0.1:8000/api/chat/stream"
    cuerpo = {"personaje": "kira", "mensaje": mensaje, "historia": []}
    print(f"POST {url}\n  mensaje: {mensaje!r}\n" + "-" * 60)
    tipos: dict[str, int] = {}
    fin = None
    error = None
    texto = []
    async with httpx.AsyncClient(timeout=120) as client:
        async with client.stream("POST", url, json=cuerpo) as r:
            async for linea in r.aiter_lines():
                if not linea.startswith("data: "):
                    continue
                ev = json.loads(linea[6:])
                t = ev.get("tipo", "?")
                tipos[t] = tipos.get(t, 0) + 1
                if t == "delta":
                    texto.append(ev.get("texto", ""))
                elif t == "fin":
                    fin = ev
                elif t == "error":
                    error = ev.get("mensaje")
                elif t in ("reintento", "esperando"):
                    print(f"  [{t}] {ev}")
    print("-" * 60)
    print("eventos:", tipos)
    if error:
        print("ERROR:", error)
    if fin:
        print("fin.emotion:", fin.get("emotion"))
        print("fin.message:", (fin.get("message") or "")[:300])
    concatenado = "".join(texto)
    print("texto streameado (últimos 300):", concatenado[-300:] if concatenado else "(vacío)")
    if not fin and not error:
        print("SIN fin NI error: el stream se cortó sin cerrar")


if __name__ == "__main__":
    msg = sys.argv[1] if len(sys.argv) > 1 else "Dame un codigo de python en ejemplo"
    asyncio.run(main(msg))
