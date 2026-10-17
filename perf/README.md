# Perfilado general con perf

Desde la raíz, `make perf` compila los cuatro problemas y ejecuta, en serie,
sequential, fine, coarse, smt y cmp con tres repeticiones por configuración.
Requiere Linux, Python 3, perf compatible con el kernel y las dependencias de
compilación de los proyectos (C++17, SDL2 para N-body, Eigen y GDAL).
El reporte y las gráficas SVG no requieren paquetes Python adicionales.

```sh
make perf
make perf-nbody REPETITIONS=5 THREADS=4 SMT_THREADS=12
make perf-ray
make perf-dl
make perf-sentinel SLC_A=/ruta/A.tif SLC_B=/ruta/B.tif
make perf-report
```

Los CSV quedan directamente en esta carpeta:

```text
perf/
  nbody.csv
  ray.csv
  dl.csv
  sentinel.csv
  reporte.md
  plots/*.svg
  raw/<problema-fechaUTC>/
    environment.json
    status.txt
    preflight-*.txt / *.log
    <modelo>-<repeticion>.stat / .stdout / .stderr / .json
```

Cada CSV usa formato largo: una fila por evento, modelo, repetición y CPU/PMU.
Incluye valor, unidad, disponibilidad, tiempo de contador, porcentaje medido,
tiempo de pared, trabajadores, estado SMT, comando y enlace a evidencia cruda.
Cada campaña reemplaza el CSV de su problema, conserva las capturas anteriores
y regenera el reporte de todos los CSV existentes. Los resultados generados
se versionan en Git junto con las capturas crudas; también se pueden compartir
copiando la carpeta `perf/`.
Una campaña interrumpida deja `status.txt=incomplete`; su CSV puede ser parcial
y no debe presentarse como una campaña terminada.

## Configuración

`THREADS=auto` selecciona hasta cuatro núcleos físicos permitidos para coarse y
CMP. `SMT_THREADS=auto` selecciona todas las CPUs lógicas permitidas para SMT.
Fine usa un solo hilo real con tareas cooperativas. El perfilador no cambia el
estado global SMT ni llama a sudo. Para comparar on/off, configure el equipo
por separado y copie los resultados de cada campaña antes de iniciar otra.

Variables principales del Makefile:

| Variable | Valor inicial / finalidad |
| --- | --- |
| `PROBLEMS` | `nbody ray dl sentinel`; selección para `make perf` |
| `MODELS` | `sequential fine coarse smt cmp` |
| `REPETITIONS` | `3`; ninguna repetición se descarta como calentamiento |
| `THREADS`, `SMT_THREADS` | `auto`; un P fijo por modelo en cada campaña |
| `EVENTS` | task-clock, cambios de contexto, migraciones, page faults, ciclos, instrucciones, referencias/fallos de caché, stalls frontend/backend |
| `SCOPE` | `process`; `cpu` añade desglose por CPU lógica a nivel del sistema |
| `NBODY_ARGS` | 500 cuerpos por galaxia, 1000 pasos, dt=0.002, semilla=42 |
| `RAY_ARGS` | 640×480, 8 muestras, 12 rebotes, tiles de 32 |
| `SENTINEL_ARGS` | ventana 24444×1499, tiles 32×32, admite NoData |
| `SLC_A`, `SLC_B` | TIFF corregistrados; rutas relativas a la raíz o absolutas |
| `DL_EPOCHS`, `DL_SAMPLES`, `DL_SEED` | `1000`, `100`, `1` |
| `NBODY_QUANTUM`, `RAY_QUANTUM`, `SENTINEL_QUANTUM` | `1`, `64`, `2` operaciones por turno |
| `DL_QUANTUM_US` | `100` microsegundos por turno |
| `BLOCK_INTERVAL_MS`, `BLOCK_DURATION_MS` | `10`, `1`; pausas coarse en N-body, ray y DL |

No añada opciones de modelo, hilos, salida o benchmark dentro de `*_ARGS`:
las controla el perfilador. El entrenamiento fine de DL puede tardar varios
minutos por repetición con la carga inicial. Ejemplo de comprobación corta:

```sh
make perf PROBLEMS='nbody ray dl' REPETITIONS=1 THREADS=2 SMT_THREADS=2 \
  NBODY_ARGS='--bodies-per-galaxy 20 --steps 10' \
  RAY_ARGS='--width 64 --height 48 --samples 1 --bounces 2 --tile-size 16' \
  DL_EPOCHS=1 DL_SAMPLES=5
```

Para medir actividad por CPU lógica:

```sh
make perf-ray SCOPE=cpu REPETITIONS=3
```

Este modo utiliza `perf stat -a -A -C` sobre la afinidad permitida y requiere
permisos para contadores del sistema. Incluye otros procesos; **no atribuye
esa actividad exclusivamente al problema**. `cpu-clock` sustituye a task-clock.
En modo process, task-clock / tiempo de pared estima CPUs lógicas equivalentes
ocupadas por el proceso y sus hilos, sin desglosarlas por ID. No es una medición
de utilización individual de cada hilo lógico.

## Disponibilidad e interpretación

Antes de medir, se verifica cada evento con perf. Eventos no soportados se
registran sin valor como `no_soportado`; `<not counted>` se preserva. Los fallos
de permisos detienen la campaña y guardan el diagnóstico. Use `perf list` para
seleccionar eventos de su procesador, por ejemplo:

```sh
make perf-ray EVENTS=task-clock,context-switches,cycles,instructions
```

Si `perf_event_paranoid` o las capacidades del sistema impiden abrir contadores,
el administrador debe habilitar el acceso apropiado antes de medir. El script
no cambia permisos ni sustituye datos reales por datos simulados.

El reporte generado incluye medias, desviaciones, cobertura, tiempo de pared,
uso agregado del proceso, gráficas por evento/PMU y una discusión de cómo leer
los contadores para cada modelo. Conserve la misma entrada y estado del equipo
para comparar modelos. Los contadores cubren también lectura de datos y salida:
no equivalen al kernel aislado. Stalls y fallos de caché ayudan a formular
hipótesis; no identifican por sí solos una causa. En CPUs híbridas se mantienen
separados los eventos de cada PMU, y el porcentaje medido permite advertir
conteos extrapolados desde intervalos pequeños.

`make test-perf` verifica el parser, fallos de permisos, eventos ausentes y una
campaña de cinco modelos con perf simulado, sin medir ni alterar el sistema.
