"""
test_relay.py — el puente BLE no puede quedarse MUDO ni perder comandos.

Correr:  .venv/bin/python test_relay.py   (desde Cerebro/Backend)

Contexto (el bug del 17-sep): el usuario reportaba "siempre despues de la
primera respuesta se desconecta solo". Los logs del VPS mostraron otra cosa:
ultimo_comando=CALLA, ultimo_ack=ACK:LOADING y ble_relay=true. O sea el
puente seguia VIVO; lo que pasaba es que el latido tenia un hipo, el server
liberaba el puesto a los 60s y —lo grave— VACIABA la cola, asi que TALK/CALLA
(la boca y la cara de la placa) se perdian para siempre.

Que cubre:
  1. La cola SOBREVIVE a la expiracion del latido (el mismo cel vuelve y
     recibe lo que quedo pendiente).
  2. La cola no se vacia al re-registrarse (ni el mismo dueno ni otro).
  3. Los comandos VENCIDOS se descartan solos por EDAD (nada de rafagas
     rancias al reconectar) y la cola tiene tope.
  4. /api/ble/nack los devuelve al frente, sin duplicar.
  5. Un segundo dispositivo no le roba el puesto a un dueno VIVO, pero si
     releva a uno muerto (auto-cura).

NO toca el puerto serie: abrir /dev/ttyACM0 RESETEA el micro:bit, y eso le
tira el enlace BLE a quien este probando. Por eso el server mira KIRA_SIN_SERIAL.
"""
import os
import sys
import time
import traceback

os.environ["KIRA_SIN_SERIAL"] = "1"   # ANTES de importar: nada de abrir el USB

import kira_server as k  # noqa: E402

PUENTE = "celular-feria-token"
OTRO = "notebook-token"
fallos = []


def chequear(condicion, descripcion, extra=""):
    print(f"  [{'OK ' if condicion else 'FALLA'}] {descripcion}{(' -> ' + str(extra)) if extra and not condicion else ''}")
    if not condicion:
        fallos.append(descripcion)


def liberar_todo(s):
    s.relay_conectado = False
    s.relay_token = None
    s._relay_tx.clear()
    s.relay_ultimo_poll = 0.0


def expirar(s):
    """Simula el hipo real: el ultimo latido fue hace RELAY_GRACIA_SEG+1."""
    s.relay_ultimo_poll = time.time() - (k.RELAY_GRACIA_SEG + 1)
    s.relay_vivo()          # expiracion perezosa (lo que hace el worker)


def main():
    s = k.serial_mgr
    liberar_todo(s)

    print("== 1) un cel se registra y el server le encola comandos ==")
    chequear(s.relay_registrar(PUENTE) == "ok", "el primer cel toma el puente")
    s._relay_encolar("LOADING\n")
    s._relay_encolar("HAPPY\n")
    chequear(len(s._relay_tx) == 2, "2 comandos esperando al puente", s._relay_tx)

    print("\n== 2) el latido se vence (60s sin poll): el puesto se libera ==")
    expirar(s)
    chequear(s.relay_conectado is False, "el puesto quedo libre (auto-cura)")
    chequear(len(s._relay_tx) == 2, "** la cola SOBREVIVIO a la expiracion **", s._relay_tx)

    print("\n== 3) el MISMO cel vuelve y recibe lo pendiente (el bug de la boca) ==")
    chequear(s.relay_registrar(PUENTE) == "ok", "el mismo cel recupera el puesto")
    lineas = s.relay_tomar_tx()
    chequear("HAPPY\n" in lineas, "recibio el HAPPY que habia quedado pendiente", lineas)
    s._relay_encolar("TALK\n")
    s._relay_encolar("CALLA\n")
    chequear(s.relay_tomar_tx() == ["TALK\n", "CALLA\n"],
             "recibe TALK/CALLA (antes se perdian y la placa quedaba muda)")

    print("\n== 4) un comando VENCIDO se descarta solo (nada rancio) ==")
    s._relay_tx.append((time.time() - (k.RELAY_TX_VIDA_SEG + 5), "NEUTRAL\n"))
    s._relay_encolar("TALK\n")
    chequear(s.relay_tomar_tx() == ["TALK\n"], "el de 20s se tiro, el fresco paso")

    print("\n== 5) tope de la cola: una reconexion no dispara una rafaga eterna ==")
    s.relay_devolver([f"CMD{i}\n" for i in range(50)], )
    chequear(len(s._relay_tx) == k.RELAY_TX_MAX,
             f"la cola quedo topada en {k.RELAY_TX_MAX}", len(s._relay_tx))
    s._relay_tx.clear()

    print("\n== 6) NACK: devolver sin duplicar y al frente ==")
    s._relay_encolar("HAPPY\n")
    s.relay_devolver(["TALK\n", "HAPPY\n"])
    lineas = s.relay_tomar_tx()
    chequear(lineas.count("HAPPY\n") == 1, "HAPPY no se duplico", lineas)
    chequear(lineas[0] == "TALK\n", "el devuelto va primero", lineas)

    print("\n== 7) dueno VIVO no se deja robar el puesto / muerto si (auto-cura) ==")
    liberar_todo(s)
    s.relay_registrar(PUENTE)          # latiendo (recien registrado)
    chequear(s.relay_registrar(OTRO) == "ocupado", "el segundo dispositivo recibe 409")
    expirar(s)
    chequear(s.relay_registrar(OTRO) == "ok", "pasada la gracia, SI toma el puesto (auto-cura)")
    liberar_todo(s)

    print("\n== 8) ANTI-TORMENTA: un token en loop se frena (fue el bug del celu: 834 en 10min) ==")
    liberar_todo(s)
    resultados = [s.relay_registrar(PUENTE) for _ in range(k.RELAY_REGISTRO_MAX + 3)]
    primeros = resultados[: k.RELAY_REGISTRO_MAX]
    ultimos = resultados[k.RELAY_REGISTRO_MAX:]
    chequear(all(r == "ok" for r in primeros), "los primeros registros pasan normal", primeros[-3:])
    chequear(all(r == "bucle" for r in ultimos),
             f"al pasarse de {k.RELAY_REGISTRO_MAX} en {k.RELAY_REGISTRO_VENTANA:.0f}s devuelve 'bucle' (429)",
             ultimos[:3])
    chequear(s.relay_registrar(OTRO) == "ocupado", "otro token NO hereda el freno (da 409)")
    # pasada la ventana, se destraba solo
    s._relay_registros = [t - k.RELAY_REGISTRO_VENTANA - 1 for t in s._relay_registros]
    chequear(s.relay_registrar(PUENTE) == "ok", "pasada la ventana, se destraba solo")
    liberar_todo(s)
    s._relay_registros = []

    print()
    if fallos:
        print(f"FALLARON {len(fallos)} chequeo(s):")
        for f in fallos:
            print(f"  - {f}")
        return 1
    print("TODO OK ✅")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except Exception:
        traceback.print_exc()
        sys.exit(2)
