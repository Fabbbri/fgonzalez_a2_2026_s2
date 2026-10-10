# Simulación N-body de dos galaxias en 2D


## Estado actual

La base secuencial incluye:

- dos galaxias distinguibles por color e identificador de origen;
- interacción N-body completa con complejidad $O(N^2)$ por paso;
- suavizado gravitacional para evitar singularidades numéricas;
- integración temporal mediante Euler simpléctico;
- trayectoria binaria con el estado inicial y todos los pasos;
- reproducción nativa 2D posterior al cálculo;
- ejecución sin ventana para mediciones;
- tiempo total de ejecución en CSV, antes de abrir la ventana;
- validación de masa, energía, momento lineal y centro de masa en las pruebas;
- condiciones iniciales reproducibles mediante una semilla;
- pruebas físicas, de interfaz de línea de comandos y de visualización.

Los modelos Fine-Grained, Coarse-Grained, SMT y CMP todavía no están
implementados. Sus diagramas preliminares ya documentan cómo se dividirá y
sincronizará el cálculo gravitacional en cada modelo.

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
```

La ejecución normal sigue este orden:

1. Ejecuta toda la simulación y guarda el estado inicial y cada paso.
2. Finaliza y cierra `output/trajectory.nbody`.
3. Imprime el tiempo total y los parámetros de la ejecución en CSV por stdout.
4. Abre una ventana que reproduce el archivo, sin recalcular la física.

Para generar datos sin abrir la ventana:

```bash
./build/nbody --no-output --trajectory output/run-001.nbody > metrics-001.csv
```

`--no-output` conserva la captura y escritura de todos los estados; únicamente
omite el reproductor y no inicializa SDL2. Los directorios de la trayectoria
se crean automáticamente. La ruta elegida se sobrescribe en cada ejecución;
use rutas distintas si desea conservar varias trayectorias.

Para reproducir un archivo ya generado, en una ejecución independiente:

```bash
./build/nbody --replay output/run-001.nbody
```

`--replay` se utiliza sin opciones de simulación: lee sus parámetros del archivo,
no ejecuta el integrador ni imprime nuevas métricas. Espacio pausa; N avanza un
frame; R vuelve al estado inicial y pausa; F encuadra. La rueda y el arrastre
controlan la cámara, +/- cambia el ritmo de reproducción y Esc cierra la ventana.

### Interpretación de métricas

El enunciado (§4.4) exige tiempo total de ejecución, speedup, eficiencia paralela
y escalabilidad según la cantidad de hilos o procesos. Una ejecución individual
registra el tiempo total y los parámetros necesarios para comparar muestras:

```text
model,workers,bodies,steps,dt,seed,total_seconds
```

`total_seconds` es el intervalo continuo desde la apertura del archivo hasta su
flush y cierre, incluyendo cálculo y captura. Excluye inicialización aleatoria,
impresión y reproducción. `model` y `workers` identifican el esquema de ejecución;
`bodies`, `steps`, `dt` y `seed` identifican la entrada. Actualmente se utiliza
`sequential` con un trabajador como referencia.

La salida normal no imprime diagnósticos físicos ni tiempos parciales. Las
validaciones físicas siguen disponibles mediante `make test`.

Las métricas comparativas se obtienen externamente mediante Make/scripts:

- Speedup: tiempo medio secuencial dividido entre tiempo medio del modelo,
  con la misma entrada y política de captura.
- Eficiencia: speedup dividido entre la cantidad de trabajadores, explicando
  su significado para cada modelo, especialmente si es simulado.
- Escalabilidad: evolución del tiempo, speedup y eficiencia al variar la cantidad
  de hilos o procesos.
- Estadísticas: número de repeticiones, media, desviación estándar e intervalos
  de confianza del 95 % del tiempo y speedup.
- Distribuciones y estabilidad: boxplots o histogramas de las muestras y análisis
  de convergencia para justificar si las 200 repeticiones iniciales bastan.

El enunciado también exige perfilado con herramientas como perf o VTune
(ciclos, instrucciones, uso de CPU por hilo lógico, stalls) y comparación de SMT
habilitado/deshabilitado. Estos datos se recolectan con el perfilador.
Las repeticiones, estadísticas del tiempo e histograma están implementados.
Los modelos paralelos y el análisis comparativo de speedup/eficiencia todavía
están pendientes; una sola configuración secuencial no permite compararlos.

El cierre comprueba los errores de escritura, pero no fuerza persistencia física
con `fsync`; el sistema operativo puede mantener datos en su caché de archivos.
Todos los modelos futuros deberán conservar estas mismas condiciones.

### Repeticiones y análisis estadístico

Se requiere Python 3.8 o posterior, sin paquetes externos. C++ ejecuta la
simulación, Bash controla las repeticiones y Python analiza los tiempos.

```bash
make benchmark
```

Ejecuta 200 simulaciones completas consecutivas, sin ventana, y genera un lote
nuevo en `output/benchmark-FECHA-HORA-ID/`. Conserva el mismo estado inicial y
la misma captura por paso en todas las repeticiones. Se sobrescribe la trayectoria
del lote para conservar únicamente la última; las muestras y lotes anteriores
se mantienen. No ejecuta simulaciones en paralelo, ni descarta warmups u outliers.
Un proceso fallido detiene el lote y conserva las muestras ya completadas.

Para cambiar la entrada o la cantidad de repeticiones:

```bash
make benchmark REPETITIONS=200 BENCHMARK_ARGS="--bodies-per-galaxy 200 --steps 15000 --dt 0.002 --seed 42"
```

`REPETITIONS` permite valores desde 2 para pruebas rápidas; usar menos de 200
se indica explícitamente en el reporte y requiere justificación experimental.
`BENCHMARK_ARGS` acepta opciones físicas; el script controla la ruta de captura
y la ejecución sin ventana. Cada lote incluye:

| Archivo | Contenido |
|---|---|
| `samples.csv` | Número de repetición, configuración y tiempo individual |
| `summary.csv` | n, media, desviación estándar muestral e IC 95 % de la media |
| `histogram.svg` | Histograma de todos los tiempos, con frecuencias absolutas |
| `convergence.csv` | Media, desviación e IC para cada prefijo desde n=2 |
| `trajectory.nbody` | Trayectoria de la última repetición, lista para reproducir |
| `command.txt`, `environment.txt` | Comando, repeticiones, SO y topología de CPU si hay lscpu |
| `last-run.log` | Mensajes de la última simulación |

Puede recalcular el análisis sin repetir simulaciones:

```bash
python3 scripts/analyze.py output/benchmark-FECHA-HORA-ID/samples.csv
```

La desviación estándar utiliza el denominador `n-1`. El intervalo de confianza
bilateral para la media es `media ± t(0.975, n-1) * s / sqrt(n)`; no es un rango
que contenga el 95 % de las ejecuciones. El cuantil Student t se calcula por
integración numérica y bisección, con pruebas contra casos analíticos y la
[tabla de NIST](https://www.itl.nist.gov/div898/handbook/eda/section3/eda3672.htm).
La fórmula sigue los [límites de confianza para la media de NIST](https://www.itl.nist.gov/div898/handbook/eda/section3/eda352.htm).

Se supone que las mediciones son suficientemente independientes y que la media
puede aproximarse con este método. Para justificar el número de repeticiones,
revise `convergence.csv`, el semiancho del IC relativo a la media y el cambio
entre las medias de las dos mitades del lote, incluidos en el resumen.
Un IC estrecho por sí solo no demuestra estabilidad ni elimina sesgos por
carga, temperatura o cachés. El script no declara automáticamente convergencia:
la tolerancia aceptable debe justificarse en el paper. El histograma usa la
regla de Sturges para la cantidad de intervalos e incluye todas las muestras.

Las mediciones principales deben ejecutarse sobre hardware físico, como exige
el enunciado. Registre además compilador, banderas y estado de SMT para cada
experimento. La configuración predeterminada escribe unos 24 GB acumulados
en 200 ejecuciones, aunque solo conserva una trayectoria de unos 120 MB.

### Archivo de trayectoria

El formato binario v1 comienza con `NBODY001`, una marca de orden de bytes,
cantidad total de cuerpos, pasos, `dt` y semilla. Cada frame guarda, en orden,
masa, posición XY, velocidad XY, aceleración XY e identificador de galaxia de
cada cuerpo. El frame 0 incluye las aceleraciones iniciales; hay `steps + 1`
frames. El archivo usa doubles IEEE 754 de 64 bits y enteros de tamaño fijo,
sin volcar estructuras C++ ni su padding. Se rechaza otro orden de bytes.
El modelo v1 utiliza las constantes físicas documentadas arriba.

Tamaño: `40 + (steps + 1) * cuerpos * 60` bytes. La configuración predeterminada
ocupa aproximadamente 120 MB. Se escribe y reproduce un frame a la vez, con
memoria O(N). El lector verifica versión, dimensiones y tamaño exacto, y valida
los valores de cada frame al leerlo. Si la generación falla, puede quedar un
archivo parcial; no se publican métricas exitosas y el lector rechaza su tamaño
incompleto.

### Parámetros

| Opción | Descripción | Valor predeterminado |
|---|---|---:|
| `--bodies-per-galaxy N` | Cantidad de cuerpos en cada galaxia | `100` |
| `--steps N` | Cantidad de pasos temporales | `10000` |
| `--dt X` | Duración de cada paso | `0.002` |
| `--seed N` | Semilla de 32 bits para la inicialización | `42` |
| `--trajectory ARCHIVO` | Ruta de la trayectoria generada | `output/trajectory.nbody` |
| `--no-output` | Guarda y mide sin abrir el reproductor | desactivado |
| `--replay ARCHIVO` | Reproduce un archivo existente sin simular | — |

Ejemplo con 400 cuerpos totales:

```bash
./build/nbody --bodies-per-galaxy 200 --steps 15000 --dt 0.002
```

Ejecución del ejemplo anterior:

![400 cuerpos totales](docs/diagrams/Ejemplo.png)

La ayuda integrada muestra las opciones disponibles:

```bash
./build/nbody --help
```


## Opciones de Make

| Comando | Acción |
|---|---|
| `make` | Compila si es necesario, simula, guarda, imprime métricas y reproduce |
| `make build` | Solo compila `build/nbody` |
| `make benchmark` | Ejecuta 200 repeticiones y genera estadísticas e histograma |
| `make test` | Compila y ejecuta todas las pruebas |
| `make clean` | Elimina los ejecutables generados en `build/` |


## Estructura del proyecto

```text
n-body-sim/
├── include/nbody/       Interfaces y estructuras compartidas
├── src/                 Implementación secuencial y visualización
├── tests/               Pruebas físicas, de CLI, de ventana y estadísticas
├── scripts/             Repeticiones Bash y análisis Python
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
- [`src/trajectory.cpp`](src/trajectory.cpp): escritura y lectura de estados binarios.
- [`src/native_view.cpp`](src/native_view.cpp): reproductor SDL2 y controles,
  ejecutado después de finalizar las mediciones.
