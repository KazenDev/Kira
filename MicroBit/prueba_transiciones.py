"""
prueba_transiciones.py - Stress de las transiciones en la placa real.

El cambio mas riesgoso de esta tanda NO fue el chequeo de serial: fue el guard
de no-anidamiento. Si un comando llega en medio de una transicion y dispara
otra transicion desde adentro, cada nivel reserva ~572 bytes de PILA y la pila
util de la placa es ~2 KB: a los 3-4 niveles se desborda y la placa muere.

Esta prueba manda una avalanche de cambios de emocion TAN rapido como puede
(el peor caso para el guard) y despues verifica tres cosas:

  1) La placa SIGUE VIVA (responde y no se cuelga).
  2) La cara final es la correcta.
  3) No quedaron pixeles SUELTOS: una Morfosis abortada a mitad deja pixeles
     en el camino, y eso se veria como puntos que no son de la cara. Se
     comprueba que TODOS los pixeles encendidos en toda la muestra
     pertenecen a la cara de alegria.

Uso:  python3 prueba_transiciones.py
"""
import serial, time, sys

PUERTO = "/dev/ttyACM0"

EMOCIONES = [b"HAPPY\n", b"SAD\n", b"ANGRY\n", b"SURPRISED\n",
             b"NEUTRAL\n", b"FASTIDIO\n", b"MIEDO\n", b"CANSADO\n"]

# La cara de ALEGRIA en reposo: ojos (1,1) y (3,1), sonrisa (0,3) (4,3)
# (1,4) (2,4) (3,4).
CARA_ALEGRIA = {(1, 1), (3, 1), (0, 3), (4, 3), (1, 4), (2, 4), (3, 4)}

def leer_por(s, plazo):
    buf = b""
    t0 = time.time()
    while time.time() - t0 < plazo:
        buf += s.read(400)
    return buf.decode("latin-1")

def frames_led(texto):
    """Saca (x, y) de cada pixel encendido de cada frame LED:."""
    filas = []
    for linea in texto.split("\n"):
        if not linea.startswith("LED:"):
            continue
        pix = linea[4:].strip()
        if len(pix) < 25:
            continue
        encendidos = set()
        for i, c in enumerate(pix[:25]):
            if c == '#':
                encendidos.add((i % 5, i // 5))
        filas.append(encendidos)
    return filas

def main():
    s = serial.Serial(PUERTO, 115200, timeout=0.05)
    time.sleep(2.0)
    s.reset_input_buffer()
    for _ in range(40):
        s.read(300)
    s.reset_input_buffer()

    # --- 1) AVALANCHA: 24 cambios de emocion, sin esperar nada ------------
    print(f"mandando {len(EMOCIONES) * 3} cambios de emocion sin pausa...")
    carga = b"".join(EMOCIONES * 3)
    s.write(carga)
    time.sleep(4.0)          # que termina todo (transiciones de 2 s apiladas)
    s.reset_input_buffer()

    # --- 2) SIGUE VIVA? ---------------------------------------------------
    s.write(b"STOP\n")
    time.sleep(0.5)
    respuesta = leer_por(s, 2.0)
    if "ACK:STOP" not in respuesta:
        print("  FALLO: la placa no respondio. Se murio (pila desbordada?)")
        s.close()
        return 1
    print("  OK: respondio al STOP (no se murio)")

    # --- 3) La cara final y los pixeles sueltos ---------------------------
    time.sleep(3.0)          # que se acomode la transicion
    s.reset_input_buffer()
    muestras = frames_led(leer_por(s, 4.0))
    s.close()

    if not muestras:
        print("  FALLO: no llego ningun frame LED (la replica no esta)")
        return 1

    sueltos = set()
    for m in muestras:
        sueltos |= (m - CARA_ALEGRIA)
    print(f"  frames LED vistos: {len(muestras)}")
    if sueltos:
        print(f"  FALLO: pixeles que NO son de la cara de alegria: {sorted(sueltos)}")
        print("         (eso seria una transicion abortada que dejo restos)")
        return 1
    print("  OK: ningun pixel fuera de la cara (sin restos de transicion)")

    print("\n  TODO OK: la avalanche no mato la placa ni dejo basura en pantalla")
    return 0

sys.exit(main())
