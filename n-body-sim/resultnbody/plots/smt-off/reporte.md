# Análisis de repeticiones

Carga fija: cuerpos=1000, pasos=1000, dt=0.002, semilla=42.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. La referencia común conserva su estado SMT original; las diferencias entre fases también pueden incluir cambios de frecuencia o temperatura.

| Configuración | n | Referencia 200 | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| smt · P=8 · C=8 L=8 SMT=0 | 200 | alcanzada | 1153.0441 ± 8.2058 | 59.2083 | 2.1747 ± 0.0163 | 0.1173 |
| smt · P=12 · C=8 L=8 SMT=0 | 200 | alcanzada | 1014.8620 ± 16.5243 | 119.2287 | 2.4708 ± 0.0406 | 0.2932 |

## Estadística y alcance

El margen normal aproximado es 1.96 × desviación muestral / sqrt(n). Bajo independencia, el error estándar disminuye al aumentar n. El margen relativo (margen/media) expresa la precisión estimada de tiempo y speedup. Las configuraciones con menos de 200 ejecuciones se identifican como pruebas cortas.

Los grupos separan cuerpos, pasos, dt, semilla y política. El calentamiento no se cuenta. El tiempo incluye creación/cierre de trabajadores, cálculo, sincronización y captura de trayectoria; excluye inicialización aleatoria, hash y gráficas. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos; el IC aproximado no corrige esos efectos ni la interferencia de otros procesos.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
