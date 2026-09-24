"""
test_respaldo_ia.py — el respaldo de IA (DeepSeek -> nano-gpt).

Correr:  .venv/bin/python test_respaldo_ia.py    (desde Cerebro/Backend)

Que cubre, SIN red (con un httpx falso que registra a quién le pegó):
  1. Si el principal contesta, el respaldo NO se toca (cero latencia extra)
  2. Si el principal se agota (500/429), el respaldo salva la respuesta
  3. Si el principal se queda SIN SALDO (HTTP 402), salta al respaldo YA
     (sin gastar los 3 reintentos con backoff: eso era esperar al pedo)
  4. Lo mismo en el camino de STREAM (el chat en vivo)
  5. El log avisa: "[IA] ... se agotó (...) -> CAE AL RESPALDO"
"""
import asyncio
import io
import json
import sys
import traceback
from contextlib import redirect_stdout

import httpx

import kira_server as ks

FALLOS = 0


def check(nombre: str, cond: bool, detalle: str = ""):
    global FALLOS
    print(f"  [{'OK ' if cond else 'FALLA'}] {nombre}" + (f"  -> {detalle}" if detalle and not cond else ""))
    if not cond:
        FALLOS += 1


# ---------------------------------------------------------------- httpx falso
class _Respuesta:
    """Respuesta mínima: status + cuerpo + aiter_lines para el stream."""

    def __init__(self, status: int, cuerpo: dict | None = None, lineas: list[str] | None = None):
        self.status_code = status
        self.headers: dict[str, str] = {}
        self._cuerpo = cuerpo if cuerpo is not None else {}
        self._lineas = lineas or []

    def json(self) -> dict:
        return self._cuerpo

    def raise_for_status(self):
        if self.status_code >= 400:
            req = httpx.Request("POST", "http://test/chat/completions")
            resp = httpx.Response(self.status_code, request=req)
            raise httpx.HTTPStatusError(f"HTTP {self.status_code}", request=req, response=resp)

    async def aiter_lines(self):
        for linea in self._lineas:
            yield linea


class _StreamCtx:
    def __init__(self, resp: _Respuesta):
        self._resp = resp

    async def __aenter__(self) -> _Respuesta:
        return self._resp

    async def __aexit__(self, *args) -> bool:
        return False


class _ClienteFalso:
    """Registra cada URL y contesta según el proveedor (por host)."""

    llamadas: list[str] = []
    # host -> status a devolver
    status_por_host: dict[str, int] = {}
    cuerpo_ok = {"choices": [{"message": {"content": '{"emotion":"neutral","message":"ok"}'}}]}
    lineas_ok = [
        'data: {"choices":[{"delta":{"content":"{\\"emotion\\":\\"neutral\\""}}]}',
        'data: {"choices":[{"delta":{"content":",\\"message\\":\\"hola\\"}"}}]}',
        "data: [DONE]",
    ]

    def __init__(self, **kwargs):
        pass

    async def __aenter__(self):
        return self

    async def __aexit__(self, *args):
        return False

    @staticmethod
    def _host(url: str) -> str:
        return url.split("/")[2]

    def _respuesta(self, url: str) -> _Respuesta:
        _ClienteFalso.llamadas.append(url)
        host = self._host(url)
        status = _ClienteFalso.status_por_host.get(host, 200)
        if status == 200:
            return _Respuesta(200, _ClienteFalso.cuerpo_ok, _ClienteFalso.lineas_ok)
        return _Respuesta(status)

    async def post(self, url, headers=None, json=None, **kw) -> _Respuesta:
        return self._respuesta(url)

    def stream(self, method, url, headers=None, json=None, **kw) -> _StreamCtx:
        return _StreamCtx(self._respuesta(url))


# --------------------------------------------------------------------- tests
def preparar(monkeypatch_httpx=True):
    _ClienteFalso.llamadas = []
    _ClienteFalso.status_por_host = {}
    if monkeypatch_httpx:
        ks.httpx.AsyncClient = _ClienteFalso


async def no_dormir(intento: int, respuesta=None):
    """Sin backoff real en los tests (no queremos esperar 8s)."""
    return None


async def test_principal_ok():
    print("\n== 1) el principal contesta: el respaldo NO se toca ==")
    preparar()
    ks.esperar_reintento = no_dormir
    r = await ks.post_json_ia({"model": "x", "messages": [], "max_tokens": 10})
    check("devuelve la respuesta del principal", r == _ClienteFalso.cuerpo_ok, str(r))
    check("le pegó a UN solo proveedor", len(_ClienteFalso.llamadas) == 1, str(_ClienteFalso.llamadas))
    check("y fue a DeepSeek", "api.deepseek.com" in _ClienteFalso.llamadas[0], str(_ClienteFalso.llamadas))


async def test_se_agota_y_entra_respaldo():
    print("\n== 2) el principal se agota (500): entra el respaldo ==")
    preparar()
    ks.esperar_reintento = no_dormir
    _ClienteFalso.status_por_host["api.deepseek.com"] = 500
    salida = io.StringIO()
    with redirect_stdout(salida):
        r = await ks.post_json_ia({"model": "x", "messages": [], "max_tokens": 10})
    log = salida.getvalue()
    check("la respuesta vino del respaldo", r == _ClienteFalso.cuerpo_ok, str(r))
    check("intentó 3 veces el principal (y no más)",
          sum("api.deepseek.com" in u for u in _ClienteFalso.llamadas) == 3, str(_ClienteFalso.llamadas))
    check("después cayó a nano-gpt",
          sum("nano-gpt.com" in u for u in _ClienteFalso.llamadas) == 1, str(_ClienteFalso.llamadas))
    check("el log lo dice con el motivo", "CAE AL RESPALDO" in log and "HTTP 500" in log, log.strip()[:160])
    check("y no se escapa sin dejar rastro en el log", "nano-gpt" in log, log.strip()[:160])


