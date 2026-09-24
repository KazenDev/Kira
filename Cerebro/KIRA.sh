#!/bin/bash
# ============================================================================
#  KIRA & KIRO - Launcher para LINUX (con diagnostico)
#  Uso:  ./KIRA.sh     (la primera vez: chmod +x KIRA.sh)
#
#  Verifica Python, instala dependencias (1ra vez), revisa el micro:bit
#  y levanta el Cerebro en http://127.0.0.1:8000
# ============================================================================
cd "$(dirname "$0")"

echo ""
echo "  ============================================================"
echo "    KIRA & KIRO - El Cerebro (Linux)"
echo "  ============================================================"
echo ""

# --- 1) Python ---
if ! command -v python3 >/dev/null 2>&1; then
    echo "  [ERROR] No se encontro python3."
    echo "  Instalalo con:  sudo apt install python3 python3-venv python3-pip"
    echo "  (o en Arch: sudo pacman -S python python-pip)"
    echo ""
    exit 1
fi
PY=python3
echo "  [1/4] Python: $($PY --version 2>&1)"

# --- 2) venv + dependencias ---
echo "  [2/4] Verificando dependencias..."
if [ ! -d "Backend/.venv" ]; then
    echo "        Creando entorno virtual (primera vez)..."
    $PY -m venv Backend/.venv || { echo "  [ERROR] Fallo al crear venv. Instala: sudo apt install python3-venv"; exit 1; }
fi
if ! Backend/.venv/bin/python -c "import fastapi, uvicorn, httpx, serial" >/dev/null 2>&1; then
    echo "        Instalando dependencias (primera vez)..."
    Backend/.venv/bin/pip install -r Backend/requirements.txt --quiet || {
        echo "  [ERROR] Fallo al instalar dependencias. Revisa tu conexion."
        exit 1
    }
fi
echo "        OK"

# --- 3) micro:bit (puerto serial) ---
echo "  [3/4] Buscando el micro:bit..."
PORT=""
if command -v python3 >/dev/null 2>&1; then
    PORT=$(Backend/.venv/bin/python -c "
import serial.tools.list_ports
for p in serial.tools.list_ports.comports():
    d=(p.description or '').lower()
    if 'micro' in d or 'mbed' in d or 'acm' in d:
        print(p.device); break
" 2>/dev/null)
fi
if [ -z "$PORT" ] && [ -e /dev/ttyACM0 ]; then PORT="/dev/ttyACM0"; fi
if [ -z "$PORT" ]; then
    echo "        [AVISO] No se detecto micro:bit. La web igual anda,"
    echo "        solo que la carita virtual no mostrara la replica en vivo."
else
    echo "        micro:bit detectado en: $PORT"
fi

# --- 4) Arrancar ---
echo "  [4/4] Levantando el Cerebro..."
echo "        (deja esta terminal abierta)"
echo ""
exec Backend/run.sh
