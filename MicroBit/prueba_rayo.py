#!/usr/bin/env python3
"""
prueba_rayo.py - el rayo de la tormenta: ¿se completa, y cede ante la app?

SON DOS FASES PORQUE SON DOS COMPORTAMIENTOS DISTINTOS, Y MEDIRLOS JUNTOS
DABA UN RESULTADO FALSO.

  FASE 1 (sin comandos) - el rayo se COMPLETA.
  FASE 2 (con comandos) - el rayo SE APARTA, y la app no espera.

La confusion: probar los dos a la vez da 0% de rayos completos, y parece que
el rayo este roto. No lo esta. Es que revisarSerial() devuelve true en
cuanto llega un comando, y frameRayo() lo toma como "cortar", asi que un rayo
que dura ~500 ms (unos 32 frames) con la app mandando un comando cada 95 ms se
va a cortar SIEMPRE, y a proposito. Cedele el paso a la IA es el comportamiento
correcto: es lo que hace que un STOP o un SENSOR no espere al trueno.

Asi que la pregunta correcta no es "el rayo llega al final con la app
hablando" (no, y no deberia), sino las dos de su lado:

  - sin nadie hablando, ¿llega entero?          (no lo puede interrumpir nadie)
  - con la app hablando, ¿responde rápido?      (y el rayo que se aparte)

Y el segundo es el que responde a la pregunta original del uBit.sleep(120):
la ventana sorda. Antes, con el corte del lote, ademas de partir el rayo, el
paso 4 se dormia 120 ms. Ahora no hay ningun sleep >16 ms en todo Lluvia.

TRES COSAS QUE HAY QUE HACER BIEN, Y LAS TRES SE APRENDIERON A LA BRAZA

  a) REPLICA:OFF. La replica del display (ReplicaLed.cpp) comparte el UART con
     los comandos y manda a ~20 fps mientras se mueve. Con ella encendida el
     piso de la latencia es de 134 ms, mas grande que cualquier efecto de
     120 ms que uno quiera medir. Y peor: durante el rayo la pantalla se apaga,
     la replica manda MENOS, y la latencia medida DENTRO del rayo salia MAS
     BAJA que fuera. El instrumento se movia en contra. (Ver ReplicaLed.h: ese
     comando existe por esto.)

  b) UN COMANDO A LA VEZ, NO UNA RAFA. Mandar SENSOR:TEMP cada 55 ms sin
     esperar la respuesta satura el puerto: se pierden ~9% de los comandos
     (medido: 3297 respondidos de 3636 enviados) y la latencia que se mide es
     la de una cola, no la de un puerto sordo; la mediana daba 200 ms, que no
     significa nada. Aqui se manda uno, se ESPERA su respuesta, se mide, y se
     deja un hueco. Sin cola, sin perdidas.

  c) s.timeout chico (10 ms). Con el timeout de 0,2 s del connect, cada read
     bloqueaba hasta 200 ms y la "latencia" medida era el timeout, no el
     puerto. Con 10 ms el read vuelve en cuanto hay un byte.

PARA PROBAR QUE EL TEST SIRVE: el firmware anterior daba 0% de rayos
completos y un sleep(120) en medio del retumbo.
"""
import sys
import time

import serial

PUERTO = "/dev/ttyACM0"
FASE1_S = float(sys.argv[1]) if len(sys.argv) > 1 else 240.0   # sin comandos
FASE2_S = 120.0                                               # con comandos
HUECO = 0.085                 # ~7 pings/s en la fase 2
UMBRAL_MS = 60.0              # 3 frames; la linea base anda por 10-20 ms


def conectar(intentos=12):
    """La placa se re-enumera al flashear: reintenta en vez de morir."""
    for _ in range(intentos):
        try:
            s = serial.Serial(PUERTO, 115200, timeout=0.2)
            time.sleep(2.0)
            s.reset_input_buffer()
            s.write(b"SENSOR:TEMP\n")
            t0, buf = time.time(), b""
            while time.time() - t0 < 2.5:
                buf += s.read(200)
                if b"\nTEMP:" in buf:
                    s.timeout = 0.01      # ver (c) en el docstring
                    return s
            s.close()
        except Exception:
            pass
        time.sleep(2)
    return None


def contar_marcadores(buf, acc):
    """Consume cada marcador UNA vez. Devuelve lo que dejo en buf."""
    while True:
        c = [(buf.find(m), len(m), g) for m, g in
             ((b"RAYO:INI", "INI"), (b"RAYO:P4", "P4"),
              (b"RAYO:FIN", "FIN")) if buf.find(m) >= 0]
        if not c:
            return buf
        i, n, g = min(c)
        del buf[:i + n]
        acc[g] += 1


