#!/usr/bin/env python3
# ============================================================================
#  mb.py - Consola directa al micro:bit (KIRA & KIRO)
#
#  Manda comandos por el puerto serial (o por la web si ya esta prendida)
#  para ejecutar animaciones cuando TU quieras y pararlas cuando TU quieras.
#
#  Uso:
#    python3 mb.py                 -> modo interactivo (repl mb>)
#    python3 mb.py happy           -> una animacion y sale (queda en bucle!)
#    python3 mb.py load3           -> patron de carga especifico
#    python3 mb.py stop            -> corta lo que este sonando
#    python3 mb.py --serial        -> fuerza el puerto serial directo
#    python3 mb.py --api           -> fuerza pasar por la web (127.0.0.1:8000)
#    python3 mb.py --puerto /dev/ttyACM1
#
#  Dentro del repl escribe 'ayuda' para ver todo.
#
#  NOTA: si la web esta prendida, los comandos van POR LA WEB (los dos
#  conviven felices). Si apuntas directo al serial con la web prendida,
#  los ACK se reparten entre procesos y se hace un desorden.
# ============================================================================
import argparse
import json
import sys
import time

API = "http://127.0.0.1:8000"
USB_ID = "0d28:0204"   # micro:bit v2 (para usbreset)

# nombre -> comando serial que entiende el firmware
COMANDOS = {
    # caritas (quedan puestas hasta que mandes otra cosa)
    "happy": "HAPPY", "alegria": "HAPPY",
    "sad": "SAD", "triste": "SAD",
    "angry": "ANGRY", "enojado": "ANGRY",
    "surprised": "SURPRISED", "sorprendido": "SURPRISED",
    "neutral": "NEUTRAL",
    "fastidio": "FASTIDIO", "annoyed": "FASTIDIO",
    "miedo": "MIEDO", "scared": "MIEDO",
    "cansado": "CANSADO", "tired": "CANSADO",
    # sensores onboard
    "temp": "SENSOR:TEMP", "temperatura": "SENSOR:TEMP",
    "luz": "SENSOR:LUZ", "light": "SENSOR:LUZ",
    "boton": "SENSOR:BOTON", "botones": "SENSOR:BOTON",
    "movimiento": "SENSOR:ACCEL", "acel": "SENSOR:ACCEL",
    "sonido": "SENSOR:MIC", "microfono": "SENSOR:MIC",
    "bateria": "SENSOR:BAT", "battery": "SENSOR:BAT",
    # sistema
    "talk": "TALK",
    "loading": "LOADING",
    "loadall": "LOADALL",
    "voz": "VOZ",
    "test": "TEST", "demo": "TEST",
    "trans": "TRANS",
    "transall": "TRANSALL",
    "blink": "BLINK",
    "calib": "CALIB",
    "metrostop": "METRO:STOP", "metrooff": "METRO:STOP",
    # cortar todo
    "stop": "STOP", "parar": "STOP",
    "cancelar": "CANCELAR", "cancel": "CANCELAR",
}

AYUDA = """\
CARITAS (quedan puestas hasta que cambies):   happy sad angry surprised
                                              neutral fastidio miedo cansado
EN BUCLE (hasta que mandes stop o otra cosa):
  talk              la boca habla
  loading           patron de carga al azar
  load0 .. load9    patron de carga especifico
  voz   / voz0-4    aro que reacciona al microfono
  METRO:120:4       metronomo: bpm:acento (0 = sin acento)
  metrostop         corta el metronomo (botones: A -5 | B +5 | A+B para)
  test              demo automatica de todas las emociones
  trans / transall / trans0-2     transiciones de prueba
SENSORES (lectura; no cambian la emocion): temp luz boton accel sonido bateria
CONTROL:  stop (corta y vuelve a alegria) | cancelar (descarta la escucha) | salir | ayuda
OTRO:     cualquier comando en MAYUSCULAS va tal cual al micro:bit"""


def detectar_puerto():
    """Mismo criterio que el server: busca el micro:bit en Linux/Windows."""
    try:
        import serial.tools.list_ports
        for p in serial.tools.list_ports.comports():
            d = (p.description or "").lower()
            if "micro" in d or "mbed" in d or "acm" in d:
                return p.device
    except Exception:
        pass
    import os
    for d in ("/dev/ttyACM0", "/dev/ttyACM1"):
        if os.path.exists(d):
            return d
    return None


