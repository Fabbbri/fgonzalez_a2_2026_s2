# Simulación N-body de dos galaxias en 2D

Implementación secuencial en C++17 de la interacción gravitacional entre dos
galaxias. El programa se utiliza como sistema base para estudiar y comparar
modelos de ejecución multihilo en el curso CE 4302 — Arquitectura de
Computadores II.

La versión actual inicializa dos galaxias reproducibles, calcula la interacción
gravitacional entre todos los cuerpos, integra su movimiento en dos dimensiones
y permite observar la evolución mediante una ventana nativa construida con
SDL2. El cálculo se ejecuta en un solo hilo.

## Estado actual

La base secuencial incluye:

- dos galaxias distinguibles por color e identificador de origen;
- interacción N-body completa con complejidad $O(N^2)$ por paso;
- suavizado gravitacional para evitar singularidades numéricas;
- integración temporal mediante Euler simpléctico;
- visualización nativa 2D sin archivos intermedios;
- ejecución sin ventana para mediciones;
- medición del tiempo de cálculo separada del dibujo y de los diagnósticos;
- diagnóstico de masa, energía, momento lineal y centro de masa;
- condiciones iniciales reproducibles mediante una semilla;
- pruebas físicas, de interfaz de línea de comandos y de visualización.

Los modelos Fine-Grained, Coarse-Grained, SMT y CMP todavía no están
implementados. Sus diagramas se incorporarán como parte del diseño previo.

## Modelo físico

Cada cuerpo almacena masa, posición, velocidad y aceleración en el plano XY.
La aceleración del cuerpo $i$ se obtiene de la contribución de todos los
cuerpos $j \ne i$:

$$
\vec{a}_i =
\sum_{j \ne i}
Gm_j
\frac{\vec{r}_j - \vec{r}_i}
{\left(\lVert\vec{r}_j - \vec{r}_i\rVert^2 + \epsilon^2\right)^{3/2}}
$$

La implementación utiliza unidades normalizadas, $G = 1$ y un suavizado
$\epsilon = 0.05$. Después de calcular todas las aceleraciones desde el
mismo estado temporal, actualiza cada cuerpo con Euler simpléctico:

$$
\vec{v}_i(t + \Delta t) =
\vec{v}_i(t) + \vec{a}_i(t)\Delta t
$$

$$
\vec{r}_i(t + \Delta t) =
\vec{r}_i(t) + \vec{v}_i(t + \Delta t)\Delta t
$$

La simulación representa la interacción gravitacional entre galaxias. No
modela choques rígidos entre estrellas.

## Requisitos

- compilador compatible con C++17, como GCC o Clang;
- GNU Make;
- SDL2 con archivos de desarrollo;
- `pkg-config`.

En Debian o Ubuntu:

```bash
sudo apt install build-essential pkg-config libsdl2-dev
```

En macOS con Homebrew:

```bash
brew install sdl2 pkg-config
```

## Compilación y ejecución

Desde la carpeta `n-body-sim`:

```bash
make
./build/nbody
```

La ejecución normal abre la ventana nativa. Para ejecutar el sistema base sin
visualización y obtener una medición adecuada para futuros benchmarks:

```bash
./build/nbody --no-output
```

El modo `--no-output` no inicializa SDL2. El tiempo reportado excluye la
inicialización aleatoria, los diagnósticos físicos y el dibujo.

### Parámetros

| Opción | Descripción | Valor predeterminado |
|---|---|---:|
| `--bodies-per-galaxy N` | Cantidad de cuerpos en cada galaxia | `100` |
| `--steps N` | Cantidad de pasos temporales | `10000` |
| `--dt X` | Duración de cada paso | `0.002` |
| `--seed N` | Semilla de 32 bits para la inicialización | `42` |
| `--no-output` | Ejecuta sin ventana para medir rendimiento | desactivado |

Ejemplo con 400 cuerpos totales:

```bash
./build/nbody --bodies-per-galaxy 200 --steps 15000 --dt 0.002
```

La ayuda integrada muestra las opciones disponibles:

```bash
./build/nbody --help
```


## Opciones de Make

| Comando | Acción |
|---|---|
| `make` o `make all` | Compila `build/nbody` |
| `make run` | Compila si es necesario y abre la simulación |
| `make test` | Compila y ejecuta todas las pruebas |
| `make check-sdl` | Comprueba que SDL2 esté disponible para enlazar |
| `make clean` | Elimina los ejecutables generados en `build/` |


## Estructura del proyecto

```text
n-body-sim/
├── include/nbody/       Interfaces y estructuras compartidas
├── src/                 Implementación secuencial y visualización
├── tests/               Pruebas físicas, de CLI y de ventana
├── docs/diagrams/       Diagramas de los modelos de ejecución
├── Makefile
└── README.md
```

Responsabilidades principales:

- [`src/main.cpp`](src/main.cpp): coordina configuración, simulación,
  medición y diagnósticos.
- [`src/config.cpp`](src/config.cpp): valida las opciones de línea de
  comandos.
- [`src/simulation.cpp`](src/simulation.cpp): inicialización, cálculo
  gravitacional e integración temporal.
- [`src/diagnostics.cpp`](src/diagnostics.cpp): masa, energía, momento y
  centro de masa.
- [`src/native_view.cpp`](src/native_view.cpp): ventana SDL2 y controles,
  separados del modo de medición.
- [`src/output.cpp`](src/output.cpp): presentación textual de los
  diagnósticos.

## Variables importantes para la paralelización

