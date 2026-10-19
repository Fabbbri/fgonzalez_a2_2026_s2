# Análisis de repeticiones

Carga fija: epocas=1000, muestras=100, learning_rate=0.1, semilla=1, dataset_hash=02def62d38696cddf2715e05122f741fafee2c190e42772b91f7c0c2f6604c7e.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo.

| Configuración | n | n referencia | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| smt · P=8 · C=8 L=12 SMT=1 | 20 | 20 | 18582.8629 ± 88.7020 | 202.3915 | 1.5690 ± 0.0453 | 0.1033 |
| smt · P=12 · C=8 L=12 SMT=1 | 20 | 20 | 15042.1925 ± 370.7828 | 846.0159 | 1.9383 ± 0.0730 | 0.1665 |

## Justificación y alcance

El tamaño n se obtiene de las filas del CSV para cada modelo, política, estado SMT y cantidad de trabajadores. n referencia cuenta las mediciones secuenciales utilizadas. No se supone una cantidad fija ni se completan muestras artificialmente. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. Evaluar la precisión con el margen relativo (margen/media) de tiempo y speedup; un número fijo de repeticiones no garantiza precisión suficiente.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener épocas, muestras, learning rate, dataset, semilla y política constantes durante cada barrido. El calentamiento no se cuenta. El tiempo incluye creación/cierre de trabajadores, entrenamiento y sincronización; excluye carga de MNIST, inicialización, salida por época, exportación de parámetros, hash y gráficas. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