- [`src/output.cpp`](src/output.cpp): presentación textual de los
  diagnósticos.

## Variables que aumentan el paralelismo disponible

La cantidad de cuerpos es el único parámetro actual que aumenta el paralelismo
disponible por paso: cada cuerpo añade un cálculo de aceleración independiente,
que lee las posiciones de todos y escribe su propio resultado. Los pasos
adicionales dependen del estado anterior; `dt`, la semilla y la visualización
cambian la evolución física o las condiciones de ejecución, pero no agregan
tareas independientes por paso. 

### Cantidad de cuerpos

`--bodies-per-galaxy` determina el tamaño total del sistema:

```text
N = 2 × bodies_per_galaxy
```

El
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



## Diagramas de los modelos de ejecución

Los diagramas representan el diseño preliminar de los cuatro modelos. En ellos,
$N$ es la cantidad total de partículas, $B_i$ es un bloque de partículas, $M$
es la cantidad de hilos de grano grueso, $C$ es la cantidad de núcleos físicos
y $K$ es la cantidad de interacciones que una tarea Fine-Grained procesa durante
cada turno.

### Fine-Grained Multithreading

Se crea una tarea $P_i$ por cada partícula. El planificador las recorre en orden
*round-robin* y cada tarea calcula únicamente las siguientes $K$ interacciones
de su partícula durante su turno. Si todavía quedan interacciones pendientes,
la tarea conserva su progreso y espera la siguiente ronda. Cuando las $N$
tareas terminan, una barrera permite actualizar velocidades y posiciones antes
de comenzar el siguiente paso temporal.

