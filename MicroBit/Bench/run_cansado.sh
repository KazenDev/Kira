#!/bin/sh
# Bench de Cansado: la version vieja (de git) contra la que se flashea.
set -e
cd "$(dirname "$0")"
g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Cansado \
    bench_cansado.cpp CansadoViejo.cpp \
    ../Codigo/Animaciones/Emociones/Cansado/Cansado.cpp \
    -o bench_cansado
echo "--- bench compilado ---"
./bench_cansado
