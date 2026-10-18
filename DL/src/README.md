# DL: ejecución y benchmark

Desde `DL/src`, `make` compila los cinco programas. Se requiere Linux, Eigen 3,
Python 3, NumPy y Matplotlib. MNIST debe estar en `../data/mnist/` (IDX sin comprimir).
Los ejecutables conservan sus argumentos posicionales anteriores.

```bash
make
./main 1000
./fino 1000
./grueso 1000 8 10 1  # épocas, trabajadores, intervalo ms, espera ms
./cmp 8 1000
./smt 12 "" 1000     # afinidad automática: primero un CPU por núcleo, luego siblings
```

## Benchmark

```bash
make benchmark
```

Por defecto: 1000 épocas, primeras 100 muestras de MNIST, learning rate 0.1,
semilla 1, 200 repeticiones por configuración y un calentamiento excluido.
`THREAD_COUNTS=auto` y `CMP_THREAD_COUNTS=auto` usan 1, 2, 4, 8 y C, omitiendo
repetidos y valores mayores que los C núcleos físicos permitidos.
`SMT_THREAD_COUNTS=auto` usa C y L (o C y 2*C cuando no hay siblings accesibles).
En el equipo de 8 núcleos físicos y 12 CPU lógicas los barridos son coarse/CMP
1,2,4,8 y SMT 8,12. Fine usa un hilo real y cuatro tareas cooperativas con quantum
100 µs; su eficiencia se calcula con un hilo real.

`SMT_STATES=on,off` ejecuta los modelos seleccionados con SMT on, después una
referencia `sequential-off.csv` y el modelo SMT off. Mantiene el mismo número de
trabajadores en ambos estados y restaura el estado global original, también si
ocurre un error, Ctrl+C o SIGTERM. Cambiar SMT requiere sudo y soporte de Linux;
SIGKILL o un corte de energía impiden la restauración automática.

```bash
# Prueba corta sin modificar el estado SMT del equipo:
make benchmark SMT_STATES=keep REPETITIONS=3 EPOCHS=2 SAMPLES=5 RESULTS_DIR=/tmp/dl-prueba

# Solo coarse, con cantidades explícitas:
make benchmark BENCHMARK_MODELS=coarse THREAD_COUNTS=1,2,4,8

# Solo CMP, usando una referencia previa, sin repetir secuencial:
make benchmark BENCHMARK_MODELS=cmp CMP_THREAD_COUNTS=1,2,4,8 BASELINE_CSV=resultdl/sequential.csv

# Solo SMT off, con referencia propia nueva y sin sobrescribir sequential.csv:
make benchmark BENCHMARK_MODELS=smt SMT_STATES=off SMT_THREAD_COUNTS=8,12

# Solo SMT off, reutilizando la referencia previa (el CSV registra su estado):
make benchmark BENCHMARK_MODELS=smt SMT_STATES=off SMT_THREAD_COUNTS=8,12 BASELINE_CSV=resultdl/sequential.csv

make smt SMT=on
make smt SMT=off
make plots
make amdahl
make test
```

`make smt SMT=on/off` deja el estado global solicitado; `SMT=keep` no lo modifica.
El ejecutable SMT se compila como `smt.bin` y `./smt` es un enlace compatible con
los scripts anteriores, para separar el programa de la regla Make de control.

Variables adicionales: `EPOCHS`, `SAMPLES`, `SEED`, `QUANTUM_US`,
`BLOCK_INTERVAL_MS=10`, `BLOCK_DURATION_MS=1`, `REPETITIONS`, `RESULTS_DIR`,
`BENCHMARK_ARGS`. `BLOCK_INTERVAL_MS=0` desactiva los bloqueos artificiales.
Un `--threads N` en `BENCHMARK_ARGS` sustituye los barridos por N trabajadores.

## Coarse y afinidad

Coarse crea N trabajadores persistentes, reparte todos los coeficientes de W1,
b1, W2, b2, W3 y b3 en bloques contiguos y disjuntos y sincroniza con variables
de condición. El coordinador conserva el forward/backprop y el orden SGD por
imagen. Calcula todos los gradientes con los pesos anteriores y espera todas
las actualizaciones antes de iniciar la siguiente imagen.

Cada trabajador revisa el intervalo de bloqueo cada 256 coeficientes. Al vencer,
espera la duración configurada liberando el mutex y continúa desde el mismo
coeficiente. El reloj persiste entre muestras, para que el bloqueo también
ocurra cuando una actualización dura menos que el intervalo. Se informa el
número de bloqueos. CMP y SMT no tienen estas pausas artificiales.

CMP usa una CPU permitida por núcleo físico; SMT añade hermanos lógicos y admite
sobresuscripción. Ambos verifican la afinidad efectiva y fallan si no se puede
fijar; respetan restricciones de `taskset`. Con SMT off, 12 trabajadores sobre
8 CPU lógicas comparten CPU: el nombre del modelo no implica SMT físico activo.

## Resultados y métricas

`resultdl/` sigue la organización de `resultnbody/`:

