#!/bin/sh
# Bench de Sorprendido: la version vieja (de git) contra la que se flashea.
set -e
cd "$(dirname "$0")"
g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Sorprendido \
    bench_sorprendido.cpp SorprendidoViejo.cpp \
    ../Codigo/Animaciones/Emociones/Sorprendido/Sorprendido.cpp \
    -o bench_sorprendido
echo "--- bench compilado ---"
./bench_sorprendido
