#!/bin/sh
# Bench de Enojado: la version vieja (de git) contra la que se flashea.
set -e
cd "$(dirname "$0")"
g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Enojado \
    bench_enojado.cpp EnojadoViejo.cpp \
    ../Codigo/Animaciones/Emociones/Enojado/Enojado.cpp \
    -o bench_enojado
echo "--- bench compilado ---"
./bench_enojado
