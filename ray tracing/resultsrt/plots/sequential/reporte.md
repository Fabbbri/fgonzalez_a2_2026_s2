# Análisis de repeticiones

Carga fija: ancho=640, alto=480, tile_ancho=32, tile_alto=32, muestras=8, rebotes=12, semilla=2026.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
La referencia es sequential, medida en cada par de ejecuciones.

| Configuración | n | Referencia 200 | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| sequential · P=1 · base | 200 | alcanzada | 1115.2564 ± 2.1042 | 15.1824 | 1.0000 ± 0.0000 | 0.0000 |

## Estadística y alcance

El margen normal aproximado es 1.96 × desviación muestral / sqrt(n). Bajo independencia, el error estándar disminuye al aumentar n. El margen relativo (margen/media) expresa la precisión estimada de tiempo y speedup. Las configuraciones con menos de 200 ejecuciones se identifican como pruebas cortas.

Los grupos separan resolución, tiles, muestras, rebotes, semilla y política. El calentamiento no se cuenta. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos; el IC aproximado no corrige esos efectos ni la interferencia de otros procesos.
