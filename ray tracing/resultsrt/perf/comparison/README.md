# Comparación general de perf

Carga: `--samples 8`.

Conteo escalado por perf (puede extrapolar) · alcance process · barras negras: IC95 aproximado de la media.
Rayado naranja: porcentaje medido bajo o desconocido; valores conservados. P = trabajadores.
P-cores y E-cores se comparan por separado. IPC mayor o menos eventos no garantiza menor tiempo.

Cada fila conserva la configuración y sus repeticiones; no se mezclan cantidades de trabajadores ni estados SMT.
Las referencias compartidas se muestran una sola vez desde su captura original.
Campañas independientes pueden tener sesgos por temperatura, frecuencia u orden. Los IC no corrigen esos sesgos.

| Categoría | Gráfica |
| --- | --- |
| Instrucciones por ciclo (IPC) | [Abrir](comparacion-ipc.png) |
| Ciclos de CPU | [Abrir](comparacion-cycles.png) |
| Instrucciones completadas | [Abrir](comparacion-instructions.png) |
| Fallos de caché | [Abrir](comparacion-cache-misses.png) |
| Cambios de contexto | [Abrir](comparacion-context-switches.png) |
| Migraciones entre CPUs | [Abrir](comparacion-cpu-migrations.png) |
| Tiempo CPU acumulado | [Abrir](comparacion-task-clock.png) |

## Capturas incluidas

- `cmp-p1`
- `cmp-p2`
- `cmp-p4`
- `cmp-p8`
- `coarse-p1`
- `coarse-p2`
- `coarse-p4`
- `coarse-p8`
- `fine`
- `sequential`
- `smt-off-p12`
- `smt-off-p8`
- `smt-on-p12`
- `smt-on-p8`
