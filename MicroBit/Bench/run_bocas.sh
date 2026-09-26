#!/bin/sh
# Bench de las 8 bocas de TALK (los Hablar*.cpp).
#
# Estas 8 se evaporated del analisis de las emociones: usan fiber_sleep() en
# vez de uBit.sleep(), asi que un grep de "sleep" no las encuentra. Y en
# Principal.cpp el TALK tiene PRIORIDAD sobre la animacion de la emocion, asi
# que mientras la IA habla, esto es todo lo que corre en el bucle principal.
set -e
cd "$(dirname "$0")"

E=../Codigo/Animaciones/Emociones

g++ -O2 -Wall -I mock -I ../Codigo -I ../Codigo/Animaciones/Emociones/Alegria -I ../Codigo/Animaciones/Emociones/Triste -I ../Codigo/Animaciones/Emociones/Cansado -I ../Codigo/Animaciones/Emociones/Miedo \
    bench_bocas.cpp \
    "$E/Alegria/Hablar.cpp" \
    HablarViejo.cpp HablarTristeViejo.cpp HablarCansadoViejo.cpp HablarMiedoViejo.cpp \
    "$E/Triste/HablarTriste.cpp" \
    "$E/Enojado/HablarEnojado.cpp" \
    "$E/Sorprendido/HablarSorprendido.cpp" \
    "$E/Neutral/HablarNeutral.cpp" \
    "$E/Fastidio/HablarFastidio.cpp" \
    "$E/Miedo/HablarMiedo.cpp" \
    "$E/Cansado/HablarCansado.cpp" \
    -o bench_bocas

echo "--- bench compilado ---"
./bench_bocas
