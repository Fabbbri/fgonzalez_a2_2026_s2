# Análisis de repeticiones

Carga fija: ancho=24444, alto=1499, origen_x=0, origen_y=0, slc_a_hash=ad590e8b3670e2b87fca50cf7d8db257f98a444d48b49a4f5fb1748061495a1a, slc_b_hash=4cd2b8aff29361b539bf6d3f2b170085a11a3814b0da736154a8c9db9bf96c1f, allow_nodata=1.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado. En la comparación de modelos, el tiempo secuencial aparece en su propia fila, sin repetirlo junto a cada modelo. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo.

| Configuración | n | n referencia | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| cmp · P=1 · C=8 L=8 SMT=0 | 200 | 200 | 86.6343 ± 1.5299 | 11.0390 | 0.7619 ± 0.0190 | 0.1372 |
| cmp · P=2 · C=8 L=8 SMT=0 | 200 | 200 | 50.4345 ± 0.4299 | 3.1017 | 1.3087 ± 0.0256 | 0.1849 |
| cmp · P=4 · C=8 L=8 SMT=0 | 200 | 200 | 38.5455 ± 0.0530 | 0.3826 | 1.7123 ± 0.0303 | 0.2185 |
| cmp · P=8 · C=8 L=8 SMT=0 | 200 | 200 | 36.6266 ± 0.0551 | 0.3978 | 1.8020 ± 0.0319 | 0.2300 |
| coarse · P=1 · tiles=32×32 · C=8 L=8 SMT=0 | 200 | 200 | 103.6021 ± 0.9475 | 6.8366 | 0.6371 ± 0.0127 | 0.0913 |
| coarse · P=2 · tiles=32×32 · C=8 L=8 SMT=0 | 200 | 200 | 60.5573 ± 0.3807 | 2.7467 | 1.0899 ± 0.0204 | 0.1472 |
| coarse · P=4 · tiles=32×32 · C=8 L=8 SMT=0 | 200 | 200 | 41.0238 ± 0.2738 | 1.9755 | 1.6089 ± 0.0303 | 0.2188 |
| coarse · P=8 · tiles=32×32 · C=8 L=8 SMT=0 | 200 | 200 | 36.8500 ± 0.2869 | 2.0699 | 1.7911 ± 0.0345 | 0.2490 |
| fine · P=1 · quantum=2 píxeles · C=8 L=8 SMT=0 | 200 | 200 | 442.1282 ± 6.0928 | 43.9618 | 0.1493 ± 0.0033 | 0.0241 |
| sequential · P=1 · base · C=8 L=8 SMT=0 | 200 | 200 | 66.0026 ± 1.1635 | 8.3947 | 1.0000 ± 0.0000 | 0.0000 |

## Justificación y alcance

El tamaño n se obtiene de las filas del CSV para cada modelo, política, estado SMT y cantidad de trabajadores. n referencia cuenta las mediciones secuenciales utilizadas. No se supone una cantidad fija ni se completan muestras artificialmente. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. Evaluar la precisión con el margen relativo (margen/media) de tiempo y speedup; un número fijo de repeticiones no garantiza precisión suficiente.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener TIFF A/B, ventana, tiles, quantum y política NoData constantes durante cada barrido. El calentamiento no se cuenta. El tiempo principal incluye preparación, creación de trabajadores, cálculo, sincronización y cierre; excluye lectura de TIFF, exportación, resumen y huella. El CSV también registra el tiempo del proceso completo por separado, sin exportar TIFF durante las mediciones. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