```text
resultdl/
  benchmark.txt
  sequential.csv, sequential-summary.csv
  sequential-off.csv, sequential-off-summary.csv
  fine.csv, fine-summary.csv
  coarse.csv, coarse-summary.csv
  smt-on.csv, smt-on-summary.csv
  smt-off.csv, smt-off-summary.csv
  cmp.csv, cmp-summary.csv
  references/        # referencias CSV y parámetros inmutables por campaña
  logs/              # comandos, topología, entorno, hashes, estado y parámetros
  backups/           # CSV y resúmenes previos antes de sobrescribirlos
  plots/
    sequential/, sequential-off/, fine/, coarse/, smt-on/, smt-off/, cmp/
    comparison/
```

Solo se crean los archivos correspondientes a los modelos/estados medidos.
Una campaña exclusiva de SMT guarda su referencia en `sequential-on.csv` o
`sequential-off.csv`. Las referencias externas se validan y copian; deben
mantener la carga y los archivos de parámetros indicados en el CSV.

Cada fila corresponde a una repetición real e incluye modelo, trabajadores,
épocas, muestras, learning rate, semilla, hash del dataset, quantum, pausas,
tiempo total, media secuencial, speedup, eficiencia, topología, SMT activo,
afinidad verificada, referencia y checksum SHA256 de parámetros.

Speedup global = media secuencial / media paralela; eficiencia = speedup / N.
Los resúmenes incluyen desviación e IC95 aproximado; el método delta incorpora
la variabilidad de la referencia compartida. Las gráficas incluyen comparación
de tiempo/speedup/eficiencia, histogramas, boxplots, escalabilidad y ajuste de
Amdahl. Se separan los estados SMT/topologías y no se mezclan cargas distintas.
Una ejecución parcial actualiza sus gráficas; `make plots` compara todos los CSV
actuales. Los datos nuevos sustituyen el CSV del modelo completo, con respaldo
previo; no se añaden automáticamente a un barrido anterior.

El cronómetro incluye entrenamiento, creación/cierre de trabajadores y
sincronización. Excluye carga de MNIST, inicialización, impresión por época,
exportación de parámetros, comprobación de equivalencia y gráficas. Eigen usa
un hilo interno. Los algoritmos existentes mantienen sus distintos órdenes de
sumas: se compara cada resultado con secuencial usando `rtol=1e-9`, `atol=1e-11`,
además de exigir parámetros idénticos entre repeticiones de una configuración.
Los artefactos conservan parámetros binarios y el error máximo observado.

## Continuar un benchmark interrumpido

```bash
make benchmark BENCHMARK_MODELS="coarse smt cmp" REPETITIONS=20 BASELINE_CSV=resultdl/sequential.csv BENCHMARK_ARGS="--resume"
```

`--resume` conserva los CSV existentes, omite las configuraciones que ya tienen
al menos 20 repeticiones y completa las demás hasta 20. Si ya hay más de 20,
se conservan todas: no se recortan resultados. Se ejecuta un calentamiento solo
para las configuraciones que necesitan mediciones nuevas. Se requieren la misma
referencia secuencial, carga, política y topología. Los resúmenes y gráficas se
regeneran con todas las mediciones conservadas y nuevas, en la misma carpeta.

## Kernel secuencial comparable con CMP/SMT

`main.cpp` recorre las filas de W1/W2/W3 para el forward, propaga los deltas por
columnas y actualiza los pesos con los mismos bucles de CMP/SMT, en un solo hilo
y sin barreras. Conserva los productos `dot` de Eigen que esos modelos también
usan. El estado se reserva antes del cronómetro. Coarse usa también productos `dot` por fila/columna y bucles explícitos para
los gradientes. Conserva los gradientes completos para que el pool actualice
pesos y sesgos en bloques disjuntos, con su barrera y bloqueos periódicos.
Su estado temporal se reserva antes del cronómetro.

Las referencias secuenciales medidas antes de este cambio corresponden al kernel
anterior. Para comparar con este secuencial hay que volver a medir la referencia
y recalcular los speedups. Los CSV anteriores no se modifican automáticamente.

## Aplicar una nueva referencia a mediciones existentes

```bash
make rebase BASELINE_CSV=resultdl/sequential.csv
```

El comando valida la carga y los pesos finales, conserva los tiempos medidos de
cada modelo y cambia `tiempo_secuencial_ms`, speedup, eficiencia y la referencia
compartida a una copia inmutable de la nueva medición secuencial. Regenera los
resúmenes y gráficas sin entrenar nuevamente. Guarda los CSV y resúmenes previos
en `resultdl/backups/rebase-*/`. El nuevo denominador puede haberse medido en otro
estado SMT; ese estado sigue registrado en `referencia_smt`.

Las mediciones de coarse anteriores a la adaptación por filas/columnas miden
su implementación anterior. Para medir el nuevo coarse con la referencia actual:

```bash
make benchmark BENCHMARK_MODELS=coarse REPETITIONS=20 THREAD_COUNTS=1,2,4,8 BASELINE_CSV=resultdl/sequential.csv
```

Ejecutar sin `--resume` para reemplazar el barrido anterior, con respaldo previo,
y evitar mezclar tiempos de ambas implementaciones. Los CSV existentes no se
modifican al editar o compilar el modelo.
