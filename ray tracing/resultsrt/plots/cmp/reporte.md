# Análisis de repeticiones

Carga fija: ancho=640, alto=480, tile_ancho=32, tile_alto=32, muestras=8, rebotes=12, semilla=2026.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo.

| Configuración | n | Referencia 200 | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| cmp · P=1 · C=8 L=12 SMT=1 | 200 | alcanzada | 1099.2629 ± 2.9445 | 21.2453 | 1.0145 ± 0.0033 | 0.0240 |
| cmp · P=2 · C=8 L=12 SMT=1 | 200 | alcanzada | 567.2380 ± 0.5334 | 3.8486 | 1.9661 ± 0.0041 | 0.0299 |
| cmp · P=4 · C=8 L=12 SMT=1 | 200 | alcanzada | 339.7869 ± 0.6821 | 4.9216 | 3.2822 ± 0.0090 | 0.0652 |
| cmp · P=8 · C=8 L=12 SMT=1 | 200 | alcanzada | 233.2642 ± 0.4931 | 3.5578 | 4.7811 ± 0.0135 | 0.0977 |

## Justificación y alcance

Se eligen inicialmente 200 repeticiones por configuración para cumplir la referencia del enunciado, caracterizar la variabilidad y estimar la precisión de la media. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. 200 no garantiza precisión suficiente: en el informe se debe evaluar el margen relativo (margen/media) de tiempo y speedup, y aumentar las repeticiones si es excesivo. Una prueba con menos de 200 sirve para validar el funcionamiento, no se presenta como campaña final.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener escena, semilla, resolución, tiles, muestras, rebotes y política constantes durante cada barrido. El calentamiento no se cuenta. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.