![Diagrama de Fine-Grained Multithreading](docs/diagrams/fine-grained.png)

### Coarse-Grained Multithreading

Las $N$ partículas se dividen en $M$ bloques grandes y cada bloque $B_i$ se
asigna a un hilo. Cada hilo calcula todas las interacciones de las partículas de
su bloque antes de alcanzar la barrera. La barrera es el punto de sincronización
costoso del diagrama: solo después de que todos los hilos terminan se actualizan
velocidades y posiciones y comienza el siguiente paso temporal.

![Diagrama de Coarse-Grained Multithreading](docs/diagrams/coarse-grained.png)

### SMT — Simultaneous Multithreading

Con $C$ núcleos físicos, las partículas se dividen en $2C$ bloques para ejecutar
dos hilos lógicos por núcleo. Los dos hilos lógicos de cada núcleo comparten sus
recursos de ejecución, tal como indica el diagrama. Todos los hilos se reúnen en
una barrera después de calcular las fuerzas. La evaluación deberá comparar el
mismo trabajo con SMT habilitado y deshabilitado en el hardware.

![Diagrama de Simultaneous Multithreading](docs/diagrams/smt.png)

### CMP — Chip Multiprocessing

Las partículas se dividen en $C$ bloques y cada bloque se asigna a un hilo que
se ejecuta en un núcleo físico diferente. Así, los núcleos calculan fuerzas en
paralelo real sin compartir un mismo núcleo. Una barrera espera a que los $C$
bloques estén listos antes de actualizar velocidades y posiciones y avanzar al
siguiente paso temporal.

![Diagrama de Chip Multiprocessing](docs/diagrams/cmp.png)
