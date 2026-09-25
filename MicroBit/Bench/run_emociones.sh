#!/bin/sh
# Compila y corre el bench de las 8 emociones en el host (no toca la placa).
# El shim MicroBit.h imita el framebuffer, el reloj y el coste de cada
# operacion. El Codigo/ que se mide es el REAL.
set -e
cd "$(dirname "$0")"

E=../Codigo/Animaciones/Emociones

g++ -O2 -Wall -I mock -I ../Codigo \
    bench_emociones.cpp \
    "$E/Alegria/Alegria.cpp" \
    "$E/Triste/Triste.cpp" \
    "$E/Enojado/Enojado.cpp" \
    "$E/Sorprendido/Sorprendido.cpp" \
    "$E/Neutral/Neutral.cpp" \
    "$E/Fastidio/Fastidio.cpp" \
    "$E/Miedo/Miedo.cpp" \
    "$E/Cansado/Cansado.cpp" \
    -o bench_emociones

echo "--- bench compilado ---"
./bench_emociones "${1:-12000}"
