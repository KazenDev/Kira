#!/bin/sh
# Compila el PROTOTIPO A/B de la alegria y deja el MICROBIT.hex listo para
# flashear. NO toca MicroBit/Codigo/ (la fuente de verdad) ni el source/ real
# de forma permanente: source/ es un target de rsync, regenerable.
#
#   sh build.sh          compila
#   sh build.sh flash    compila y flashea (necesita la placa conectada)
#   sh build.sh restore  deja source/ con el Codigo/ real de nuevo
#
# Que compila: el Codigo/Animaciones/Emociones/Alegria/Alegria.cpp REAL, sin
# tocarlo, contra un Sistema.h minimo para que el prototipo controle la ruta
# de comandos.
set -e
cd "$(dirname "$0")"
RAIZ="$(cd .. && pwd)"
SAMPLES="$RAIZ/Referencias/codal-microbit-v2-samples"

montar_source() {
    rm -rf "$SAMPLES/source"
    mkdir -p "$SAMPLES/source/Animaciones/Emociones/Alegria" "$SAMPLES/source/Animaciones/Sistema"
    cp "$RAIZ/Prototipo/main.cpp"  "$SAMPLES/source/main.cpp"
    # El stub va en Animaciones/Sistema/ porque Alegria.cpp lo incluye con la
    # ruta relativa "../../Sistema/Sistema.h".
    cp "$RAIZ/Prototipo/Animaciones/Sistema/Sistema.h" \
       "$SAMPLES/source/Animaciones/Sistema/Sistema.h"
    # Alegria.cpp/.h: LOS DE VERDAD, sin tocar un byte.
    cp "$RAIZ/Codigo/Animaciones/Emociones/Alegria/Alegria.h" \
       "$SAMPLES/source/Animaciones/Emociones/Alegria/Alegria.h"
    cp "$RAIZ/Codigo/Animaciones/Emociones/Alegria/Alegria.cpp" \
       "$SAMPLES/source/Animaciones/Emociones/Alegria/Alegria.cpp"
    echo "source/ = prototipo (Alegria.cpp real + stub de Sistema)"
}

montar_real() {
    rsync -a --delete "$RAIZ/Codigo/" "$SAMPLES/source/"
    rm -f "$SAMPLES/source/Principal.cpp"
    cp "$RAIZ/Codigo/Principal.cpp" "$SAMPLES/source/main.cpp"
    echo "source/ = firmware real de Kira"
}

case "${1:-build}" in
  build)
    montar_source
    cd "$SAMPLES"
    python3 build.py
    echo
    echo "LISTO: $SAMPLES/MICROBIT.hex"
    echo "Para flashear: conectá la micro:bit y corré  sh build.sh flash"
    ;;
  flash)
    montar_source
    cd "$SAMPLES"
    python3 build.py
    DEST=/media/zkazen/MICROBIT
    if [ ! -d "$DEST" ]; then
        echo "No aparece $DEST. ¿Conectaste la micro:bit por USB?" >&2
        exit 1
    fi
    echo "Flasheando..."
    cp MICROBIT.hex "$DEST/"
    sleep 3
    echo "Hecho. Mirá la placa. (A = test de latencia, B = cambiar de modo)"
    ;;
  restore)
    montar_real
    echo "Para recompilar la firmware real: cd $SAMPLES && python3 build.py"
    ;;
  *)
    echo "uso: sh build.sh [build|flash|restore]" >&2
    exit 1
    ;;
esac
