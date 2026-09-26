"""
prueba_talk.py - Verifica la boca de TALK en la placa real.

TALK es el camino que mas se usa en una feria (mientras la IA habla) y el que
tenia la ventana sorda mas larga del proyecto, porque Principal.cpp le da
prioridad a la boca sobre la animacion de la emocion: mientras habla, la boca
es TODO lo que corre en el hilo principal.

Prueba, para CADA emocion con boca migrada:
  1) Que TALK arranque y la placa lo acuse.
  2) Que la boca siga animandose (para Triste: la mandibula sube y baja; para
     Alegria: los dientes prenden y apagan).
  3) La latencia de un comando CON TALK ACTIVO. Era el numero que se arreglo.
  4) Que al salir de TALK la cara quede limpia (nada de la boca de TALK).

Uso:  python3 prueba_talk.py
"""
import serial, time, re, sys

PUERTO = "/dev/ttyACM0"

# (etiqueta, comando de emocion, pixeles que se mueven, como se que se movieron,
#  pixel que SOLO escribe la boca de TALK, o None si la boca usa unicamente
#  pixeles que ya estan en la cara en reposo)
#
# OJO: el ultimo campo es indispensable. Para Alegria la boca de TALK escribe
# los MISMOS pixeles que su cara en reposo ((1,3)(2,3)(3,3) son la sonrisa), asi
# que no hay ningun pixel exclusivo y lo unico verificable es que la boca dejo
# de moverse. Para Triste, en cambio, la mandibula enciende (2,4), que NO
# existe en su frown en reposo, y ese si sirve para detectar un resto.
CASOS = [
    ("ALEGRIA", b"HAPPY\n", {(1,3),(2,3),(3,3)}, "dientes",  None),
    ("TRISTE",  b"SAD\n",   {(2,3), (2,4)},          "mandibula", (2,4)),
    # Cansado: los 3 dientes de la boca chica laten a 200 de brillo (tenue).
    # OJO: la boca de TALK de Cansado usa los MISMOS pixeles que su cara en
    # reposo ((1,3)(2,3)(3,3) son la boca chica), asi que no hay pixel
    # exclusivo: lo unico verificable es que la boca se detuvo.
    ("CANSADO", b"TIRED\n", {(1,3),(2,3),(3,3)},    "boca chica", None),
    # Miedo: la boca GRANDE de 8 pixeles (filas 2 a 4) se cierra y se reabre.
    # Tampoco hay pixel exclusivo: usa los mismos que su cara en reposo.
    ("MIEDO",   b"SCARED\n", {(1,2),(2,2),(3,2),(1,3),(3,3),
                              (1,4),(2,4),(3,4)},      "boca grito", None),
]

def asentar(s, seg=2.0):
    time.sleep(seg)
    s.reset_input_buffer()
    for _ in range(40):
        s.read(300)
    s.reset_input_buffer()

def leer_por(s, plazo):
    buf = ""
    t0 = time.time()
    while time.time() - t0 < plazo:
        buf += s.read(400).decode("latin-1")
    return buf

def pixeles(texto,subset):
    """Los estados de `subset` en cada frame LED visto."""
    est = []
    for m in re.findall(r"LED:([.#]{25})", texto):
        px = {(i % 5, i // 5) for i, c in enumerate(m[:25]) if c == "#"}
        est.append(frozenset(p for p in subset if p in px))
    return est

def probar(s, nombre, cmd, movers, etiqueta, pixel_talk):
    ok = True
    print(f"\n=== {nombre} ===")

    # --- arrancar TALK ----------------------------------------------------
    s.reset_input_buffer()
    s.write(cmd); time.sleep(1.2)
    s.reset_input_buffer()
    s.write(b"TALK\n"); time.sleep(1.5)
    r = leer_por(s, 0.4)
    if "ACK:TALK" in r:
        print("  1) TALK arranca        OK")
    else:
        print("  1) TALK arranca        FALLO (sin ACK:TALK)")
        return False

    # --- la boca se mueve -------------------------------------------------
    s.reset_input_buffer()
    est = pixeles(leer_por(s, 4.0), movers)
    distintos = len(set(est))
    print(f"  2) la boca se mueve     {distintos} estados distintos de {etiqueta} en 4 s")
    if distintos < 2:
        print("     FALLO: la boca quedo fija en un solo estado")
        ok = False
    else:
        print("     OK")

    # --- latencia con TALK activo ----------------------------------------
    lat = []
    for i in range(24):
        s.reset_input_buffer()
        t0 = time.time()
        s.write(b"SENSOR:TEMP\n")
        while time.time() - t0 < 1.0:
            if s.readline().startswith(b"ACK:"):
                lat.append((time.time() - t0) * 1000)
                break
        time.sleep(0.12)
    if not lat:
        print("  3) latencia            FALLO (nenhum ACK con TALK activo)")
        ok = False
    else:
        lat.sort()
        peor = max(lat)
        print(f"  3) latencia            {len(lat)}/24 ACKs   "
              f"media {sum(lat)/len(lat):5.1f} ms   PEOR {peor:5.1f} ms")
        if peor < 60:
            print("     OK: se entera en el frame aunque este hablando")
        else:
            print(f"     FALLO: {peor:.0f} ms para un frame de 16 ms")
            ok = False

    # --- salir de TALK: la boca queda quieta y limpia -------------------
    # Se usa CALLA y NO un cambio de emocion. Razon: CALLA corta TALK y
    # repinta la cara de la MISMA emocion, asi que el unico cambio posible en
    # la pantalla es la boca. Si se mandara otra emocion, los pixeles medidos
    # mezclarian los de la animacion nueva (el bostezo de Cansado, por
    # ejemplo, enciende (2,4), que es justo el pixel de la mandibula de
    # Triste). Con CALLA no hay confound.
    s.reset_input_buffer()
    s.write(b"CALLA\n"); time.sleep(0.6)
    s.reset_input_buffer()
    txt = leer_por(s, 1.5)

    frames = []
    for m in re.findall(r"LED:([.#]{25})", txt):
        px = {(i % 5, i // 5) for i, c in enumerate(m[:25]) if c == "#"}
        frames.append(px)

    conMandibula = (sum(1 for px in frames if pixel_talk in px)
                    if pixel_talk else 0)
    ultimos = [frozenset(p for p in movers if p in px) for px in frames[-6:]]
    quieta = len(set(ultimos)) <= 1

    if not quieta:
        print("     FALLO: la boca sigue parpadeando despues de CALLA")
        ok = False
    elif pixel_talk and conMandibula:
        print(f"     FALLO: quedo el pixel {pixel_talk} de TALK colgando "
              f"({conMandibula} frames)")
        ok = False
    else:
        extra = f", y {pixel_talk} apagado" if pixel_talk else ""
        print(f"     OK: la boca se detuvo{extra}")

    return ok

def main():
    s = serial.Serial(PUERTO, 115200, timeout=0.05)
    asentar(s, 2.5)
    todo = True
    for nombre, cmd, movers, etiqueta, pixel_talk in CASOS:
        if not probar(s, nombre, cmd, movers, etiqueta, pixel_talk):
            todo = False
    s.close()
    print("\n" + ("TODO OK" if todo else "HUBO FALLOS"))
    return 0 if todo else 1

sys.exit(main())
