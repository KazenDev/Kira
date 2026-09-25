#!/bin/sh
# Compila y corre el bench de Alegria en el host (no toca la placa).
# El shim MicroBit.h imita el framebuffer, el reloj y el coste de las
# operaciones que usa la animacion. El Codigo/ que se mide es el REAL.
set -e
cd "$(dirname "$0")"

g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Alegria \
    bench_alegria.cpp \
    AlegriaVieja.cpp \
    ../Codigo/Animaciones/Emociones/Alegria/Alegria.cpp \
    -o bench_alegria

echo "--- bench compilado ---"
./bench_alegria "${1:-6148}"
