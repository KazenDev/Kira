#!/bin/sh
# Bench de Neutral: la version vieja (de git) contra la que se flashea.
set -e
cd "$(dirname "$0")"
g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Neutral \
    bench_neutral.cpp NeutralViejo.cpp \
    ../Codigo/Animaciones/Emociones/Neutral/Neutral.cpp \
    -o bench_neutral
echo "--- bench compilado ---"
./bench_neutral
