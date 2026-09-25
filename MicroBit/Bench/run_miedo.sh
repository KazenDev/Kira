#!/bin/sh
# Bench de Miedo: la version vieja (de git) contra la que se flashea.
set -e
cd "$(dirname "$0")"
g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Miedo \
    bench_miedo.cpp MiedoViejo.cpp \
    ../Codigo/Animaciones/Emociones/Miedo/Miedo.cpp \
    -o bench_miedo
echo "--- bench compilado ---"
./bench_miedo
