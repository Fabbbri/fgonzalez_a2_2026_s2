# Framework experimental de modelos de ejecución multithreading

Proyecto de **Arquitectura de Computadores II** para comparar ejecución
secuencial, Fine-Grained, Coarse-Grained, SMT y CMP en cuatro problemas:

| Problema | Implementación |
| --- | --- |
| N-body | Interacción gravitacional de dos galaxias en 2D y reproducción de trayectorias con SDL2. |
| Ray tracing | Renderizado de una escena con reparto de píxeles por tiles. |
| Deep Learning | Entrenamiento SGD de una red neuronal para clasificar dígitos MNIST. |
| Sentinel-1 SLC | Producto conjugado de muestras radar complejas leídas con GDAL. |

Las implementaciones están escritas en C++17. Los scripts de Python y Bash
automatizan mediciones, validación de resultados, reportes y gráficas de tiempo,
speedup, eficiencia y escalabilidad, con análisis de la ley de Amdahl.
Fine-Grained utiliza planificación cooperativa en un hilo real; SMT y CMP
distribuyen trabajadores según la topología y afinidad disponibles.

## Estructura

```text
.
├── Makefile              # Compilación y perfilado general
├── n-body-sim/           # Simulación, reproductor, pruebas y resultados
├── ray tracing/          # Renderizador, pruebas y resultados
├── DL/                   # Red neuronal, datos MNIST y resultados
├── sentinel1-SLC/        # Procesamiento radar, scripts y resultados
├── scripts/              # Perfilador común con perf y sus pruebas
├── perf/                 # CSV por problema, capturas, reporte y gráficas
├── defensa/              # Presentación web local de los cuatro problemas
├── graficas.md           # Galería de resultados experimentales
└── video/                # Material de presentación y guion
```

## Compilación y ejecución

Se requiere un compilador C++17, GNU Make y Python 3. Las dependencias específicas
son SDL2 para N-body y la visualización de ray tracing, Eigen para Deep Learning
y GDAL para Sentinel. Los scripts de análisis utilizan NumPy y Matplotlib.
El perfilado requiere Linux, perf y permisos de acceso a sus contadores.
MNIST y los TIFF de Sentinel deben estar disponibles.

Desde la raíz:

```bash
make build                 # Compilar los cuatro problemas
make perf                  # Medir los cinco modelos de cada problema
make perf-report           # Regenerar el reporte y las gráficas de perf
```


## Resultados y documentación

Abre [defensa/index.html](defensa/index.html) en un navegador para consultar las
tablas y gráficas de los cuatro problemas, sin servidor ni conexión a Internet.
También puedes consultar la [galería de gráficas](graficas.md).
El perfilado con perf guarda un CSV por problema, evidencia original y un reporte
con discusión de los contadores; consulta sus opciones en [perf/README.md](perf/README.md).

- [N-body: ejecución y modelos](n-body-sim/README.md).
- [Ray tracing: ejecución y diseño](ray%20tracing/README.md).
- [Deep Learning: ejecución](DL/src/README.md) y [fundamentos](DL/docs/DOCUMENTACION.md).
- [Sentinel-1 SLC: procesamiento y datos](sentinel1-SLC/interferogram/docs/README.md).

Las comparaciones requieren mantener la misma carga y registrar trabajadores,
estado SMT y alcance de medición. Los tiempos instrumentados con perf cubren el
proceso completo y deben distinguirse de los tiempos internos de cada algoritmo.
