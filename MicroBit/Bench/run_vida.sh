#!/bin/sh
# Tests del motor de vida en el PC (no toca la placa). Ver test_vida.cpp.
set -e
cd "$(dirname "$0")"
N=../Codigo/Animaciones/Vida/Nucleo
E=../Codigo/Animaciones/Emociones
g++ -O2 -Wall -Wextra -I ../Codigo test_vida.cpp \
    $N/Lienzo.cpp $N/Ojos.cpp $N/Habla.cpp $N/Cara.cpp $N/Reacciones.cpp \
    $N/Juegos.cpp $N/Catalogo.cpp \
    $E/Emociones.cpp $E/*/*.cpp \
    -o test_vida
./test_vida
