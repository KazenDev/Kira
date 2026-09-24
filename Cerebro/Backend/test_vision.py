"""
test_vision.py — LA FOTO DEL CELULAR LLEGA A LOS OJOS DE LA IA.

Correr:  .venv/bin/python test_vision.py                    (incluye 1 llamada real)
         .venv/bin/python test_vision.py --solo-unitarios   (sin gastar IA)

Que cubre:
  1. normalizar_foto(): lo que el celular manda (data URL base64) se valida y
     se limpia; una foto rara NUNCA rompe la charla (se ignora y sigue).
  2. mensaje_con_foto(): el turno con imagen es el content-ARRAY de la API
     (texto + image_url con detail high) y el texto lleva el aviso que evita
     el "no puedo ver imágenes".
  3. El ENDPOINT: un turno con foto y SIN texto es válido (antes daba 400), la
     foto va SOLO en el mensaje nuevo (el historial queda en texto: si no, se
     reenviarían megabytes de base64 en cada turno) y cierra con 'fin'.
  4. INTEGRACION real: un PNG con un "7" enorme dibujado a mano (sin PIL) se
     manda de verdad a DeepSeek y se verifica que LO LEA.
"""
import asyncio
import base64
import json
import struct
import sys
import traceback
import zlib

import httpx

import kira_server as ks

FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    estado = "OK " if cond else "FALLA"
    if not cond:
        FALLOS += 1
    print(f"  [{estado}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))


JPG = "data:image/jpeg;base64," + base64.b64encode(b"\xff\xd8\xff" + b"x" * 400).decode()
PNG = "data:image/png;base64," + base64.b64encode(b"\x89PNG" + b"y" * 400).decode()


# --------------------------------------------------------------------------
#  PNG a mano (sin PIL): fondo blanco y un "7" negro gigante para que la IA
#  tenga algo INEQUIVOCO que leer. Es el equivalente a sacarle una foto a un
#  papel con un número escrito.
# --------------------------------------------------------------------------
def _chunk(tipo: bytes, datos: bytes) -> bytes:
    return (struct.pack(">I", len(datos)) + tipo + datos
            + struct.pack(">I", zlib.crc32(tipo + datos) & 0xFFFFFFFF))


def png_del_siete(ancho: int = 220, alto: int = 220) -> bytes:
    def negro(x: int, y: int) -> bool:
        if 30 <= y <= 62 and 24 <= x <= ancho - 24:      # barra de arriba
            return True
        if 62 < y <= alto - 18:                          # diagonal
            centro = (ancho - 34) - (y - 62) * ((ancho - 74) / (alto - 80))
            return abs(x - centro) <= 16
        return False

    filas = bytearray()
    for y in range(alto):
        filas.append(0)  # filtro PNG: none
        for x in range(ancho):
            filas += b"\x00\x00\x00" if negro(x, y) else b"\xff\xff\xff"
    cabecera = struct.pack(">IIBBBBB", ancho, alto, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n"
            + _chunk(b"IHDR", cabecera)
            + _chunk(b"IDAT", zlib.compress(bytes(filas)))
            + _chunk(b"IEND", b""))


# ==========================================================================
#  1) normalizar_foto: la foto que llega del celular
# ==========================================================================
def test_unitarios():
    print("\n== 1) normalizar_foto: basura entra, NADA rompe la charla ==")

    for malo, caso in [(None, "None"), ("", "texto vacio"), ("hola", "no es data URL"),
                       ("data:image/jpeg,AAAA", "sin ;base64"),
                       ("data:image/svg+xml;base64," + "A" * 400, "formato no soportado"),
                       ("data:image/jpeg;base64,AA", "imagen vacia"),
                       ("data:image/png;base64," + "A" * (13 * 1024 * 1024), "demasiado grande"),
                       ({}, "dict vacio")]:
        check(f"rechaza {caso}", ks.normalizar_foto(malo) is None)

    check("acepta JPEG del celular", ks.normalizar_foto(JPG) == JPG)
    check("acepta PNG", ks.normalizar_foto(PNG) == PNG)
    check("acepta el formato dict {datos}",
          ks.normalizar_foto({"datos": JPG}) == JPG)
    check("normaliza espacios y mayusculas del mime",
          ks.normalizar_foto("  data:IMAGE/JPEG;base64," + JPG.split("base64,")[1]) is not None)

    print("\n== 2) mensaje_con_foto: el turno que ve la IA ==")
    check("sin foto -> texto plano (como siempre)",
          ks.mensaje_con_foto("hola", None) == "hola")
    check("sin foto y sin texto -> cadena vacia",
          ks.mensaje_con_foto("", None) == "")

    contenido = ks.mensaje_con_foto("", JPG)
    check("con foto -> lista de bloques (content array)", isinstance(contenido, list), str(type(contenido)))
    check("primer bloque: el texto", bool(contenido) and contenido[0].get("type") == "text")
    check("segundo bloque: la imagen", len(contenido) == 2 and contenido[1].get("type") == "image_url")
    check("la imagen va como data URL con detail high",
          contenido[1]["image_url"]["url"] == JPG and contenido[1]["image_url"]["detail"] == "high")
    texto = contenido[0]["text"]
    check("sin texto, igual le dice que mire la foto", "Mir" in texto, texto[:80])
    check("el aviso PROHIBE el 'no puedo ver imagenes'", "PROHIBIDO" in texto, texto[:120])
    check("usa la palabra FOTO para que el modelo sepa que hay imagen", "[FOTO]" in texto)

    con_texto = ks.mensaje_con_foto("¿qué ves acá?", PNG)
    check("el texto del usuario se conserva", con_texto[0]["text"].startswith("¿qué ves acá?"))
    check("con foto y con texto -> igual son 2 bloques (texto + imagen)", len(con_texto) == 2)


# ==========================================================================
#  3) El endpoint con foto (IA stubbeada: sin red, determinista)
# ==========================================================================
async def test_endpoint_con_foto():
    print("\n== 3) el endpoint con una FOTO real ==")

    originales = (ks._stream_ia, ks.memoria_recuerdos_bloque, ks.memoria_memo_post_charla,
                  ks.cargar_personaje)
    capturado: list[list] = []

    async def stream_falso(messages, caja, temperatura):
        capturado.append(messages)
        cuerpo = {"emotion": "surprised", "message": "¡veo un siete!", "tool": None}
        caja["buffer"] = json.dumps(cuerpo, ensure_ascii=False)
        caja["salida"] = cuerpo
        return
        yield  # async generator

    async def sin_recuerdos(*a, **k):
        return ""

    async def memo_sin_efecto(*a, **k):
        return None

    def pj_sin_voz(nombre):
        p = originales[3](nombre)
        p["voice_id"] = "PENDIENTE"   # sin TTS: el test no toca la red
        return p

    ks._stream_ia = stream_falso
    ks.memoria_recuerdos_bloque = sin_recuerdos
    ks.memoria_memo_post_charla = memo_sin_efecto
    ks.cargar_personaje = pj_sin_voz
    try:
        transport = httpx.ASGITransport(app=ks.app)
        async with httpx.AsyncClient(transport=transport, base_url="http://test") as cliente:
            # (a) foto SIN texto: antes esto era un 400 "mensaje vacio"
            async with cliente.stream(
                "POST", "/api/chat/stream",
                json={"personaje": "kira", "mensaje": "", "imagen": JPG,
                      "historia": [{"rol": "user", "contenido": "hola"},
                                   {"rol": "assistant", "contenido": "hola!"}]},
            ) as r:
                crudo = "".join([p async for p in r.aiter_text()])
                estado_http = r.status_code
            # (b) foto CON texto
            async with cliente.stream(
                "POST", "/api/chat/stream",
                json={"personaje": "kira", "mensaje": "mirá esto", "imagen": PNG, "historia": []},
            ) as r2:
                crudo2 = "".join([p async for p in r2.aiter_text()])
            # (c) ni texto ni foto: sigue siendo un error del cliente
            r3 = await cliente.post("/api/chat/stream",
                                    json={"personaje": "kira", "mensaje": "", "historia": []})

        check("foto sin texto -> 200 (ya no es 'mensaje vacio')", estado_http == 200, f"HTTP {estado_http}")
        check("cerró con 'fin'", '"tipo": "fin"' in crudo, crudo[:200])
        check("el mensaje final llega al frontend", "siete" in crudo, crudo[-300:])
        check("sin texto ni foto -> 400", r3.status_code == 400, f"HTTP {r3.status_code}")

        msgs_a = capturado[0]
        ultimo = msgs_a[-1]
        check("la IA recibe el turno con la foto como content array",
              isinstance(ultimo.get("content"), list), str(type(ultimo.get("content"))))
        check("la imagen va en el bloque image_url",
              bool(ultimo.get("content")) and ultimo["content"][1].get("type") == "image_url")
        check("y es EXACTAMENTE la foto que mandó el celular",
              ultimo["content"][1]["image_url"]["url"] == JPG)
        check("la imagen solo se manda en el turno NUEVO (el historial queda en texto)",
              all(isinstance(m.get("content"), str) for m in msgs_a[:-1]))
        check("el system prompt sigue siendo texto",
              isinstance(msgs_a[0].get("content"), str))

        msgs_b = capturado[1]
        check("foto + texto: el texto del usuario no se pierde",
              msgs_b[-1]["content"][0]["text"].startswith("mirá esto"))
    finally:
        (ks._stream_ia, ks.memoria_recuerdos_bloque, ks.memoria_memo_post_charla,
         ks.cargar_personaje) = originales


# ==========================================================================
#  4) INTEGRACION real: DeepSeek mira el PNG del "7"
# ==========================================================================
async def test_integracion():
    print("\n== 4) INTEGRACION real: le mandamos un '7' dibujado a DeepSeek ==")

    png = png_del_siete()
    data_url = "data:image/png;base64," + base64.b64encode(png).decode()
    print(f"        PNG de prueba: {len(png)} bytes")

    originales = (ks.memoria_memo_post_charla, ks.cargar_personaje)

    async def memo_sin_efecto(*a, **k):
        # que la charla de prueba no quede en la memoria de Kira
        return None

    def pj_sin_voz(nombre):
        p = originales[1](nombre)
        p["voice_id"] = "PENDIENTE"
        return p

    ks.memoria_memo_post_charla = memo_sin_efecto
    ks.cargar_personaje = pj_sin_voz
    try:
        transport = httpx.ASGITransport(app=ks.app)
        async with httpx.AsyncClient(transport=transport, base_url="http://test", timeout=120) as cliente:
            async with cliente.stream(
                "POST", "/api/chat/stream",
                json={"personaje": "kira", "imagen": data_url,
                      "mensaje": "¿Qué número grande se ve en esta foto? Decímelo con tus palabras.",
                      "historia": []},
            ) as r:
                crudo = "".join([p async for p in r.aiter_text()])

        check("NO hubo error y cerró con 'fin'",
              '"tipo": "fin"' in crudo and '"tipo": "error"' not in crudo, crudo[:300])
        fin = [l for l in crudo.splitlines() if '"tipo": "fin"' in l]
        mensaje = ""
        if fin:
            mensaje = json.loads(fin[0][5:]).get("message", "")
            print(f"        respuesta de la IA: {mensaje!r}")
        check("la IA LEYÓ el 7 de la imagen",
              "siete" in mensaje.lower() or "7" in mensaje, mensaje)
    finally:
        ks.memoria_memo_post_charla, ks.cargar_personaje = originales


def main():
    print("=" * 62)
    print("TESTS — visión: la foto del celular llega a la IA")
    print("=" * 62)
    try:
        test_unitarios()
        asyncio.run(test_endpoint_con_foto())
        if "--solo-unitarios" in sys.argv:
            print("\n(integracion salteada)")
        else:
            asyncio.run(test_integracion())
    except Exception:
        traceback.print_exc()
        global FALLOS
        FALLOS += 1
    print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"{FALLOS} FALLAS ❌"))


if __name__ == "__main__":
    main()
