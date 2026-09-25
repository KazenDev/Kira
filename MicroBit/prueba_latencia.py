"""
prueba_latencia.py - Mide la REACTIVIDAD de la placa real, en vivo.

Reproduce lo que siente la persona: la IA manda un comando y la cara tiene que
enterarse. Se mide el tiempo entre que sale el comando por el cable y que la
placa responde, barriendo el ciclo de la animacion (que es justo donde
importa: la version vieja tenia una franja sorda de 1,3 s).

Hace DOS mediciones porque hay dos animaciones bloqueantes distintas y no hay
que mezclarlas:

  A) SENSOR:TEMP  -> no cambia la cara. Mide solo el ritmo con que el bucle
                     principal de la animacion chequea el serial. Esta es la
                     cifra que mide el patron de frame.
  B) HAPPY        -> cambia la cara, y eso dispara una TRANSICION. Las
                     transiciones tambien son bloqueantes (y todavia no
                     chequean el serial), asi que esta cifra mezcla las dos.

Uso:  python3 prueba_latencia.py
"""
import serial, time, sys

PUERTO = "/dev/ttyACM0"

def asentar(s):
    """Deja la placa quieta: drena el diluvio de frames LED."""
    time.sleep(1.0)
    s.reset_input_buffer()
    for _ in range(30):
        s.read(300)
    s.reset_input_buffer()

def medir_una(s, cmd, ventana=1.2):
    """Manda un comando y cronometra cuanto tarda la placa en responder."""
    s.reset_input_buffer()
    t0 = time.time()
    s.write(cmd)
    while time.time() - t0 < ventana:
        linea = s.readline()
        if linea.startswith(b"ACK:"):
            return (time.time() - t0) * 1000.0
    return None

def barrido(s, cmd, muestras, pausa, ventana=1.2):
    """Barre el ciclo de la animacion mandando comandos espaciados."""
    latencias, perdidas = [], 0
    for _ in range(muestras):
        ms = medir_una(s, cmd, ventana)
        if ms is None:
            perdidas += 1
        else:
            latencias.append(ms)
        time.sleep(pausa)
    return latencias, perdidas

def mostrar(nombre, latencias, perdidas, muestras):
    print(f"\n  {nombre}")
    if not latencias:
        print("    no se recibio ningun ACK")
        return None
    latencias.sort()
    print(f"    ACKs recibidos : {len(latencias)}/{muestras}  (perdidos {perdidas})")
    print(f"    latencia media : {sum(latencias)/len(latencias):7.1f} ms")
    print(f"    p95            : {latencias[int(len(latencias)*0.95)-1]:7.1f} ms")
    print(f"    PEOR latencia  : {max(latencias):7.1f} ms")
    return max(latencias)

def main():
    s = serial.Serial(PUERTO, 115200, timeout=0.05)
    asentar(s)

    # --- A) comando que NO cambia la cara: mide solo el bucle de animacion ---
    lat_a, perd_a = barrido(s, b"SENSOR:TEMP\n", 24, 0.25)
    peor_a = mostrar("A) SENSOR:TEMP (no cambia la cara)", lat_a, perd_a, 24)

    # --- B) comando que cambia la cara: suma el coste de la transicion ---
    s.write(b"HAPPY\n")
    time.sleep(1.5)
    asentar(s)
    lat_b, perd_b = barrido(s, b"HAPPY\n", 16, 0.8)
    peor_b = mostrar("B) HAPPY (cambia la cara -> dispara transicion)", lat_b, perd_b, 16)

    s.close()

    print()
    if peor_a is not None and peor_a < 60:
        print("  OK en A: la animacion se entera en el mismo frame (~16 ms).")
        print("           La version vieja tenia hasta 1290 ms de franja sorda.")
    else:
        print(f"  ATENCION en A: {peor_a} ms es mucho para un frame de 16 ms.")

    if peor_b is not None and peor_b > 100:
        print(f"  A notar en B: {peor_b:.0f} ms. La TRANSICION tambien es bloqueante")
        print("              y no chequea el serial: es la siguiente candidata.")
    return 0

sys.exit(main())
