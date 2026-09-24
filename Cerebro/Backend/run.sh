#!/bin/bash
# ================================================================================
#  KIRA - Launcher del Backend
#  Uso:  ./run.sh
#
#  Detecta si el micro:bit necesita sudo (Linux: grupo dialout) y arranca
#  el server. En Windows (feria) no hace falta nada de esto.
# ================================================================================
cd "$(dirname "$0")"

# Carga secretos y configuración local sin meterlos en run.sh.
# El archivo .env queda ignorado por .gitignore.
if [ -f .env ]; then
    set -a
    # shellcheck disable=SC1091
    . ./.env
    set +a
fi

PY=./.venv/bin/python
[ -x "$PY" ] || PY=python3

# Si el usuario no puede abrir /dev/ttyACM* (falta grupo dialout), usa sudo
NECESITA_SUDO=""
if [ -e /dev/ttyACM0 ] || [ -e /dev/ttyACM1 ]; then
  if ! $PY -c "import serial; serial.Serial('/dev/ttyACM0', 115200, timeout=0.1).close()" 2>/dev/null; then
    echo "[run] el puerto serial necesita permisos -> usando sudo (agrega tu user a 'dialout' para evitarlo)"
    NECESITA_SUDO="sudo"
  fi
fi

echo "=============================================================="
echo "  KIRA - El Cerebro esta vivo"
echo "  Abri: http://127.0.0.1:8000"
echo "=============================================================="

# OJO el -u: sin buffer. Sin esto, con la salida a un archivo/pipe Python
# acumula los prints y el log aparece a pedazos (imposible seguir el viaje de
# una herramienta o el puente BLE en vivo).
exec $NECESITA_SUDO $PY -u -m uvicorn kira_server:app --host 127.0.0.1 --port 8000
