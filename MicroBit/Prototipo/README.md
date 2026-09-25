# Prototipo A/B — el banco de pruebas visual de la emoción Alegría

Esto **no es la firmware de Kira**. Es el banco con el que se decidió migrar
las animaciones al patrón de frame: compila el `Alegria.cpp` **real** (el de
la placa, sin tocarlo) contra una copia congelada de la versión vieja, y las
alterna en la misma placa para que la diferencia se vea **con los ojos**, no
solo en un número.

## Cómo usarlo

```sh
sh build.sh flash     # compila el prototipo y lo flashea
sh build.sh restore   # deja Codigo/ real de vuelta en el source/ del build
```

- `A` = "viene un comando de la IA": mide la **latencia** de la placa real.
- `B` = cambia de versión al instante.

## Advertencia importante

`main.cpp` tiene **una copia congelada** de la animación con patrón de frame
dentro. No la edites: la fuente de verdad es
`Codigo/Animaciones/Emociones/Alegria/Alegria.cpp`. Esta copia quedó
 deliberadamente parada en el momento de la migración, que es justamente lo
que la hace útil como línea de base: si alguien cambia la animación real,
`sh build.sh flash` muestra **antes vs. después** en la misma placa.

Cuando ya no lo necesites para comparar, se puede borrar el directorio entero
sin afectar nada: la firmware vive en `Codigo/`.
