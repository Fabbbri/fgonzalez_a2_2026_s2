# Análisis de repeticiones

Carga fija: ancho=24444, alto=1499, origen_x=0, origen_y=0, slc_a_hash=d1e69c7ee86c24b7bca70e2c13d6cb32cfc8e229ef1ec1ab7402734543315fe9, slc_b_hash=895fa59bc9b4ee11c2dc6c8371a2d17e8aa24354fa2901705dc49b4dfdaa3432, allow_nodata=1.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado. En la comparación de modelos, el tiempo secuencial aparece en su propia fila, sin repetirlo junto a cada modelo. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo.

| Configuración | n | n referencia | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| cmp · P=1 · C=8 L=12 SMT=1 | 200 | 200 | 119.0395 ± 0.3711 | 2.6777 | 0.6907 ± 0.0051 | 0.0368 |
| cmp · P=2 · C=8 L=12 SMT=1 | 200 | 200 | 95.4481 ± 1.0075 | 7.2692 | 0.8615 ± 0.0108 | 0.0777 |
| cmp · P=4 · C=8 L=12 SMT=1 | 200 | 200 | 89.2815 ± 0.3641 | 2.6272 | 0.9210 ± 0.0072 | 0.0520 |
| cmp · P=8 · C=8 L=12 SMT=1 | 200 | 200 | 90.9103 ± 0.1922 | 1.3868 | 0.9045 ± 0.0063 | 0.0458 |
| coarse · P=1 · tiles=32×32 · C=8 L=12 SMT=1 | 200 | 200 | 130.2851 ± 2.1504 | 15.5157 | 0.6311 ± 0.0112 | 0.0811 |
| coarse · P=2 · tiles=32×32 · C=8 L=12 SMT=1 | 200 | 200 | 99.6933 ± 1.1513 | 8.3071 | 0.8248 ± 0.0110 | 0.0794 |
| coarse · P=4 · tiles=32×32 · C=8 L=12 SMT=1 | 200 | 200 | 89.1539 ± 0.4271 | 3.0817 | 0.9223 ± 0.0076 | 0.0547 |
| coarse · P=8 · tiles=32×32 · C=8 L=12 SMT=1 | 200 | 200 | 91.0511 ± 0.3218 | 2.3219 | 0.9031 ± 0.0068 | 0.0493 |
| fine · P=1 · quantum=2 píxeles · C=8 L=12 SMT=1 | 200 | 200 | 578.6986 ± 2.0438 | 14.7468 | 0.1421 ± 0.0011 | 0.0078 |
| sequential · P=1 · base · C=8 L=12 SMT=1 | 200 | 200 | 82.2239 ± 0.5498 | 3.9669 | 1.0000 ± 0.0000 | 0.0000 |
| smt · P=8 · C=8 L=12 SMT=1 | 200 | 200 | 89.2119 ± 0.2664 | 1.9220 | 0.9217 ± 0.0067 | 0.0487 |
| smt · P=12 · C=8 L=12 SMT=1 | 200 | 200 | 89.8981 ± 0.1508 | 1.0881 | 0.9146 ± 0.0063 | 0.0455 |
| smt · P=8 · C=8 L=8 SMT=0 | 200 | 200 | 88.5604 ± 0.4512 | 3.2554 | 0.9285 ± 0.0078 | 0.0563 |
| smt · P=12 · C=8 L=8 SMT=0 | 200 | 200 | 88.7698 ± 0.2278 | 1.6440 | 0.9263 ± 0.0066 | 0.0479 |

## Justificación y alcance

El tamaño n se obtiene de las filas del CSV para cada modelo, política, estado SMT y cantidad de trabajadores. n referencia cuenta las mediciones secuenciales utilizadas. No se supone una cantidad fija ni se completan muestras artificialmente. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. Evaluar la precisión con el margen relativo (margen/media) de tiempo y speedup; un número fijo de repeticiones no garantiza precisión suficiente.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener TIFF A/B, ventana, tiles, quantum y política NoData constantes durante cada barrido. El calentamiento no se cuenta. El tiempo principal incluye preparación, creación de trabajadores, cálculo, sincronización y cierre; excluye lectura de TIFF, exportación, resumen y huella. El CSV también registra el tiempo del proceso completo por separado, sin exportar TIFF durante las mediciones. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