### Cantidad de cuerpos

`--bodies-per-galaxy` determina el tamaño total del sistema:

```text
N = 2 × bodies_per_galaxy
```

Es la variable que más aumenta el paralelismo disponible. El
[núcleo de cálculo gravitacional](src/simulation.cpp#L56-L80) es la región que
concentra la mayor parte del trabajo de la simulación: cada cuerpo `i` calcula
su aceleración recorriendo los otros $N-1$ cuerpos. Por tanto:

- existen $N$ resultados de aceleración que pueden calcularse de manera
  independiente dentro de un paso;
- cada resultado requiere $N-1$ contribuciones;
- el trabajo total por paso crece como $O(N^2)$;
- aumentar $N$ incrementa tanto el trabajo como la cantidad de tareas que
  pueden distribuirse entre unidades de ejecución.

La separación entre el vector de entrada `bodies` y el vector de salida
`accelerations` evita que el cálculo de un cuerpo modifique las posiciones
que leen los demás. Esa interfaz está declarada en
[`include/nbody/simulation.hpp`](include/nbody/simulation.hpp#L26-L30).

### Número de pasos

`--steps` determina cuántas veces se ejecuta el núcleo de cálculo
gravitacional. Aumentarlo incrementa el trabajo total aproximadamente como:

```text
trabajo total ≈ steps × N²
```

Sin embargo, los pasos temporales tienen una dependencia: el paso $t+1$
necesita las posiciones y velocidades producidas en $t$. El
[bucle temporal secuencial](src/main.cpp#L25-L34) no puede repartirse
directamente entre hilos sin cambiar el algoritmo numérico.

Esta variable es útil para aumentar la duración del experimento y amortizar
costos fijos, pero no aumenta el paralelismo disponible dentro de un paso.

### Tamaño temporal `dt`

`--dt` controla la resolución y estabilidad de la integración. Si se mantiene
constante el tiempo físico total, reducir `dt` requiere más pasos y aumenta
el trabajo. No crea nuevas tareas independientes porque conserva la
dependencia temporal entre estados.

La actualización de los cuerpos sí contiene paralelismo dentro de cada paso:
una vez calculadas todas las aceleraciones, cada velocidad y posición puede
actualizarse de forma independiente. Esta región se encuentra en
[`advance_one_step`](src/simulation.cpp#L96-L113).

### Número de trabajadores y partición

El número de hilos o procesos será una variable de los modelos posteriores.
La estrategia prevista consiste en dividir el rango exterior de cuerpos
`i = [0, N)` entre trabajadores. Cada trabajador producirá aceleraciones para
un subconjunto distinto y todos leerán el mismo estado de posiciones.

Entre el cálculo de aceleraciones y la actualización será necesaria una
barrera: ningún cuerpo debe avanzar al estado $t+1$ mientras otro todavía
calcula fuerzas usando parcialmente ese nuevo estado.

El número de trabajadores permitirá estudiar speedup, eficiencia y
escalabilidad. Si hay demasiados trabajadores para el valor de $N$, el
costo de planificación y sincronización puede superar el trabajo asignado a
cada uno.

### Semilla y modo de ejecución

`--seed` no aumenta el paralelismo, pero es esencial para una comparación
válida: todos los modelos deben recibir exactamente las mismas condiciones
iniciales. La generación reproducible de ambas galaxias está en
[`create_galaxies`](src/simulation.cpp#L42-L52).

La visualización tampoco forma parte del cálculo evaluado. El
[modo sin ventana](src/main.cpp#L25-L34) evita inicializar SDL2 y mide un
intervalo continuo. La ventana nativa mantiene separado el cálculo del dibujo
en [`run_native`](src/native_view.cpp#L155-L223).

## Diagramas de los modelos de ejecución

Los siguientes espacios están preparados para los diagramas de diseño. Las
imágenes deben guardarse en `docs/diagrams/` con los nombres indicados.

### Fine-Grained Multithreading

Modelo planificado mediante tareas con estado explícito y selección
*round-robin* en cada cuanto mínimo, aunque la tarea actual no esté bloqueada.

![Diagrama de Fine-Grained Multithreading](docs/diagrams/fine-grained.png)

### Coarse-Grained Multithreading

Modelo planificado con bloques de trabajo grandes y cambios de ejecución ante
eventos de bloqueo costosos definidos explícitamente.

![Diagrama de Coarse-Grained Multithreading](docs/diagrams/coarse-grained.png)

### SMT — Simultaneous Multithreading

Aproximación planificada mediante sobresuscripción de hilos respecto a los
núcleos físicos. La evaluación final deberá contrastar el hardware con SMT
habilitado y deshabilitado.

![Diagrama de Simultaneous Multithreading](docs/diagrams/smt.png)

### CMP — Chip Multiprocessing

Modelo planificado mediante paralelismo real sobre varios núcleos, dividiendo
los índices de cuerpos entre trabajadores y sincronizando las fases de cálculo
y actualización.

![Diagrama de Chip Multiprocessing](docs/diagrams/cmp.png)

## Verificación

Las pruebas se ejecutan con:

```bash
make test
```

Las verificaciones actuales comprueban:

- cálculo de energía, momento y centro de masa;
- integración mediante Euler simpléctico;
- estabilidad de un caso orbital sencillo;
- suavizado cuando dos cuerpos ocupan la misma posición;
- reproducibilidad de la interfaz de línea de comandos;
- equivalencia física entre la ejecución visual y la ejecución sin ventana;
- controles básicos de la ventana nativa.