class CanalSerial:
    """Abre el puerto serial DIRECTAMENTE (usar solo con la web apagada)."""

    def __init__(self, puerto=None, baud=115200):
        import serial
        self.puerto = puerto or detectar_puerto()
        if not self.puerto:
            sys.exit("[mb] no encontro el micro:bit (proba --puerto /dev/ttyACM0)")
        try:
            self.ser = serial.Serial(self.puerto, baud, timeout=0.1)
        except PermissionError:
            sys.exit(f"[mb] sin permisos para {self.puerto}\n"
                     "     -> sudo usermod -aG dialout $USER (y re-login),"
                     " o corre con sudo")
        except Exception as e:
            sys.exit(f"[mb] no pude abrir {self.puerto}: {e}")
        time.sleep(2)  # el micro:bit se reinicia al abrir el puerto
        self.buf = b""
        print(f"[mb] serial directo en {self.puerto}")

    def enviar(self, cmd):
        self.ser.write((cmd + "\n").encode())
        fin = time.time() + 2.5
        frames_led = False
        respuesta = None
        while time.time() < fin:
            datos = self.ser.read(512)
            if datos:
                self.buf += datos
                while b"\n" in self.buf:
                    linea, self.buf = self.buf.split(b"\n", 1)
                    texto = linea.decode(errors="replace").strip()
                    if not texto:
                        continue
                    if texto.startswith("LED:"):
                        frames_led = True
                        continue
                    print(f"  << {texto}")
                    if respuesta is None:
                        respuesta = texto
        if respuesta is None and frames_led:
            # sintoma clasico (ver MICROBIT.md): transmite LEDs pero no ACK
            print("  !! transmite frames pero no responde ACK ->")
            print("     desconecta/conecta el cable, aprieta el boton reset")
            print(f"     de atras, o corre: usbreset {USB_ID}")
        return respuesta

    def cerrar(self):
        try:
            self.ser.close()
        except Exception:
            pass


class CanalApi:
    """Pasa los comandos por la web (convive con el server, recomendado)."""

    def __init__(self):
        import urllib.request
        self._urllib = urllib.request

    def _post(self, ruta, data):
        req = self._urllib.Request(
            API + ruta,
            data=json.dumps(data).encode(),
            headers={"Content-Type": "application/json"},
        )
        with self._urllib.urlopen(req, timeout=4) as r:
            return json.loads(r.read().decode())

    def enviar(self, cmd):
        try:
            self._post("/api/comando", {"cmd": cmd})
            # los sensores contestan por otro lado; leemos el ultimo ack
            if cmd.startswith("SENSOR"):
                time.sleep(0.8)
                with self._urllib.urlopen(API + "/api/status", timeout=4) as r:
                    st = json.loads(r.read().decode())
                ack = st.get("microbit", {}).get("ultimo_ack") or ""
                if ack:
                    print(f"  << {ack}")
            else:
                print("  >> enviado por la web")
        except Exception as e:
            print(f"[mb] la web no contesta ({e}); usa --serial")

    def cerrar(self):
        pass


def normalizar(txt):
    """'alegria' -> HAPPY | 'load3' -> LOAD3 | 'SENSOR:TEMP' -> tal cual."""
    txt = txt.strip()
    if not txt:
        return None
    if "_" in txt or txt == txt.upper() and any(c.isalpha() for c in txt):
        return txt.upper().replace("_", ":")
    low = txt.lower()
    if low in COMANDOS:
        return COMANDOS[low]
    # variantes con numero: load3, voz2, trans1
    for pref in ("load", "voz", "trans"):
        if low.startswith(pref) and low[len(pref):].isdigit():
            return low.upper()
    return txt.upper()


def main():
    ap = argparse.ArgumentParser(description="Consola directa al micro:bit")
    ap.add_argument("comando", nargs="*", help="comando unico (happy, load3, stop...)")
    ap.add_argument("--serial", action="store_true", help="forzar puerto serial directo")
    ap.add_argument("--api", action="store_true", help="forzar via web 127.0.0.1:8000")
    ap.add_argument("--puerto", help="/dev/ttyACM0, COM3, etc")
    args = ap.parse_args()

    canal = None
    if not args.serial:
        try:
            import urllib.request
            with urllib.request.urlopen(API + "/api/status", timeout=1):
                canal = CanalApi()
            print("[mb] web detectada: los comandos pasan por ahi (conviven)")
        except Exception:
            pass
    if canal is None:
        canal = CanalSerial(args.puerto)

    # ---- modo comando unico ----
    if args.comando:
        cmd = normalizar(" ".join(args.comando))
        if cmd:
            print(f">> {cmd}")
            canal.enviar(cmd)
        canal.cerrar()
        return

    # ---- modo interactivo ----
    print("=" * 62)
    print("  Consola del micro:bit - escribi 'ayuda' o 'salir'")
    print("  Las animaciones en bucle corren HASTA QUE tu digas stop.")
    print("=" * 62)
    try:
        while True:
            try:
                txt = input("mb> ").strip()
            except EOFError:
                break
            if not txt:
                continue
            if txt.lower() in ("salir", "exit", "q", "quit"):
                break
            if txt.lower() in ("ayuda", "help", "?"):
                print(AYUDA)
                continue
            cmd = normalizar(txt)
            if not cmd:
                continue
            print(f">> {cmd}")
            canal.enviar(cmd)
    except KeyboardInterrupt:
        print()
    finally:
        canal.cerrar()
        print("chau!")


if __name__ == "__main__":
    main()
