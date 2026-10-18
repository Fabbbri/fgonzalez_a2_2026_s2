# Análisis de repeticiones

Carga fija: cuerpos=40, pasos=10, dt=0.002, semilla=42.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo.

| Configuración | n | Referencia 200 | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| coarse · P=1 · bloqueo=10/1 ms | 3 | prueba corta, pendiente | 0.4681 ± 0.2038 | 0.1801 | 0.6660 ± 0.3138 | 0.2773 |
| coarse · P=2 · bloqueo=10/1 ms | 3 | prueba corta, pendiente | 0.5788 ± 0.1042 | 0.0921 | 0.5386 ± 0.1371 | 0.1212 |
| coarse · P=4 · bloqueo=10/1 ms | 3 | prueba corta, pendiente | 0.7315 ± 0.0528 | 0.0466 | 0.4262 ± 0.0827 | 0.0731 |
| coarse · P=8 · bloqueo=10/1 ms | 3 | prueba corta, pendiente | 0.8467 ± 0.2503 | 0.2212 | 0.3682 ± 0.1274 | 0.1126 |

## Justificación y alcance

Se eligen inicialmente 200 repeticiones por configuración para cumplir la referencia del enunciado, caracterizar la variabilidad y estimar la precisión de la media. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. 200 no garantiza precisión suficiente: en el informe se debe evaluar el margen relativo (margen/media) de tiempo y speedup, y aumentar las repeticiones si es excesivo. Una prueba con menos de 200 sirve para validar el funcionamiento, no se presenta como campaña final.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener cuerpos, pasos, dt, semilla y política constantes durante cada barrido. El calentamiento no se cuenta. El tiempo incluye creación/cierre de trabajadores, cálculo, sincronización y captura de trayectoria; excluye inicialización aleatoria, hash y gráficas. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
