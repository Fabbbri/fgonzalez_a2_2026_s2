# Gráficas de resultados experimentales

Material complementario del estudio de modelos de ejecución secuencial, Fine-Grained, Coarse-Grained, SMT y CMP. Esta galería reúne las 20 gráficas incluidas en `defensa/resultados/`, agrupadas por tipo para comparar los cuatro problemas. Al final se describe cómo se ejecutaron las repeticiones y cómo se generaron las gráficas.

## Índice

- [Tiempo de ejecución](#tiempo)
- [Aceleración (speedup)](#speedup)
- [Eficiencia paralela](#eficiencia)
- [Escalabilidad](#escalabilidad)
- [Reportes de mediciones](#reportes)
- [Repeticiones y generación de las gráficas](#metodo)

---

<a id="tiempo"></a>

## Tiempo de ejecución

Menor tiempo indica mejor rendimiento.

<table>
  <tr>
    <th width="50%">Deep Learning</th>
    <th width="50%">N-body</th>
  </tr>
  <tr>
    <td><a href="defensa/resultados/dl/comparacion-tiempo.png"><img src="defensa/resultados/dl/comparacion-tiempo.png" width="100%" alt="Deep Learning: tiempo de ejecución"></a></td>
    <td><a href="defensa/resultados/nbody/comparacion-tiempo.png"><img src="defensa/resultados/nbody/comparacion-tiempo.png" width="100%" alt="N-body: tiempo de ejecución"></a></td>
  </tr>
  <tr>
    <th width="50%">Ray Tracing</th>
    <th width="50%">Sentinel-1, equipo A (3200 MT/s)</th>
  </tr>
  <tr>
    <td><a href="defensa/resultados/ray/comparacion-tiempo.png"><img src="defensa/resultados/ray/comparacion-tiempo.png" width="100%" alt="Ray Tracing: tiempo de ejecución"></a></td>
    <td><a href="defensa/resultados/sentinel/comparacion-tiempo.png"><img src="defensa/resultados/sentinel/comparacion-tiempo.png" width="100%" alt="Sentinel-1, equipo A (3200 MT/s): tiempo de ejecución"></a></td>
  </tr>
  <tr>
    <th colspan="2">Sentinel-1, equipo B (8533 MT/s; sin SMT)</th>
  </tr>
  <tr>
    <td colspan="2" align="center"><a href="defensa/resultados/sentinel_equipo_b/comparacion-tiempo.png"><img src="defensa/resultados/sentinel_equipo_b/comparacion-tiempo.png" width="50%" alt="Sentinel-1, equipo B (8533 MT/s; sin SMT): tiempo de ejecución"></a></td>
  </tr>
</table>

---

<a id="speedup"></a>

## Aceleración (speedup)

Una aceleración mayor que 1 indica una mejora respecto a la referencia secuencial.

<table>
  <tr>
    <th width="50%">Deep Learning</th>
    <th width="50%">N-body</th>
  </tr>
  <tr>
    <td><a href="defensa/resultados/dl/comparacion-speedup.png"><img src="defensa/resultados/dl/comparacion-speedup.png" width="100%" alt="Deep Learning: aceleración (speedup)"></a></td>
    <td><a href="defensa/resultados/nbody/comparacion-speedup.png"><img src="defensa/resultados/nbody/comparacion-speedup.png" width="100%" alt="N-body: aceleración (speedup)"></a></td>
  </tr>
  <tr>
    <th width="50%">Ray Tracing</th>
    <th width="50%">Sentinel-1, equipo A (3200 MT/s)</th>
  </tr>
  <tr>
    <td><a href="defensa/resultados/ray/comparacion-speedup.png"><img src="defensa/resultados/ray/comparacion-speedup.png" width="100%" alt="Ray Tracing: aceleración (speedup)"></a></td>
    <td><a href="defensa/resultados/sentinel/comparacion-speedup.png"><img src="defensa/resultados/sentinel/comparacion-speedup.png" width="100%" alt="Sentinel-1, equipo A (3200 MT/s): aceleración (speedup)"></a></td>
  </tr>
  <tr>
    <th colspan="2">Sentinel-1, equipo B (8533 MT/s; sin SMT)</th>
  </tr>
  <tr>
    <td colspan="2" align="center"><a href="defensa/resultados/sentinel_equipo_b/comparacion-speedup.png"><img src="defensa/resultados/sentinel_equipo_b/comparacion-speedup.png" width="50%" alt="Sentinel-1, equipo B (8533 MT/s; sin SMT): aceleración (speedup)"></a></td>
  </tr>
</table>

---

<a id="eficiencia"></a>

## Eficiencia paralela

Relaciona la aceleración con la cantidad de trabajadores utilizados.

<table>
  <tr>
    <th width="50%">Deep Learning</th>
    <th width="50%">N-body</th>
  </tr>
  <tr>
    <td><a href="defensa/resultados/dl/comparacion-eficiencia.png"><img src="defensa/resultados/dl/comparacion-eficiencia.png" width="100%" alt="Deep Learning: eficiencia paralela"></a></td>
    <td><a href="defensa/resultados/nbody/comparacion-eficiencia.png"><img src="defensa/resultados/nbody/comparacion-eficiencia.png" width="100%" alt="N-body: eficiencia paralela"></a></td>
  </tr>
  <tr>
    <th width="50%">Ray Tracing</th>
    <th width="50%">Sentinel-1, equipo A (3200 MT/s)</th>
  </tr>
  <tr>
    <td><a href="defensa/resultados/ray/comparacion-eficiencia.png"><img src="defensa/resultados/ray/comparacion-eficiencia.png" width="100%" alt="Ray Tracing: eficiencia paralela"></a></td>
    <td><a href="defensa/resultados/sentinel/comparacion-eficiencia.png"><img src="defensa/resultados/sentinel/comparacion-eficiencia.png" width="100%" alt="Sentinel-1, equipo A (3200 MT/s): eficiencia paralela"></a></td>
  </tr>
  <tr>
    <th colspan="2">Sentinel-1, equipo B (8533 MT/s; sin SMT)</th>
  </tr>
  <tr>
    <td colspan="2" align="center"><a href="defensa/resultados/sentinel_equipo_b/comparacion-eficiencia.png"><img src="defensa/resultados/sentinel_equipo_b/comparacion-eficiencia.png" width="50%" alt="Sentinel-1, equipo B (8533 MT/s; sin SMT): eficiencia paralela"></a></td>
  </tr>
</table>

---

<a id="escalabilidad"></a>

## Escalabilidad

Muestra el comportamiento al variar la cantidad de trabajadores.

<table>
  <tr>
    <th width="50%">Deep Learning</th>
    <th width="50%">N-body</th>
  </tr>
  <tr>
    <td><a href="defensa/resultados/dl/escalabilidad.png"><img src="defensa/resultados/dl/escalabilidad.png" width="100%" alt="Deep Learning: escalabilidad"></a></td>
    <td><a href="defensa/resultados/nbody/escalabilidad.png"><img src="defensa/resultados/nbody/escalabilidad.png" width="100%" alt="N-body: escalabilidad"></a></td>
  </tr>
  <tr>
    <th width="50%">Ray Tracing</th>
    <th width="50%">Sentinel-1, equipo A (3200 MT/s)</th>
  </tr>
  <tr>
    <td><a href="defensa/resultados/ray/escalabilidad.png"><img src="defensa/resultados/ray/escalabilidad.png" width="100%" alt="Ray Tracing: escalabilidad"></a></td>
    <td><a href="defensa/resultados/sentinel/escalabilidad.png"><img src="defensa/resultados/sentinel/escalabilidad.png" width="100%" alt="Sentinel-1, equipo A (3200 MT/s): escalabilidad"></a></td>
  </tr>
  <tr>
    <th colspan="2">Sentinel-1, equipo B (8533 MT/s; sin SMT)</th>
  </tr>
  <tr>
    <td colspan="2" align="center"><a href="defensa/resultados/sentinel_equipo_b/escalabilidad.png"><img src="defensa/resultados/sentinel_equipo_b/escalabilidad.png" width="50%" alt="Sentinel-1, equipo B (8533 MT/s; sin SMT): escalabilidad"></a></td>
  </tr>
</table>

---

<a id="reportes"></a>

## Reportes de mediciones

Cada reporte contiene la tabla de repeticiones, medias, desviaciones e IC95 de su problema.

- [Deep Learning](defensa/resultados/dl/reporte.md)
- [Simulación gravitacional N-body](defensa/resultados/nbody/reporte.md)
- [Ray Tracing](defensa/resultados/ray/reporte.md)
- [Sentinel-1 SLC, equipo A (3200 MT/s)](defensa/resultados/sentinel/reporte.md)
- [Sentinel-1 SLC, equipo B (8533 MT/s; sin SMT)](defensa/resultados/sentinel_equipo_b/reporte.md)

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
