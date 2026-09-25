#!/bin/sh
# Bench de Triste: la version vieja (de git) contra la que se flashea.
set -e
cd "$(dirname "$0")"
g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Triste \
    bench_triste.cpp TristeViejo.cpp \
    ../Codigo/Animaciones/Emociones/Triste/Triste.cpp \
    -o bench_triste
echo "--- bench compilado ---"
./bench_triste
