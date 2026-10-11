# Análisis de repeticiones

Carga fija: ancho=640, alto=480, tile_ancho=32, tile_alto=32, muestras=8, rebotes=12, semilla=2026.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
La referencia es sequential, medida en cada par de ejecuciones.

| Configuración | n | Referencia 200 | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| sequential · P=1 · base | 200 | alcanzada | 1115.2564 ± 2.1042 | 15.1824 | 1.0000 ± 0.0000 | 0.0000 |

## Justificación y alcance

Se eligen inicialmente 200 repeticiones por configuración para cumplir la referencia del enunciado, caracterizar la variabilidad y estimar la precisión de la media. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. 200 no garantiza precisión suficiente: en el informe se debe evaluar el margen relativo (margen/media) de tiempo y speedup, y aumentar las repeticiones si es excesivo. Una prueba con menos de 200 sirve para validar el funcionamiento, no se presenta como campaña final.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener escena, semilla, resolución, tiles, muestras, rebotes y política constantes durante cada barrido. El calentamiento no se cuenta. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.
