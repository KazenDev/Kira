"""
prueba_rx.py - Prueba en vivo del fix de basura/contaminacion en el UART.

Reproduce el sintoma original:

    STOP -> b'ACK:^\\\\\\xf7\\xf7\\xf7\\x94\\xffSTOP\\n'     (ACK contaminado)

y verifica los tres casos:
  1) comando limpio         -> ACK limpio
  2) parcial sin "\\n"       -> se descarta solo a los 2 s, y el comando
                               siguiente entra limpio
  3) linea con bytes de control terminada en "\\n" -> se limpia, avisa, y el
                               ACK sale limpio
  4) byte NUL               -> quirk del TRANSPORTE (2 bytes fantasma que se
                               tragan el comando siguiente). El firmware no los
                               ve. Lo que se verifica es que el estado
                               envenenado se cure solo a los 2 s.

Uso:  python3 prueba_rx.py
"""
import serial, time, sys

PUERTO = "/dev/ttyACM0"

def abrir():
    s = serial.Serial(PUERTO, 115200, timeout=0.3)
    # La placa tarda en bootear y manda un diluvio de frames LED. Si se le
    # escribe antes de que se asiente, el ACK se pierde por timing y el test
    # miente. Se espera a que se calle.
    time.sleep(2.0)
    leer_por(s, 1.0)
    s.reset_input_buffer()
    s.reset_output_buffer()
    return s

def leer_por(s, plazo):
    """Lee por TIEMPO, no por cantidad.

    Importante: la placa retransmite el frame LED real por serial a ~20fps
    mientras la cara se mueve. Un s.read(N) bloquea hasta juntar N bytes o
    hasta que el puerto se calle, asi que con la placa animandose NUNCA
    devuelve a tiempo y el test miente. Hay que leer por deadline.
    """
    buf = ""
    t0 = time.time()
    while time.time() - t0 < plazo:
        buf += s.read(256).decode("latin-1")
    return buf

def mandar(s, datos, plazo=1.5):
    s.reset_input_buffer()
    s.write(datos)
    return leer_por(s, plazo)

def lineas(texto):
    return [x.strip() for x in texto.split("\n") if x.strip()]

ok = True
s = abrir()

# --- 1) comando limpio ---------------------------------------------------
print("1) comando limpio")
r = mandar(s, b"STOP\n")
acks = [l for l in lineas(r) if l.startswith("ACK")]
print("     ", acks)
if acks == ["ACK:STOP"]:
    print("      OK: ACK limpio")
else:
    print("      FALLO: se esperaba exactamente ['ACK:STOP']")
    ok = False

# --- 2) parcial sin delimitador (el bug original) ------------------------
# En el firmware viejo estos bytes se quedaban en el buffer PARA SIEMPRE y se
# pegaban al comando siguiente. Ahora rxDescartarSiVencio() los tira a los 2 s.
# OJO: la basura NO lleva NUL. Un 0x00 en el cable dispara el quirk de
# transporte del caso 4 y hace este test intermitente.
print("\n2) parcial sin \\n (3 bytes de basura), se espera el corte de 2 s...")
s.reset_input_buffer()
s.write(b"\xff\xfe\x80")
buf = leer_por(s, 4.0)
desc = [l for l in lineas(buf) if l.startswith("RX:PARCIAL")]
print("     ", desc if desc else "NINGUNO")
if not desc:
    print("      FALLO: la basura no se descarto")
    ok = False
else:
    print("      OK: el parcial se descarto solo")

r = mandar(s, b"HAPPY\n")
acks = [l for l in lineas(r) if l.startswith("ACK")]
print("      comando siguiente ->", acks)
if acks == ["ACK:HAPPY"]:
    print("      OK: el comando siguiente entro limpio")
else:
    print("      FALLO: el comando siguiente quedo contaminado")
    ok = False

# --- 3) linea con bytes de control TERMINADA en \n -----------------------
print("\n3) linea con bytes de control terminada en \\n")
# 0x02 y 0x1B (ESC) son de control y deben desaparecer. Lo que queda tiene
# que ser exactamente STOP. (El NUL queda fuera: ver caso 4.)
r = mandar(s, b"\x02STO\x1bP\n")
todo = lineas(r)
acks = [l for l in todo if l.startswith("ACK")]
basura = [l for l in todo if l.startswith("RX:BASURA")]
print("     ", todo)
if not basura:
    print("      FALLO: no aviso que la linea traia basura")
    ok = False
elif acks != ["ACK:STOP"]:
    print("      FALLO: se esperaba ACK:STOP, vino", acks)
    ok = False
else:
    print("      OK: aviso la basura y entrego ACK:STOP limpio")

# --- 4) NUL: quirk de transporte + AUTOCURADO ---------------------------
# Un byte NUL (0x00) NO es problema del firmware: el transporte entrega 2
# bytes fantasma y se traga el comando siguiente. El firmware no los ve, asi
# que no hay nada que sanear. Lo que SI se verifica aca es que el estado
# envenenado sea AUTOCURABLE: antes los bytes se quedaban en el buffer para
# siempre y contaminaban todos los comandos. Ahora rxDescartarSiVencio() los
# tira a los 2 s y el comando siguiente entra limpio.
print("\n4) NUL (quirk de transporte) y autocurado")
s.reset_input_buffer()
s.write(b"\x00STOP\n")
buf = leer_por(s, 1.5)
perdido = [x for x in lineas(buf) if x.startswith("ACK")]
print("      comando tras el NUL ->", perdido if perdido else "se perdio (esperado: quirk del transporte)")
if perdido:
    print("      NOTA: el transporte ya no traga el comando, el fix podria revisarse")
print("      esperando 3 s (el corte de 2 s)...")
time.sleep(3.0)
r = mandar(s, b"HAPPY\n")
acks = [l for l in lineas(r) if l.startswith("ACK")]
print("      comando siguiente ->", acks)
if acks == ["ACK:HAPPY"]:
    print("      OK: el estado envenenado se cura solo en 2 s")
else:
    print("      FALLO: no se autocuro (quedaba envenenado para siempre)")
    ok = False

s.close()
print("\n" + ("TODO OK" if ok else "HUBO FALLOS"))
sys.exit(0 if ok else 1)
