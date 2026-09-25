#!/bin/sh
# Compila y corre el bench de las transiciones en el host (no toca la placa).
# El shim MicroBit.h imita el framebuffer, el reloj y el coste de cada
# operacion. El Codigo/ que se mide es el REAL.
set -e
cd "$(dirname "$0")"

C=../Codigo/Animaciones/Sistema/Transiciones

g++ -O2 -Wall -I mock -I ../Codigo -I "$C" -I "$C/Morfosis" -I "$C/Cortina" -I "$C/Fundido" \
    bench_transiciones.cpp \
    TransicionesOpt.cpp \
    "$C/Transiciones.cpp" \
    Viejas/MorfosisViejo.cpp Viejas/CortinaViejo.cpp Viejas/FundidoViejo.cpp \
    "$C/Morfosis/Morfosis.cpp" \
    "$C/Cortina/Cortina.cpp" \
    "$C/Fundido/Fundido.cpp" \
    -o bench_transiciones

echo "--- bench compilado ---"
./bench_transiciones
