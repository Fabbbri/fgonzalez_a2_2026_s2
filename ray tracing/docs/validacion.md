# Validación de los modelos por tiles

Comprobaciones del 6 de octubre de 2026 para sequential, fine, coarse, SMT y CMP.

- Compilación C++17 con SDL2 y variante sin SDL2: correctas.
- `make test`: imagen idéntica entre modelos, bordes parciales, K = 1, 7 y
  mayor que el tile, orden round-robin, reanudación, callbacks en el principal,
  cancelación durante una espera de 10 segundos y propagación de errores.
- Métricas: pruebas con tiempos conocidos verifican speedup, eficiencia,
  desviación muestral e intervalo de confianza aproximado del 95 %.
  Los CSV se comprueban con mediciones reales de prueba y sus cocientes.
- Coarse con 1, 2 y 4 trabajadores: se conserva la imagen y la partición en
  tiles. Se ejecutó el barrido también con la compilación sin SDL2 a 41 × 29
  y tiles de 9 × 9, comprobando bordes parciales y exportación de CSV.
- Fine: ejecución de prueba de 200 repeticiones a 11 × 7 para verificar el
  valor predeterminado del benchmark. Es una carga pequeña de validación,
  no un experimento representativo del rendimiento de la escena completa.
- SMT/CMP: topologías sintéticas verifican selección de núcleos distintos,
  hermanos lógicos, sobresuscripción y límite P <= C para CMP. Se comprobó
  afinidad real con `sched_getcpu` sin alterar la del principal.
- Ambos modelos conservan la imagen y los callbacks en el principal. Se
  comprobaron cancelación antes de iniciar y durante el render, y propagación
  de errores de afinidad y callbacks después de hacer join.
- Integración SMT/CMP: cinco repeticiones con barridos CMP P = 1, 2, 4 y SMT
  P = 8, 12, 16, con CSV y gráficas automáticas. Se ejecutaron además 200
  repeticiones por modelo a 41 × 29, tiles 9 × 9, una muestra y cuatro rebotes.
  Son pruebas funcionales; no se usan sus tiempos como resultados experimentales.
- Restricción a una CPU con `taskset`: SMT detectó C = 1 y H = 1 y usó dos
  trabajadores sobre esa CPU, conservando la imagen. Esto comprueba la
  sobresuscripción, no sustituye deshabilitar SMT de hardware en BIOS.
- Las gráficas separan campañas con distintos estados SMT o topologías,
  comprobado con `make test-plots`.
- Gráficas: `make plots` generó boxplots, histogramas, curvas con IC95 y un
  reporte a partir de los CSV existentes de cinco iteraciones. Se revisó
  visualmente la curva de escalabilidad. Las gráficas se generan sin ventana.
- `make test-plots`: comprueba inclusión de valores atípicos, estadística de
  cocientes pareados, exclusión de archivos de resumen y rechazo de cargas
  distintas o métricas inconsistentes. No requiere Matplotlib.
- `make WITH_SDL=0 benchmark-plots`: comprobado con 200 repeticiones de coarse
  para P = 1, 2 y 4, resolución 41 × 29, tiles 9 × 9, una muestra y cuatro
  rebotes. Es una prueba de integración; no una campaña final de rendimiento.
  Para verificar las gráficas se usó Matplotlib en un entorno Python temporal.
- Generación automática: `make benchmark` y su alias `make benchmark-plots`
  comprobados con cinco repeticiones, tanto fine como coarse. Se respetó
  `--metrics-output` y se conservaron ocho gráficas y el reporte de cada modelo
  en carpetas separadas, sin que una ejecución sobrescribiera la otra.
- `make test-display`: presentación y eventos con `SDL_VIDEODRIVER=dummy`.
  Esto prueba SDL sin comprobar una ventana visible en un escritorio real.
- Comparación de los PPM de la escena completa a 641 × 479, tiles de 32 × 23,
  dos muestras y seis rebotes: los tres modelos generan los mismos bytes.
  Coarse creó 441 hilos y se comprobó ejecución de bloqueos de 1 ms.
- A la misma resolución 641 × 479 se compararon los PPM de sequential, CMP
  con ocho trabajadores y SMT con doce: resultados idénticos byte por byte.
- AddressSanitizer y UndefinedBehaviorSanitizer: pruebas correctas con
  `ASAN_OPTIONS=detect_leaks=0`. LeakSanitizer no pudo ejecutarse bajo el ptrace
  del entorno; esa ejecución no comprueba fugas. Se repitieron las pruebas
  con el benchmark, coarse configurable y los modelos SMT/CMP.
- ThreadSanitizer no pudo iniciar: `unexpected memory mapping`. No se afirma
  haber descartado carreras mediante esa herramienta; queda pendiente correrla
  en un entorno compatible.

Los tiempos de estas pruebas son comprobaciones funcionales. No constituyen
las mediciones experimentales repetidas en hardware físico que pide el proyecto.
La campaña principal con SMT activado/desactivado en BIOS, las capturas reales de
perfilado y su análisis siguen pendientes. La ventana se verificó con SDL dummy; no se inspeccionó
una ventana visible en un escritorio real.

## Integración de perf: 7 de octubre de 2026

- `make test-profile`: lectura de contadores por CPU/PMU, porcentajes activos,
  eventos no soportados, derivación IPC sin cruzar CPU/PMU y conservación de la
  carga de referencia secuencial.
- Integración del comando `make profile` en alcances process y cpu: dos
  repeticiones de SMT y CMP respectivamente, junto con sequential. Se usó
  una fixture que invoca el renderizador real y escribe contadores ficticios
  conocidos. Se verificaron capturas, metadata, CSV, IPC, reporte y boxplots.
  Esos datos están en build y no constituyen resultados de perfilado real.
- Perf instalado: versión 6.17.13. La prueba real de apertura de eventos falla
  con `perf_event_paranoid=4`; ni siquiera task-clock pudo medirse. El script
  detectó el bloqueo antes de renderizar y no modificó permisos ni fabricó
  una campaña válida. Queda pendiente ejecutar las capturas con acceso a la
  PMU en el equipo físico, como se describe en perfilado.md.

## Presentación de gráficas

- Regenerados los CSV existentes de fine, coarse y CMP con la misma carga:
  barras de tiempo, speedup y eficiencia; curvas separadas; boxplots e histogramas.
  Las medias, referencias pareadas e IC95 siguen usando los mismos datos.
- Inspección visual de las gráficas de tiempo y speedup: valores visibles,
  segundos, referencia 1×, colores consistentes, configuración y repeticiones.
- Regenerada una captura perf existente usando `make profile-plots`, sin ejecutar
  perf ni cambiar sus contadores. Se revisó visualmente el IPC y se conservaron
  las PMU separadas. Nuevas barras con IC95 y boxplots de apoyo.
- `make test-plots` y `make test-profile`: correctos. Estos cambios afectan la
  presentación y documentación; no modifican el renderizador ni sus métricas.

## Organización de src

- Fuentes agrupadas en app, scene, render, execution y metrics, con main.cpp
  como entrada. Includes, Makefile, dependencias y rutas del README actualizados.
- Compilación con SDL2 y sin SDL2: correctas. `make test` y `make test-display`
  pasan después del cambio; SDL se comprobó con el controlador dummy.
- Los cinco modelos a 41 × 29, tiles 9 × 9, una muestra y cuatro rebotes
  producen PPM idénticos a la imagen guardada antes de reorganizar los archivos.
