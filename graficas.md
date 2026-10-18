# Gráficas de resultados experimentales

Material complementario del estudio de modelos de ejecución secuencial, Fine-Grained, Coarse-Grained, SMT y CMP. Esta galería reúne las 20 gráficas incluidas en `defensa/resultados/`, con sus reportes de mediciones. Al final se describe cómo se ejecutaron las repeticiones y cómo se generaron las gráficas.

## Índice

- [Deep Learning](#dl)
- [Simulación gravitacional N-body](#nbody)
- [Ray Tracing](#ray)
- [Procesamiento de imágenes Sentinel-1 SLC](#sentinel)
- [Repeticiones y generación de las gráficas](#metodo)

---

<a id="dl"></a>

## Deep Learning

[Reporte de mediciones](defensa/resultados/dl/reporte.md)

### Tiempo de ejecución

Menor tiempo indica mejor rendimiento.

![Deep Learning: tiempo de ejecución](defensa/resultados/dl/comparacion-tiempo.png)

### Aceleración (speedup)

Una aceleración mayor que 1 indica una mejora respecto a la referencia secuencial.

![Deep Learning: aceleración (speedup)](defensa/resultados/dl/comparacion-speedup.png)

### Eficiencia paralela

Relaciona la aceleración con la cantidad de trabajadores utilizados.

![Deep Learning: eficiencia paralela](defensa/resultados/dl/comparacion-eficiencia.png)

### Escalabilidad

Muestra el comportamiento al variar la cantidad de trabajadores.

![Deep Learning: escalabilidad](defensa/resultados/dl/escalabilidad.png)

---

<a id="nbody"></a>

## Simulación gravitacional N-body

[Reporte de mediciones](defensa/resultados/nbody/reporte.md)

### Tiempo de ejecución

Menor tiempo indica mejor rendimiento.

![Simulación gravitacional N-body: tiempo de ejecución](defensa/resultados/nbody/comparacion-tiempo.png)

### Aceleración (speedup)

Una aceleración mayor que 1 indica una mejora respecto a la referencia secuencial.

![Simulación gravitacional N-body: aceleración (speedup)](defensa/resultados/nbody/comparacion-speedup.png)

### Eficiencia paralela

Relaciona la aceleración con la cantidad de trabajadores utilizados.

![Simulación gravitacional N-body: eficiencia paralela](defensa/resultados/nbody/comparacion-eficiencia.png)

### Escalabilidad

Muestra el comportamiento al variar la cantidad de trabajadores.

![Simulación gravitacional N-body: escalabilidad](defensa/resultados/nbody/escalabilidad.png)

---

<a id="ray"></a>

## Ray Tracing

[Reporte de mediciones](defensa/resultados/ray/reporte.md)

### Tiempo de ejecución

Menor tiempo indica mejor rendimiento.

![Ray Tracing: tiempo de ejecución](defensa/resultados/ray/comparacion-tiempo.png)

### Aceleración (speedup)

Una aceleración mayor que 1 indica una mejora respecto a la referencia secuencial.

![Ray Tracing: aceleración (speedup)](defensa/resultados/ray/comparacion-speedup.png)

### Eficiencia paralela

Relaciona la aceleración con la cantidad de trabajadores utilizados.

![Ray Tracing: eficiencia paralela](defensa/resultados/ray/comparacion-eficiencia.png)

### Escalabilidad

Muestra el comportamiento al variar la cantidad de trabajadores.

![Ray Tracing: escalabilidad](defensa/resultados/ray/escalabilidad.png)

---

<a id="sentinel"></a>

## Procesamiento de imágenes Sentinel-1 SLC

### Equipo A: memoria de 3200 MT/s

[Reporte de mediciones](defensa/resultados/sentinel/reporte.md)

#### Tiempo de ejecución

Menor tiempo indica mejor rendimiento.

![Procesamiento de imágenes Sentinel-1 SLC: tiempo de ejecución](defensa/resultados/sentinel/comparacion-tiempo.png)

#### Aceleración (speedup)

Una aceleración mayor que 1 indica una mejora respecto a la referencia secuencial.

![Procesamiento de imágenes Sentinel-1 SLC: aceleración (speedup)](defensa/resultados/sentinel/comparacion-speedup.png)

#### Eficiencia paralela

Relaciona la aceleración con la cantidad de trabajadores utilizados.

![Procesamiento de imágenes Sentinel-1 SLC: eficiencia paralela](defensa/resultados/sentinel/comparacion-eficiencia.png)

#### Escalabilidad

Muestra el comportamiento al variar la cantidad de trabajadores.

![Procesamiento de imágenes Sentinel-1 SLC: escalabilidad](defensa/resultados/sentinel/escalabilidad.png)

### Equipo B: memoria de 8533 MT/s

Incluye secuencial, Fine-Grained, Coarse-Grained y CMP; no se midió el modelo SMT.

[Reporte de mediciones](defensa/resultados/sentinel_equipo_b/reporte.md)

#### Tiempo de ejecución

Menor tiempo indica mejor rendimiento.

![Sentinel-1, equipo B (8533 MT/s): Tiempo de ejecución](defensa/resultados/sentinel_equipo_b/comparacion-tiempo.png)

#### Aceleración (speedup)

Una aceleración mayor que 1 indica una mejora respecto a la referencia secuencial.

![Sentinel-1, equipo B (8533 MT/s): Aceleración (speedup)](defensa/resultados/sentinel_equipo_b/comparacion-speedup.png)

#### Eficiencia paralela

Relaciona la aceleración con la cantidad de trabajadores utilizados.

![Sentinel-1, equipo B (8533 MT/s): Eficiencia paralela](defensa/resultados/sentinel_equipo_b/comparacion-eficiencia.png)

#### Escalabilidad

Muestra el comportamiento al variar la cantidad de trabajadores.

![Sentinel-1, equipo B (8533 MT/s): Escalabilidad](defensa/resultados/sentinel_equipo_b/escalabilidad.png)

---

<a id="metodo"></a>

## Repeticiones y generación de las gráficas

Cada configuración (modelo de ejecución y cantidad de trabajadores) se ejecutó N repeticiones, donde N es el valor de `REPETITIONS`.

### Ejecución de las repeticiones

| Problema | Repeticiones | Comando | Script de repeticiones | Resultados |
| --- | --- | --- | --- | --- |
| Deep Learning | 9–20 | `make benchmark` (en `DL/src/`) | [`scripts/benchmark.py`](DL/src/scripts/benchmark.py) | `resultdl/` |
| N-body | 200 | `make benchmark` (en `n-body-sim/`) | [`scripts/benchmark.py`](n-body-sim/scripts/benchmark.py) | `resultnbody/` |
| Ray Tracing | 200 | `make run-all` (en `ray tracing/`) | [`scripts/run_all.sh`](ray%20tracing/scripts/run_all.sh) -> [`raytracer --benchmark --repetitions N`](ray%20tracing/src/metrics/benchmark.cpp) | `resultsrt/` |
| Sentinel-1 SLC | 200 | `make benchmark` (en `sentinel1-SLC/`) | [`scripts/benchmark.py`](sentinel1-SLC/scripts/benchmark.py) | `resultsen/` |

Por ejemplo, en Sentinel-1 el objetivo `benchmark` del [Makefile](sentinel1-SLC/Makefile) ejecuta:

```make
benchmark: $(TARGET) smt
	$(PYTHON) scripts/benchmark.py ./$(TARGET) --results-dir "$(OUTPUT)" \
		--models $(BENCHMARK_MODELS) --repetitions "$(REPETITIONS)" --warmups "$(WARMUPS)" \
		--thread-counts "$(THREAD_COUNTS)" --smt-states "$(SMT_STATES)" ... $(COMMON_ARGS)
```

y dentro de [`benchmark.py`](sentinel1-SLC/scripts/benchmark.py) cada configuración sigue este ciclo:

```python
for workers in workers_list:
    for _ in range(args.warmups): # calentamiento, no se guarda
        run(model, workers, label)
    for repetition in range(1, args.repetitions + 1):
        record, total, digest = run(model, workers, label) # una repetición medida
        writer.writerow(...) # una fila por repetición en el CSV
```

### Estadística y gráficas

`scripts/plot_metrics.py` de cada proyecto ([DL](DL/src/scripts/plot_metrics.py), [N-body](n-body-sim/scripts/plot_metrics.py), [Ray Tracing](ray%20tracing/scripts/plot_metrics.py), [Sentinel-1](sentinel1-SLC/scripts/plot_metrics.py)) lee los CSV (una fila por repetición) y genera automáticamente:

- Media, desviación estándar e IC95 de tiempo, speedup y eficiencia:

  $$\mathrm{IC}_{95} = \bar{x} \pm 1.96 \cdot \frac{s}{\sqrt{n}}$$

  donde $\bar{x}$ es la media, $s$ la desviación estándar y $n$ la cantidad de repeticiones.

- Comparación y escalabilidad: `comparacion-tiempo.png`, `comparacion-speedup.png`, `comparacion-eficiencia.png` y `escalabilidad.png`, las que aparecen en las secciones anteriores.
- Distribución de todas las iteraciones: `boxplot-*.png` e `histograma-*.png` por configuración, en `plots/comparison/` dentro de la carpeta de resultados de cada proyecto.
- Reporte: `reporte.md` con la tabla de $n$, $\bar{x} \pm \mathrm{IC}_{95}$ y desviación por configuración, y `amdahl.md` con el ajuste de la ley de Amdahl.

### Regenerar las gráficas

Desde los CSV ya medidos, sin volver a ejecutar los modelos:

```bash
make plots # tiempo, speedup, eficiencia, escalabilidad, boxplots, histogramas y reporte
make amdahl # solo el ajuste de Amdahl
```