def main():
    s = conectar()
    if not s:
        print("FALLO: no se pudo hablar con la placa")
        return 1

    fallos = []
    try:
        return correr(s)
    finally:
        # SIEMPRE dejar la replica como estaba. Sin esto, que este test se
        # corte por excepcion, por Ctrl-C o por el timeout del shell deja la
        # replica apagada en la RAM de la placa, y el siguiente test que
        # necesite frames LED (prueba_transiciones.py) falla con "no llego
        # ningun frame LED", que no dice nada de su propia causa. Ese footgun
        # costo una ronda de diagnostico entero.
        try:
            s.write(b"REPLICA:ON\n")
            s.write(b"STOP\n")
        except Exception:
            pass


def correr(s):
    s.write(b"REPLICA:OFF\n")
    time.sleep(0.5)

    # ---------------- FASE 1: sin comandos, ¿llega entero? ----------------
    s.reset_input_buffer()
    s.write(b"LOAD3\n")
    time.sleep(1.5)
    s.reset_input_buffer()
    acc = {"INI": 0, "P4": 0, "FIN": 0}
    buf = bytearray()
    t0 = time.time()
    while time.time() - t0 < FASE1_S:
        d = s.read(400)
        if d:
            buf += d
        buf = contar_marcadores(buf, acc)
        if len(buf) > 500:
            del buf[:-100]

    s.write(b"STOP\n")
    ini, p4, fin = acc["INI"], acc["P4"], acc["FIN"]
    print(f"FASE 1  {FASE1_S:.0f} s en LOAD3 SIN mandar nada")
    print(f"  RAYO:INI (entraron al rayo)   : {ini}")
    print(f"  RAYO:P4  (llegaron al retumbo): {p4}")
    print(f"  RAYO:FIN (llegaron AL FINAL)  : {fin}")
    if ini:
        print(f"  completos: {fin}/{ini} = {100*fin/ini:.0f}%"
              f"   (el firmware anterior daba 0%)")
    print()

    if ini < 3:
        fallos.append(f"fase 1: solo {ini} rayos en {FASE1_S:.0f} s: no hay material")
    elif fin != ini or p4 != ini:
        fallos.append(f"fase 1: {ini} rayos pero solo {p4} llegaron al retumbo y "
                      f"{fin} al final. {ini-fin} se cortaron a mitad.")

    # ---------------- FASE 2: con comandos, ¿responde rápido? ----------------
    s.reset_input_buffer()
    s.write(b"LOAD3\n")
    time.sleep(1.5)
    s.reset_input_buffer()
    acc2 = {"INI": 0, "P4": 0, "FIN": 0}
    buf = bytearray()
    lat = []
    t0 = time.time()
    proximo = t0
    while time.time() - t0 < FASE2_S:
        ahora = time.time()
        if ahora >= proximo:
            t = time.time()
            s.write(b"SENSOR:TEMP\n")
            # Un SOLO buffer para la respuesta y los marcadores. Con un buffer
            # aparte la respuesta se descartaba al salir y se COMIA los RAYO: que
            # llegaba de paso (era el turno, no la placa, la que los perdia).
            #
            # Y se busca "\nTEMP:" y no "TEMP:" porque la respuesta es
            # "ACK:SENSOR:TEMP\nTEMP:25\n" y el primer TEMP: esta DENTRO del
            # ACK: con find("TEMP:") el match disparaba en el ACK.
            while time.time() - t < 2.0:
                d = s.read(200)
                if d:
                    buf += d
                i = buf.rfind(b"\nTEMP:")
                if i >= 0 and buf.find(b"\n", i + 1) >= 0:
                    lat.append((time.time() - t) * 1000.0)
                    break
            proximo = time.time() + HUECO

        d = s.read(400)
        if d:
            buf += d
        buf = contar_marcadores(buf, acc2)
        if len(buf) > 500:
            del buf[:-100]

    v = sorted(lat)
    print(f"FASE 2  {FASE2_S:.0f} s en LOAD3 mandando un comando cada ~95 ms")
    print(f"  pings con respuesta: {len(v)}")
    if v:
        print(f"  mediana {v[len(v)//2]:6.1f} ms    p95 {v[int(len(v)*.95)]:6.1f}"
              f"    PEOR {v[-1]:6.1f} ms   (umbral {UMBRAL_MS:.0f})")
    print(f"  rayos: {acc2['INI']} iniciados, {acc2['FIN']} completados")
    print("  (que se corten aqui es lo CORRECTO: la app manda un comando cada")
    print("   95 ms y revisarSerial() le gana al rayo. No es un fallo.)")
    print()

    if not v:
        fallos.append("fase 2: la placa no contesto ni un SENSOR:TEMP")
    elif v[-1] > UMBRAL_MS:
        fallos.append(f"fase 2: un ping tardo {v[-1]:.1f} ms: el rayo congela "
                      f"el puerto (el sleep(120) de antes)")

    if fallos:
        print("FALLO")
        for f in fallos:
            print(f"  - {f}")
        return 1

    print(f"OK fase 1: {fin}/{ini} rayos completos sin interrupcion (antes 0%)")
    print(f"OK fase 2: el puerto nunca se congela (peor ping {v[-1]:.1f} ms)")
    print("OK: el rayo cede ante un comando de la app, que es lo que debe hacer")
    return 0


sys.exit(main())
