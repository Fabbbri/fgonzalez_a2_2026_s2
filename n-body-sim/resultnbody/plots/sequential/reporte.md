# Análisis de repeticiones

Carga fija: cuerpos=40, pasos=10, dt=0.002, semilla=42.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
La referencia sequential se compara consigo misma: speedup y eficiencia son 1.

| Configuración | n | Referencia 200 | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| sequential · P=1 · base | 3 | prueba corta | 0.3117 ± 0.0562 | 0.0496 | 1.0000 ± 0.0000 | 0.0000 |

## Estadística y alcance

El margen normal aproximado es 1.96 × desviación muestral / sqrt(n). Bajo independencia, el error estándar disminuye al aumentar n. El margen relativo (margen/media) expresa la precisión estimada de tiempo y speedup. Las configuraciones con menos de 200 ejecuciones se identifican como pruebas cortas.

Los grupos separan cuerpos, pasos, dt, semilla y política. El calentamiento no se cuenta. El tiempo incluye creación/cierre de trabajadores, cálculo, sincronización y captura de trayectoria; excluye inicialización aleatoria, hash y gráficas. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos; el IC aproximado no corrige esos efectos ni la interferencia de otros procesos.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
