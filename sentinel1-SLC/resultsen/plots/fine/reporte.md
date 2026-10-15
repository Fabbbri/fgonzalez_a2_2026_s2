# Análisis de repeticiones

Carga fija: ancho=24444, alto=1500, origen_x=0, origen_y=0, slc_a_hash=d1e69c7ee86c24b7bca70e2c13d6cb32cfc8e229ef1ec1ab7402734543315fe9, slc_b_hash=895fa59bc9b4ee11c2dc6c8371a2d17e8aa24354fa2901705dc49b4dfdaa3432, allow_nodata=1.

Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). Las barras muestran estimaciones con IC95 aproximado; las referencias secuenciales de tiempo se conservan por comparación, sin mezclar campañas. Los boxplots e histogramas muestran variabilidad y todas las repeticiones.

Se incluyen todas las iteraciones, sin eliminar valores atípicos.
Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. Su IC95 usa el método delta y considera la incertidumbre de ambas medias. No se interpreta cada cociente por ejecución como una observación independiente de la referencia. El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo.

| Configuración | n | n referencia | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |
| --- | ---: | --- | ---: | ---: | ---: | ---: |
| fine · P=1 · quantum=2 píxeles · C=8 L=12 SMT=1 | 200 | 200 | 583.1049 ± 2.2960 | 16.5662 | 0.1441 ± 0.0013 | 0.0097 |

## Justificación y alcance

El tamaño n se obtiene de las filas del CSV para cada modelo, política, estado SMT y cantidad de trabajadores. n referencia cuenta las mediciones secuenciales utilizadas. No se supone una cantidad fija ni se completan muestras artificialmente. El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); al aumentar n disminuye el error estándar si las muestras son independientes. Evaluar la precisión con el margen relativo (margen/media) de tiempo y speedup; un número fijo de repeticiones no garantiza precisión suficiente.

Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones de carga del equipo en el informe. Mantener TIFF A/B, ventana, tiles, quantum y política NoData constantes durante cada barrido. El calentamiento no se cuenta. El tiempo principal mide el kernel y su sincronización; excluye lectura de TIFF, creación/cierre de trabajadores, exportación, resumen y huella. El CSV también registra el tiempo del proceso completo por separado, sin exportar TIFF durante las mediciones. Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico.

La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md).
