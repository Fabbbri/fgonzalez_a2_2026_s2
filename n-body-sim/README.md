# Simulación N-body de dos galaxias en 2D

Simulación en C++17 de interacción gravitacional completa, con costo **O(N²)
por paso**, suavizado gravitacional e integración Euler simpléctica. Los cinco
modelos comparten la física y generan trayectorias reproducibles con una semilla.
La distribución inicial es didáctica y no representa galaxias en equilibrio.

## Requisitos y ejecución

Se requiere un compilador C++17, GNU Make, SDL2 y pkg-config. Los scripts de
análisis y pruebas requieren Python 3, NumPy y Matplotlib; perf requiere Linux
y permisos para acceder a los contadores.

Desde `n-body-sim/`:

| Comando | Modelo y trabajadores predeterminados |
| --- | --- |
| `make sec` | Secuencial: un hilo. |
| `make fine` | Tareas cooperativas en un hilo real; quantum de una interacción. |
| `make coarse` | Dos trabajadores persistentes, con pausas de 1 ms cada intervalo de 10 ms. |
| `make smt` | Una CPU lógica por trabajador: L trabajadores si hay hermanos SMT disponibles; de lo contrario, 2 × C. |
| `make cmp` | C trabajadores, uno por núcleo físico disponible. |

C y L son los núcleos físicos y CPUs lógicas permitidos. Los trabajadores no
incluyen el hilo principal coordinador. SMT y CMP usan afinidad en Linux;
sobresuscribir CPUs no activa SMT de hardware.

Cada comando compila si hace falta, calcula y guarda la trayectoria, imprime
la duración y abre su reproducción visual. Por defecto usa **1000 cuerpos,
1000 pasos, dt=0.002 y semilla=42**: dos unidades de tiempo simulado.

```bash
make sec ARGS="--steps 10000"             # 20 unidades de tiempo simulado
make fine ARGS="--quantum 4"
make coarse ARGS="--threads 4"
make cmp ARGS="--threads 4 --no-output"   # Guarda la trayectoria sin ventana
make build                               # Solo compilar
make test                                # Ejecutar las pruebas
./build/nbody --topology                  # Consultar CPUs y estado SMT
./build/nbody --help                      # Todas las opciones
```

La trayectoria se guarda en `output/trajectory.nbody`. Para reproducirla sin
recalcular: `./build/nbody --replay output/trajectory.nbody`.
En la ventana: **Espacio** pausa, **R** reinicia, **F** encuadra, **+/−** cambia
la velocidad, la rueda ajusta el zoom y **Esc** sale. Cambiar la velocidad visual
no altera la física ni las métricas.

## Mediciones

La duración incluye cálculo, creación/cierre de trabajadores y escritura de la
trayectoria; excluye inicialización aleatoria y reproducción. También se imprime
una fila CSV con modelo, trabajadores, carga y tiempo total.

```bash
make benchmark SMT_STATES=keep           # CSV y gráficas en resultnbody/
make plots                               # Regenerar gráficas
make amdahl                              # Análisis de Amdahl
```

El benchmark usa 200 repeticiones por configuración por defecto; ajusta
`REPETITIONS`, `THREAD_COUNTS`, `CMP_THREAD_COUNTS` y `SMT_THREAD_COUNTS` según el
equipo. `SMT_STATES=keep` conserva el estado del hardware. El control explícito
`make smt-control SMT=on/off` afecta a todo el equipo y puede pedir sudo.

Para perf, ejecuta **`make perf-nbody` desde la raíz**; guarda CSV, capturas,
reporte y gráficas en `perf/`. Consulta [sus opciones](../perf/README.md).
Los [diagramas de ejecución](docs/diagrams/) complementan los modelos.
