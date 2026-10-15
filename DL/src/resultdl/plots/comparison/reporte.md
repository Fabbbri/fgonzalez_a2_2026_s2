# Análisis de repeticiones

Carga fija: epocas=1000, muestras=100, learning_rate=0.1, semilla=1, dataset_hash=02def62d38696cddf2715e05122f741fafee2c190e42772b91f7c0c2f6604c7e.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo.

| Configuración | n | n referencia | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| cmp · P=1 · C=8 L=12 SMT=1 | 10 | 20 | 26903.5655 ± 610.5238 | 985.0234 | 1.0837 ± 0.0394 | 0.0808 |
| cmp · P=2 · C=8 L=12 SMT=1 | 10 | 20 | 15856.9625 ± 264.5008 | 426.7474 | 1.8387 ± 0.0606 | 0.1292 |
| cmp · P=4 · C=8 L=12 SMT=1 | 9 | 20 | 11170.2793 ± 151.6272 | 232.0825 | 2.6102 ± 0.0823 | 0.1779 |
| cmp · P=8 · C=8 L=12 SMT=1 | 10 | 20 | 18835.1471 ± 323.6551 | 522.1874 | 1.5480 ± 0.0514 | 0.1093 |
| coarse · P=1 · bloqueo=10/1 ms · C=8 L=12 SMT=1 | 20 | 20 | 46305.3105 ± 523.1639 | 1193.7040 | 0.6297 ± 0.0193 | 0.0440 |
| coarse · P=2 · bloqueo=10/1 ms · C=8 L=12 SMT=1 | 20 | 20 | 54237.2392 ± 448.5340 | 1023.4210 | 0.5376 ± 0.0159 | 0.0363 |
| coarse · P=4 · bloqueo=10/1 ms · C=8 L=12 SMT=1 | 20 | 20 | 81390.8301 ± 1102.5381 | 2515.6634 | 0.3582 ± 0.0113 | 0.0258 |
| coarse · P=8 · bloqueo=10/1 ms · C=8 L=12 SMT=1 | 20 | 20 | 153634.5782 ± 4062.7132 | 9269.9009 | 0.1898 ± 0.0074 | 0.0168 |
| fine · P=1 · quantum=100 µs · C=8 L=12 SMT=1 | 10 | 20 | 418092.4493 ± 883.9495 | 1426.1703 | 0.0697 ± 0.0020 | 0.0045 |
| sequential · P=1 · base · C=8 L=12 SMT=1 | 20 | 20 | 29156.5252 ± 829.3950 | 1892.4321 | 1.0000 ± 0.0000 | 0.0000 |
| smt · P=8 · C=8 L=12 SMT=1 | 20 | 20 | 18582.8629 ± 88.7020 | 202.3915 | 1.5690 ± 0.0453 | 0.1033 |
| smt · P=12 · C=8 L=12 SMT=1 | 20 | 20 | 15042.1925 ± 370.7828 | 846.0159 | 1.9383 ± 0.0730 | 0.1665 |
| smt · P=8 · C=8 L=8 SMT=0 | 10 | 20 | 20465.1308 ± 234.9814 | 379.1206 | 1.4247 ± 0.0437 | 0.0962 |
| smt · P=12 · C=8 L=8 SMT=0 | 10 | 20 | 121230.5269 ± 817.1403 | 1318.3798 | 0.2405 ± 0.0070 | 0.0158 |

## Justificación y alcance

El tamaño n se obtiene de las filas del CSV para cada modelo, política, estado SMT y cantidad de trabajadores. n referencia cuenta las mediciones secuenciales utilizadas. No se supone una cantidad fija ni se completan muestras artificialmente. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. Evaluar la precisión con el margen relativo (margen/media) de tiempo y speedup; un número fijo de repeticiones no garantiza precisión suficiente.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener épocas, muestras, learning rate, dataset, semilla y política constantes durante cada barrido. El calentamiento no se cuenta. El tiempo incluye creación/cierre de trabajadores, entrenamiento y sincronización; excluye carga de MNIST, inicialización, salida por época, exportación de parámetros, hash y gráficas. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
