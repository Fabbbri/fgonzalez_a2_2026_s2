# Análisis de repeticiones

Carga fija: epocas=1000, muestras=100, learning_rate=0.1, semilla=1, dataset_hash=02def62d38696cddf2715e05122f741fafee2c190e42772b91f7c0c2f6604c7e.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo.

| Configuración | n | n referencia | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| coarse · P=1 · bloqueo=10/1 ms · C=8 L=12 SMT=1 | 20 | 20 | 46305.3105 ± 523.1639 | 1193.7040 | 0.6297 ± 0.0193 | 0.0440 |
| coarse · P=2 · bloqueo=10/1 ms · C=8 L=12 SMT=1 | 20 | 20 | 54237.2392 ± 448.5340 | 1023.4210 | 0.5376 ± 0.0159 | 0.0363 |
| coarse · P=4 · bloqueo=10/1 ms · C=8 L=12 SMT=1 | 20 | 20 | 81390.8301 ± 1102.5381 | 2515.6634 | 0.3582 ± 0.0113 | 0.0258 |
| coarse · P=8 · bloqueo=10/1 ms · C=8 L=12 SMT=1 | 20 | 20 | 153634.5782 ± 4062.7132 | 9269.9009 | 0.1898 ± 0.0074 | 0.0168 |

## Justificación y alcance

El tamaño n se obtiene de las filas del CSV para cada modelo, política, estado SMT y cantidad de trabajadores. n referencia cuenta las mediciones secuenciales utilizadas. No se supone una cantidad fija ni se completan muestras artificialmente. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. Evaluar la precisión con el margen relativo (margen/media) de tiempo y speedup; un número fijo de repeticiones no garantiza precisión suficiente.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener épocas, muestras, learning rate, dataset, semilla y política constantes durante cada barrido. El calentamiento no se cuenta. El tiempo incluye creación/cierre de trabajadores, entrenamiento y sincronización; excluye carga de MNIST, inicialización, salida por época, exportación de parámetros, hash y gráficas. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