async def test_sin_saldo_salta_al_toque():
    print("\n== 3) sin saldo (HTTP 402): salta al respaldo SIN reintentar ==")
    preparar()
    ks.esperar_reintento = no_dormir
    _ClienteFalso.status_por_host["api.deepseek.com"] = 402
    salida = io.StringIO()
    with redirect_stdout(salida):
        r = await ks.post_json_ia({"model": "x", "messages": [], "max_tokens": 10})
    check("igual hubo respuesta (del respaldo)", r == _ClienteFalso.cuerpo_ok, str(r))
    check("al principal le pegó UNA sola vez (no gastó reintentos)",
          sum("api.deepseek.com" in u for u in _ClienteFalso.llamadas) == 1, str(_ClienteFalso.llamadas))
    check("el motivo en el log es HTTP 402", "HTTP 402" in salida.getvalue(), salida.getvalue()[:160])


async def test_stream_con_respaldo():
    print("\n== 4) STREAM: el chat sigue vivo con el respaldo ==")
    preparar()
    ks.esperar_reintento = no_dormir
    _ClienteFalso.status_por_host["api.deepseek.com"] = 503
    caja: dict = {}
    eventos = []
    salida = io.StringIO()
    with redirect_stdout(salida):
        async for ev in ks._stream_ia([{"role": "user", "content": "hola"}], caja, 0.8):
            eventos.append(ev)
    tipos = [json.loads(e[5:]).get("tipo") for e in eventos if e.startswith("data:")]
    check("hubo deltas (el usuario vio texto)", tipos.count("delta") >= 1, str(tipos))
    check("NO hubo error", "error" not in tipos, str(tipos))
    check("la caja quedó con la salida del respaldo",
          caja.get("salida", {}).get("message") == "hola", str(caja.get("salida")))
    check("se avisó del reintento mientras cambiaba de proveedor",
          "reintento" in tipos and "esperando" in tipos, str(tipos))
    check("el log dice que cayó al respaldo", "CAE AL RESPALDO" in salida.getvalue(), salida.getvalue()[:200])
    check("el respaldo recibió el stream",
          any("nano-gpt.com" in u for u in _ClienteFalso.llamadas), str(_ClienteFalso.llamadas))


async def test_stream_principal_ok():
    print("\n== 5) STREAM normal: no toca el respaldo ==")
    preparar()
    ks.esperar_reintento = no_dormir
    caja: dict = {}
    eventos = [ev async for ev in ks._stream_ia([{"role": "user", "content": "hola"}], caja, 0.8)]
    check("cero llamadas al respaldo",
          not any("nano-gpt.com" in u for u in _ClienteFalso.llamadas), str(_ClienteFalso.llamadas))
    check("hubo deltas", any('"tipo": "delta"' in e for e in eventos))


def test_json_adornado_y_emociones():
    print("\n== 6) el respaldo decora el JSON e inventa emociones (casos REALES) ==")
    # Exactamente lo que devolvió nano-gpt/gemma en la prueba real:
    adornado = '```json\n{\n  "emotion": "confident",\n  "message": "Listo."\n}\n```'
    limpio = ks.limpiar_json_ia(adornado)
    check("el JSON con ```markdown ahora parsea", json.loads(limpio)["message"] == "Listo.", limpio)
    check("y no queda ninguna tilde de markdown", "```" not in limpio, limpio)

    check("JSON normal no se toca",
          ks.limpiar_json_ia('{"emotion":"happy"}') == '{"emotion":"happy"}')
    check("tolera texto alrededor",
          ks.limpiar_json_ia('Claro, acá va: {"a": 1} espero que sirva') == '{"a": 1}')

    check("'confident' del respaldo -> happy (el tag de voz existe)",
          ks.normalizar_emocion("confident") == "happy", ks.normalizar_emocion("confident"))
    check("'feliz' -> happy", ks.normalizar_emocion("feliz") == "happy")
    check("emoción válida pasa igual", ks.normalizar_emocion("fastidio") == "fastidio")
    check("desconocida -> neutral (nunca rompe la cara del micro)",
          ks.normalizar_emocion("eufórico cuántico") == "neutral")
    check("vacío/None -> neutral",
          ks.normalizar_emocion(None) == "neutral" and ks.normalizar_emocion("") == "neutral")
    check("las 8 del contrato se respetan",
          all(ks.normalizar_emocion(e) == e for e in ks.EMOCIONES_VALIDAS))


def main():
    print("=" * 62)
    print("TESTS — respaldo de IA (DeepSeek -> nano-gpt)")
    print("=" * 62)
    original = ks.httpx.AsyncClient
    try:
        asyncio.run(test_principal_ok())
        asyncio.run(test_se_agota_y_entra_respaldo())
        asyncio.run(test_sin_saldo_salta_al_toque())
        asyncio.run(test_stream_con_respaldo())
        asyncio.run(test_stream_principal_ok())
        test_json_adornado_y_emociones()
    except Exception:
        traceback.print_exc()
        global FALLOS
        FALLOS += 1
    finally:
        ks.httpx.AsyncClient = original
    print("\n" + ("TODO OK ✅" if FALLOS == 0 else f"{FALLOS} FALLAS ❌"))
    return 1 if FALLOS else 0


if __name__ == "__main__":
    sys.exit(main())
