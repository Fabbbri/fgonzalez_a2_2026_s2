# Análisis de repeticiones

Carga fija: cuerpos=40, pasos=10, dt=0.002, semilla=42.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. La referencia común conserva su estado SMT original; las diferencias entre fases también pueden incluir cambios de frecuencia o temperatura.

| Configuración | n | Referencia 200 | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| coarse · P=1 · bloqueo=10/1 ms | 3 | prueba corta | 0.4681 ± 0.2038 | 0.1801 | 0.6660 ± 0.3138 | 0.2773 |
| coarse · P=2 · bloqueo=10/1 ms | 3 | prueba corta | 0.5788 ± 0.1042 | 0.0921 | 0.5386 ± 0.1371 | 0.1212 |
| coarse · P=4 · bloqueo=10/1 ms | 3 | prueba corta | 0.7315 ± 0.0528 | 0.0466 | 0.4262 ± 0.0827 | 0.0731 |
| coarse · P=8 · bloqueo=10/1 ms | 3 | prueba corta | 0.8467 ± 0.2503 | 0.2212 | 0.3682 ± 0.1274 | 0.1126 |

## Estadística y alcance

El margen normal aproximado es 1.96 × desviación muestral / sqrt(n). Bajo independencia, el error estándar disminuye al aumentar n. El margen relativo (margen/media) expresa la precisión estimada de tiempo y speedup. Las configuraciones con menos de 200 ejecuciones se identifican como pruebas cortas.

Los grupos separan cuerpos, pasos, dt, semilla y política. El calentamiento no se cuenta. El tiempo incluye creación/cierre de trabajadores, cálculo, sincronización y captura de trayectoria; excluye inicialización aleatoria, hash y gráficas. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos; el IC aproximado no corrige esos efectos ni la interferencia de otros procesos.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
