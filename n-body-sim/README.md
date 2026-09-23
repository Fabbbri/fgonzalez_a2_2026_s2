# Simulación de dos galaxias en 2D

Primer incremento: inicialización secuencial de dos galaxias en C++17.
Cada galaxia contiene 100 cuerpos distribuidos en un disco de radio 1 y masa
total 1. Los centros están en `(-3, 0)` y `(3, 0)`, con velocidades de
traslación `(0.2, 0)` y `(-0.2, 0)` para preparar su encuentro.

Cada cuerpo tiene masa, posición, velocidad, aceleración e identificador de su
galaxia de origen. La velocidad combina traslación y rotación tangencial con
velocidad angular constante de `0.5`. Es una condición inicial sencilla, sin
cuerpo central masivo ni garantía de equilibrio gravitacional. Las magnitudes
usan unidades normalizadas.

## Compilar y ejecutar

Desde `n-body-sim`, con GNU Make y GCC o Clang:

```bash
make
./build/nbody
```

Para guardar el estado inicial:

```bash
mkdir -p output
./build/nbody > output/initial.csv
```

El CSV tiene una fila por cuerpo y las columnas
`id,galaxy,mass,x,y,vx,vy,ax,ay`. `galaxy` vale `0` o `1`.
Las carpetas `build/` y `output/` ya están excluidas por el `.gitignore`
del repositorio.

Los parámetros están en `main.cpp`. La semilla fija `42` permite repetir el
estado inicial con la misma implementación de la biblioteca estándar;
distintas bibliotecas pueden generar distribuciones diferentes.

## Próximo incremento

Todavía no hay evolución temporal: las aceleraciones permanecen en cero.
El siguiente paso será calcular las aceleraciones gravitacionales de todos
los cuerpos a partir de sus posiciones, incorporando suavizado. Después se
añadirá la actualización de velocidades y posiciones con Euler simpléctico.
Todo seguirá ejecutándose en un único hilo.
