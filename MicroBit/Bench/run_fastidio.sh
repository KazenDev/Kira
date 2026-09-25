#!/bin/sh
# Bench de Fastidio: la version vieja (de git) contra la que se flashea.
set -e
cd "$(dirname "$0")"
g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Fastidio \
    bench_fastidio.cpp FastidioViejo.cpp \
    ../Codigo/Animaciones/Emociones/Fastidio/Fastidio.cpp \
    -o bench_fastidio
echo "--- bench compilado ---"
./bench_fastidio
