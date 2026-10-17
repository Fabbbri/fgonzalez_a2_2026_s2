# Gráficas de resultados experimentales

Material complementario del estudio de modelos de ejecución secuencial, Fine-Grained, Coarse-Grained, SMT y CMP. Esta galería reúne las 20 gráficas incluidas en `defensa/resultados/`, con sus reportes de mediciones.


## Índice

- [Deep Learning](#dl)
- [Simulación gravitacional N-body](#nbody)
- [Ray Tracing](#ray)
- [Procesamiento de imágenes Sentinel-1 SLC](#sentinel)

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

![Sentinel-1, equipo B (8533 MT/s): Tiempo de ejecución](defensa/resultados/sentinel_equipo_b/comparacion-tiempo.png)

#### Aceleración (speedup)

![Sentinel-1, equipo B (8533 MT/s): Aceleración (speedup)](defensa/resultados/sentinel_equipo_b/comparacion-speedup.png)

#### Eficiencia paralela

![Sentinel-1, equipo B (8533 MT/s): Eficiencia paralela](defensa/resultados/sentinel_equipo_b/comparacion-eficiencia.png)

#### Escalabilidad

![Sentinel-1, equipo B (8533 MT/s): Escalabilidad](defensa/resultados/sentinel_equipo_b/escalabilidad.png)
