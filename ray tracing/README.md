# Ray Tracing

Para conocer el diseño del proyecto y ver los diagramas de ejecución por tiles,
consulta [docs/diseno.md](docs/diseno.md).

Proyecto de Arquitectura de Computadores II en C++17. La escena contiene esferas
mates, metálicas y de vidrio sobre un suelo, iluminadas por el cielo mediante
múltiples muestras y rebotes (path tracing). La cámara ofrece una vista baja y
frontal. El cálculo se hace en CPU con modelos sequential, fine, coarse, SMT y CMP; SDL2
muestra el buffer RGB completo al terminar y atiende eventos durante el cálculo.
La imagen permanece visible hasta cerrar la ventana o pulsar Esc.

## Resumen de comandos

Ejecuta los comandos desde la carpeta del proyecto, después de instalar las
[dependencias](#dependencias-y-compilación):

```sh
cd "ray tracing"
```

### Ver la escena con cada modelo

Estos ejemplos abren la ventana y usan la misma calidad: 8 muestras por píxel.
Para una prueba aún más rápida, agrega `--width 320 --height 240 --bounces 6`
dentro de `ARGS` en cualquiera de ellos.

```sh
# Sequential: base sin paralelismo
make run ARGS="--model sequential --samples 8"

# Fine: un hilo real, turnos de K = 64 píxeles por tile
make run ARGS="--model fine --quantum 64 --samples 8"

# Coarse: 4 trabajadores con bloqueos simulados predeterminados
make run ARGS="--model coarse --threads 4 --samples 8"

# CMP: 4 trabajadores en núcleos físicos distintos
make run ARGS="--model cmp --threads 4 --samples 8"

# SMT con el estado actual del equipo
make run ARGS="--model smt --threads 12 --samples 8"

# SMT activado desde Linux
make run SMT=on ARGS="--model smt --threads 12 --samples 8"

# SMT desactivado desde Linux: los 12 trabajadores usan sobresuscripción
make run SMT=off ARGS="--model smt --threads 12 --samples 8"

# Reactivar SMT sin ejecutar el renderizador
make smt SMT=on
```

`SMT=on/off` afecta **a todo el sistema**, puede solicitar `sudo` y permanece
después de terminar. Si omites la variable, se conserva el estado actual;
`--model smt` por sí solo no activa el SMT de hardware. Este control no modifica
BIOS/UEFI; consulta los detalles y el requisito del enunciado en
[SMT y CMP en Linux](#smt-y-cmp-en-linux).

Las cantidades de trabajadores son ejemplos para el equipo de 8 núcleos físicos
y 12 CPUs lógicas. Consulta tu topología con `make run ARGS="--cpu-info"`;
CMP requiere tantos núcleos físicos disponibles como trabajadores.

### Guardar una imagen o ejecutar sin ventana

Agrega `--output nombre.ppm` para guardar y `--headless` para omitir la ventana.
Puedes combinar estas opciones con cualquiera de los modelos anteriores:

```sh
# Ver y guardar la imagen al terminar
make run ARGS="--model cmp --threads 4 --samples 8 --output escena.ppm"

# Calcular y guardar sin ventana
make run ARGS="--headless --model fine --quantum 64 --samples 8 --output fine.ppm"
```

### Comparar contra sequential y generar gráficas

Para comparar los speedups guardados con la **ley de Amdahl**, sin ejecutar
renders nuevos:

```sh
make amdahl
```

Genera `resultsrt/plots/comparison/amdahl.png` y `amdahl.md`. Cada panel mantiene
una política y estado SMT: muestra los puntos medidos con IC95, la referencia
lineal ideal y una curva de Amdahl ajustada. La fracción secuencial **efectiva**
estimada no es una medición del código: también puede reflejar sobrecarga,
esperas y contención. Con un solo P, como fine, no se estima esa fracción.
`make benchmark`, `make plots` y `make run-all` también la generan al graficar.
Puedes seleccionar archivos con `AMDAHL_CSV="resultsrt/cmp.csv"`.

Para ejecutar la campaña completa con **un solo comando**:

```sh
make run-all
```

Primero ejecuta **sequential 200 veces una sola vez** y guarda sus tiempos en
`resultsrt/sequential.csv`. Después ejecuta fine, coarse, CMP y SMT on/off;
todos reutilizan esa misma referencia, sin volver a renderizar sequential por
cada configuración. Usa 200 repeticiones por configuración y 8 muestras
por píxel; detecta los núcleos y CPUs lógicas para elegir los barridos de
trabajadores. Los benchmarks se ejecutan uno después de otro, sin competir
entre ellos, y generan gráficas individuales y una comparación conjunta.
También ejecuta `perf` para fine, coarse, CMP y SMT en cada estado y cantidad
de trabajadores, siempre con referencia sequential. El perfilado usa
`PROFILE_REPETITIONS=5` por defecto y está separado de los tiempos del benchmark.
Perf obtiene una referencia instrumentada propia una sola vez en
`resultsrt/perf/sequential/`; los demás perfiles reutilizan esos contadores.
Antes de empezar comprueba que perf pueda abrir los contadores solicitados.

El speedup global es `media(sequential) / media(modelo)`, y la eficiencia es
`speedup / trabajadores`. El IC95 aproximado usa el método delta y considera
la incertidumbre de ambas medias. Los cocientes de cada fila del CSV son
descriptivos; su promedio no es el speedup global. Los boxplots de sequential
usan sus tiempos originales, sin repetir artificialmente una media constante.
El CSV registra el estado SMT de la referencia: con `SMT_STATES=on,off` se toma
con SMT activado y se reutiliza también para SMT desactivado. Esta referencia
común no aísla por sí sola el efecto de SMT; comparar además los tiempos on/off
con la misma carga y P, y discutir cambios térmicos o de frecuencia entre fases.

El cambio de SMT afecta a todo el sistema y requiere `sudo`. La campaña intenta
restaurar el estado inicial al terminar, incluso ante un error o Ctrl+C; puede
volver a pedir la contraseña. Si la restauración falla, muestra el comando para
recuperarlo. Un cierre forzado del proceso o del equipo puede impedirla.

```sh
# Campaña de referencia: justificar también convergencia y número de iteraciones
make run-all REPETITIONS=200

# Si perf requiere permisos de administrador
make run-all PERF="sudo perf"

# Elegir independientemente las repeticiones de benchmark y de perf
make run-all PERF="sudo perf" REPETITIONS=200 PROFILE_REPETITIONS=5

# Prueba rápida con menor resolución y menos rebotes
make run-all REPETITIONS=5 ARGS="--samples 8 --width 320 --height 240 --bounces 6"

# Mantener 200 tiempos sequential, pero probar cada modelo solo 5 veces
make run-all PERF="sudo perf" BASELINE_REPETITIONS=200 REPETITIONS=5

# Elegir cantidades de trabajadores
make run-all THREAD_COUNTS=1,2,4 SMT_THREAD_COUNTS=8,12

# Ejecutar todos los modelos sin cambiar el estado SMT ni pedir sudo por SMT
make run-all SMT_STATES=keep
```

Los resultados se guardan directamente en **`resultsrt/`**. Cada ejecución
sobrescribe los CSV, resúmenes, reportes y gráficas correspondientes a los modelos
ejecutados, sin crear carpetas `campaign-*`. Las gráficas están en `resultsrt/plots/`
y `benchmark.txt` registra la carga y los barridos. La comparación conjunta solo
usa los CSV generados en esa ejecución; no mezcla archivos de pruebas anteriores.
Las capturas, contadores, reportes y gráficas de perf se sobrescriben en carpetas
fijas dentro de `resultsrt/perf/`, por ejemplo `fine/`, `cmp-p4/` y `smt-off-p12/`.
Puedes elegir otra carpeta con `RESULTS_DIR=otra-carpeta`. Los archivos de otros
modelos no ejecutados y las capturas históricas guardadas aparte se conservan.
No abre ventanas ni guarda imágenes de la escena. Para perfilar una configuración
individual también puedes usar los comandos de la siguiente sección.
El nombre del objetivo es **`make run-all`**: `make run all` ejecutaría dos
objetivos distintos.

Para ejecutar benchmarks individuales:

Cada comando realiza **5 repeticiones por configuración**, incluye su referencia
sequential con la misma carga y genera CSV, reportes y gráficas automáticamente.
Son pruebas cortas; para la campaña de referencia cambia `--repetitions 5` por
`--repetitions 200` y justifica la cantidad final.

```sh
make benchmark ARGS="--model fine --quantum 64 --samples 8 --repetitions 5 --metrics-output results/fine.csv"
make benchmark ARGS="--model coarse --thread-counts 1,2,4,8 --samples 8 --repetitions 5 --metrics-output results/coarse.csv"
make benchmark ARGS="--model cmp --thread-counts 1,2,4,8 --samples 8 --repetitions 5 --metrics-output results/cmp.csv"

# Comparar SMT on/off manteniendo la misma carga y cantidades de trabajadores
make benchmark SMT=on ARGS="--model smt --thread-counts 8,12 --samples 8 --repetitions 5 --metrics-output results/smt-on.csv"
make benchmark SMT=off ARGS="--model smt --thread-counts 8,12 --samples 8 --repetitions 5 --metrics-output results/smt-off.csv"
make smt SMT=on

# Gráficas conjuntas: los CSV deben corresponder a la misma carga
make plots CSV="results/*.csv"
```

### Perf con SMT activado y desactivado

Para actualizar todas las gráficas y reportes de perf existentes sin volver a
renderizar ni ejecutar perf:

```sh
make profile-plots-all
```

Para comparar **todos los modelos perfilados** por categoría (IPC, ciclos,
instrucciones, caché, cambios de contexto, migraciones y tiempo CPU):

```sh
make profile-compare
# Para las capturas individuales guardadas en results/perf:
make profile-compare PROFILE_COMPARE_DIR=results/perf
```

Las gráficas se guardan en `resultsrt/perf/comparison/`, con un `README.md`
que enlaza cada categoría. Cada fila distingue modelo, trabajadores y estado
SMT; P-cores y E-cores aparecen en paneles separados. Se conserva el aviso por
poco tiempo medido. Solo se comparan capturas con igual carga, CPU, eventos,
alcance y modo de conteo. `make run-all` genera esta comparación automáticamente
con las capturas de su ejecución, sin incorporar capturas antiguas ajenas.

Las **nuevas capturas** usan `PROFILE_COUNT_MODE=observed`: `perf --no-scale`
guarda conteos observados, sin extrapolar el poco tiempo de ejecución en E-cores
al tiempo completo del proceso. Ciclos, instrucciones y caché se agrupan por
PMU (`cpu_core` y `cpu_atom` por separado). Un porcentaje bajo puede indicar
poca residencia en ese tipo de núcleo; no se elimina automáticamente un conteo
observado por ese motivo. Se conserva el porcentaje en los reportes. Si hay
multiplexado, los conteos pueden ser parciales: revisa el tiempo de contador y
la captura cruda, y no los presentes como totales completos garantizados.

Por defecto `PROFILE_LOGS=0` evita crear los archivos `.stdout.txt` y
`.stderr.txt` por cada ejecución correcta. Solo guarda `.stderr.txt` si hay
avisos o errores. Se mantienen los CSV crudos de perf, `contadores.csv`,
`metadata.json`, el reporte y las gráficas. Para conservar todos los logs:

```sh
make run-all PERF="sudo perf" PROFILE_LOGS=1
# También funciona con make profile
```

Los logs existentes no se borran al cambiar esta opción; `run-all` reemplaza
los archivos propios de cada captura cuando vuelve a ejecutarla.

Para obtener datos nuevos con esta configuración:

```sh
make run-all PERF="sudo perf" PROFILE_REPETITIONS=20
# Solo una prueba perfilada, sin repetir los benchmarks
make profile PERF="sudo perf" ARGS="--model smt --threads 12 --samples 8" PROFILE_REPETITIONS=20
```

Regenerar imágenes no corrige los conteos antiguos escalados. El modo anterior
se puede seleccionar explícitamente con `PROFILE_COUNT_MODE=scaled`.
Las gráficas conservan **todos los valores numéricos**, también en capturas
antiguas escaladas. Las barras y cajas llevan un rayado naranja si alguna
muestra tiene menos del **50 %** de tiempo medido o no informa ese porcentaje.
Es un aviso, no un filtro: las etiquetas indican el porcentaje y `usadas/capturas`.
Los conteos escalados desde periodos muy cortos pueden estar muy amplificados;
sus medias e IC describen los datos guardados, sin corregir esa limitación.
Solo los casos sin medición (`<not counted>`, por ejemplo) quedan fuera del
cálculo y nunca se muestran como cero. El IPC usa ciclos e instrucciones de la
misma repetición, CPU y PMU. Los CSV originales no se modifican.

Estas mediciones son independientes del benchmark. Si `perf` necesita permisos,
usa `PERF="sudo perf"`. Cada ejecución conserva una captura separada.

```sh
make profile SMT=on ARGS="--model smt --threads 12 --samples 8" PROFILE_REPETITIONS=5
make profile SMT=off ARGS="--model smt --threads 12 --samples 8" PROFILE_REPETITIONS=5
make smt SMT=on
```

Para interpretar las mediciones y consultar otras opciones, revisa
[docs/perfilado.md](docs/perfilado.md).

## Dependencias y compilación

Desde la raíz del repositorio, entra primero a la carpeta del proyecto. Todos
los comandos siguientes se ejecutan dentro de ella:

```sh
cd "ray tracing"
```

En Debian/Ubuntu:

```sh
sudo apt install build-essential pkg-config libsdl2-dev
make build -j2
```

Se requieren C++17, Make y SDL2 con sus encabezados de desarrollo. Sin SDL2:

```sh
make WITH_SDL=0 -j2
./build/headless/raytracer --headless
```

Las dos variantes usan directorios separados. La compilación sin SDL rechaza
explícitamente el modo ventana. Para limpiar los binarios: `make clean`.

## Ejecución

Para compilar lo necesario y abrir la ventana, desde `ray tracing`, basta con:

```sh
make run
```

`make build` solo compila; `make run` compila lo necesario y ejecuta.

Puedes pasar opciones con `ARGS`, por ejemplo:

```sh
make run ARGS="--headless --output escena.ppm"
```

Los archivos de salida relativos se guardan dentro de `ray tracing`.

```sh
# Ventana: 640 × 480, tiles de 32 × 32
./build/sdl/raytracer

# Medición sin inicializar SDL ni escribir archivos
./build/sdl/raytracer --headless

# Guardar PPM binario (P6)
./build/sdl/raytracer --headless --output escena.ppm

# Visualizar y guardar; escribe al terminar antes de esperar el cierre
./build/sdl/raytracer --output escena.ppm

# Bordes parciales y tiles rectangulares
./build/sdl/raytracer --width 641 --height 479 --tile-width 32 --tile-height 23

# Cambiar ambos lados del tile
./build/sdl/raytracer --width 1280 --height 960 --tile-size 64
./build/sdl/raytracer --help
```

Las dimensiones deben ser enteros positivos. Prevalece la última opción si se
repite una dimensión. `--output` sobrescribe el archivo indicado. Al cancelar
antes de terminar, se omite la escritura para no guardar una imagen incompleta.
Códigos de salida: 0 completo, 1 error, 2 cancelado.

No se solicita vsync. Sequential y fine no añaden pausas artificiales; coarse
permite simular esperas configurables. La ventana permanece negra
durante el cálculo y muestra la escena completa al terminar, sin barrido visible.
Todos los modelos usan la misma partición en tiles y permiten cerrar la ventana
durante el cálculo. Los eventos SDL se atienden en el hilo principal.

## Modelos de ejecución por tiles

`make run` conserva `sequential` como modelo predeterminado. Los cinco modelos
usan la misma escena y el mismo cálculo de píxel; cambian cómo avanzan los tiles.

| Modelo | Ejecución |
| --- | --- |
| `sequential` | Un hilo termina un tile antes de comenzar el siguiente |
| `fine` | Un hilo simula tareas cooperativas: round-robin con hasta K píxeles completos por turno |
| `coarse` | Un hilo por tile por defecto; `--threads P` permite repartirlos entre P trabajadores, con bloqueos opcionales |
| `smt` | P trabajadores sobre CPUs lógicas, incluidos hermanos SMT; permite sobresuscripción de CPUs |
| `cmp` | P trabajadores fijados a núcleos físicos distintos, con P limitado a los núcleos disponibles |

```sh
# Fine-grained: K es configurable, por defecto 64 píxeles por turno
make run ARGS="--model fine --quantum 64"

# Coarse-grained: por defecto intervalo de 10 ms y espera de 1 ms
make run ARGS="--model coarse --block-interval-ms 10 --block-duration-ms 1"

# Medir sin ventana; un intervalo 0 desactiva los bloqueos simulados
make run ARGS="--headless --model coarse --block-interval-ms 0"
```

Fine-grained implementa la simulación cooperativa diseñada: un solo hilo real
intercala tareas y no utiliza varios núcleos a la vez. Coarse sigue la asignación
de un hilo por tile: 640 × 480 con tiles de 32 × 32 crea 300 hilos trabajadores,
además del principal. `--threads P` activa una variante de reparto con P
trabajadores y un índice atómico del siguiente tile, manteniendo fija la partición.
Esta variante permite medir escalabilidad y está documentada como una extensión
del diseño original de un hilo por tile. La planificación del sistema operativo puede interrumpirlos
incluso fuera de los bloqueos simulados. Crear muchos hilos tiene un costo y no
garantiza mayor rendimiento. Los bloqueos se comprueban entre píxeles completos;
el intervalo es tiempo de pared desde la última reanudación.

Los valores predeterminados de K y de los bloqueos son puntos de partida, no
parámetros optimizados mediante mediciones.

### SMT y CMP en Linux

Ambos modelos usan `parallel.cpp`: un contador atómico entrega tiles completos
a un grupo de trabajadores; cada tile tiene un único dueño. No aplican K ni
esperas artificiales. `cpu.cpp` detecta la topología en sysfs y las CPUs permitidas
por la afinidad del principal. Identifica núcleos por `(socket, core_id)`, sin
suponer que cada núcleo tenga dos hilos lógicos. Se requiere Linux con sysfs.

CMP elige una CPU lógica por núcleo físico y fija allí un trabajador mediante
[afinidad de Linux](https://man7.org/linux/man-pages/man3/pthread_setaffinity_np.3.html).
Con `--threads P` exige `P <= C` y `P <= M`; sin el parámetro usa `min(C, M)`.
C son los núcleos físicos disponibles y M la cantidad de tiles.

SMT primero ocupa una CPU por núcleo y luego sus hermanos lógicos. Por defecto
usa todas las CPUs lógicas disponibles, limitado por M. Si solo hay una CPU
disponible por núcleo, usa hasta dos trabajadores por núcleo para aproximar SMT
mediante sobresuscripción. `--threads P` permite fijar la cantidad: si P supera
las CPUs lógicas, varios trabajadores quedan en la misma CPU y se alternan por
el SO; eso no crea más unidades SMT. P no puede superar M. Con P menor o igual
a C se obtiene una configuración de referencia sin sobresuscribir núcleos.

```sh
# Consultar núcleos disponibles, CPUs lógicas y estado SMT informado por Linux
make run ARGS="--cpu-info"

# Ver la escena; cantidad de trabajadores automática según el modelo
make run ARGS="--model cmp --samples 8"
make run ARGS="--model smt --samples 8"

# Cinco comparaciones contra sequential, con gráficas automáticas
make benchmark ARGS="--model cmp --samples 8 --repetitions 5 --metrics-output results/cmp.csv"
make benchmark ARGS="--model smt --samples 8 --repetitions 5 --metrics-output results/smt.csv"

# Barrido CMP: ajustar la lista para que ningún P supere C
make benchmark ARGS="--model cmp --thread-counts 1,2,4 --samples 8 --repetitions 5 --metrics-output results/cmp.csv"
```

Puedes controlar SMT desde Linux con la variable `SMT=off` o `SMT=on` en
`make run`, `make benchmark` y `make profile`. El valor predeterminado
`SMT=keep` conserva el estado actual. Esto afecta **a todo el sistema**, requiere
`sudo` cuando cambia el estado y permanece después de terminar, incluso si el
renderizado falla. Solo la escritura en sysfs usa privilegios; el renderizador
se ejecuta con tu usuario. Si no se puede aplicar el estado solicitado, la
ejecución se detiene.

```sh
# Renderizar sin SMT de hardware, conservando 12 trabajadores explícitos
make run SMT=off ARGS="--model smt --threads 12 --samples 8"

# Comparar estados con la misma carga y P; guardar los datos por separado
make benchmark SMT=off ARGS="--model smt --threads 12 --samples 8 --repetitions 5 --metrics-output results/smt-off.csv"
make benchmark SMT=on ARGS="--model smt --threads 12 --samples 8 --repetitions 5 --metrics-output results/smt-on.csv"

# Reactivar SMT sin renderizar
make smt SMT=on
```

Con SMT desactivado, `--model smt` sigue funcionando mediante sobresuscripción;
no reactiva el SMT de hardware. Las cinco repeticiones del ejemplo son una
prueba corta. Este control no modifica BIOS/UEFI y su uso como sustituto del
procedimiento exigido por el enunciado debe acordarse con el profesor.

La aproximación de SMT desde software no controla la emisión de instrucciones
dentro de un núcleo. Para aislar el SMT de hardware, el enunciado exige repetir
las mediciones en el mismo equipo físico con SMT habilitado y deshabilitado en
BIOS/UEFI, conservando la carga y las mismas cantidades P explícitas (incluida
alguna P > C). El programa no cambia BIOS. El CSV registra C, CPUs lógicas,
estado `/sys/devices/system/cpu/smt/active` y la CPU asignada a cada trabajador.
Ese estado describe lo informado por Linux; no prueba por sí solo cómo se
configuró BIOS. Si la afinidad falla, se cancela y se informa el error.
En equipos híbridos, núcleos de distinto tipo pueden tener rendimientos diferentes.

## Calidad y tiempo de espera

`make run` usa 128 muestras por píxel y hasta 12 rebotes. La ventana permanece
negra mientras calcula; ahora puede tardar varias decenas de segundos según el
CPU. En sequential no se añaden pausas artificiales. Para probar la composición rápido:

```sh
make run ARGS="--samples 8 --bounces 6"
```

Para reducir el grano y guardar una imagen de mayor calidad:

```sh
make run ARGS="--samples 256 --bounces 16 --output escena.ppm"
```

Aumentar muestras reduce el ruido y suaviza bordes, pero aumenta aproximadamente
el tiempo en la misma proporción. Un número bajo de rebotes puede oscurecer el
vidrio y las reflexiones entre objetos. Límites de consola: 1–4096 muestras y
1–64 rebotes. `--samples 1` usa el centro del píxel, aunque sus rebotes siguen
siendo muestreados. No se implementa desenfoque de cámara ni eliminación de ruido.

## Estructura y cálculo

`src` está organizado por responsabilidad. `main.cpp` conecta los módulos;
cada subcarpeta conserva sus interfaces `.hpp` y sus implementaciones `.cpp`:

```text
src/
├── main.cpp
├── app/        # Argumentos de consola y ventana SDL
├── scene/      # Cámara, geometría, materiales, escena y muestreo
├── render/     # Buffer de imagen, tiles y cálculo del ray tracing
├── execution/  # Sequential, fine, coarse, SMT/CMP y afinidad CPU
└── metrics/    # Benchmark, estadísticas y exportación CSV
```

Los modelos que recorren y reparten tiles están en `src/execution/`.
Todos reutilizan el cálculo de `src/render/`; la organización no cambia
los algoritmos ni los comandos de ejecución.

| Archivo | Responsabilidad |
| --- | --- |
| `src/scene/geometry.hpp`, `src/scene/geometry.cpp` | Vectores, rayos, esferas e intersección geométrica |
| `src/scene/scene.hpp`, `src/scene/scene.cpp` | Objetos, materiales, cielo y configuración de calidad |
| `src/scene/camera.hpp`, `src/scene/camera.cpp` | Cámara y rayos hacia muestras dentro del píxel |
| `src/render/image.hpp`, `src/render/image.cpp` | Buffer RGB y escritura PPM |
| `src/render/tiles.hpp`, `src/render/tiles.cpp` | Partición rectangular y recorte de bordes |
| `src/scene/material.hpp` | Tipo de material, rugosidad e índice de refracción |
| `src/scene/sampling.hpp` | Muestras reproducibles por píxel sin estado global |
| `src/render/transport.hpp`, `src/render/transport.cpp` | Rebotes, reflexión, refracción e iluminación del cielo |
| `src/render/raytracer.hpp`, `src/render/raytracer.cpp` | Promedio de muestras, gamma y cálculo de un tile |
| `src/execution/sequential.hpp`, `src/execution/sequential.cpp` | Base secuencial, tiempos y callbacks comunes |
| `src/execution/execution.hpp`, `src/execution/fine.cpp`, `src/execution/coarse.cpp` | Modelos, round-robin y reparto de tiles |
| `src/execution/parallel.cpp`, `src/execution/cpu.hpp`, `src/execution/cpu.cpp` | SMT/CMP, topología Linux y afinidad de trabajadores |
| `scripts/profile_perf.py` | Capturas perf por modelo/CPU, referencia secuencial, reportes y gráficas |
| `src/metrics/metrics.hpp`, `src/metrics/metrics.cpp`, `src/metrics/benchmark.hpp`, `src/metrics/benchmark.cpp` | Métricas, repeticiones y exportación CSV |
| `tests/test_execution.cpp` | Igualdad de modelos, progreso, bloqueos y cancelación |
| `tests/test_display.cpp` | Presentación y cierre SDL con controlador de prueba |
| `src/app/options.hpp`, `src/app/options.cpp` | Configuración y validación de argumentos |
| `src/app/display.hpp`, `src/app/display.cpp` | Recursos SDL2, presentación y eventos |
| `src/main.cpp` | Conectar módulos, mostrar resultados y controlar la salida |
| `docs/diseno.md` | Diagramas y variables para próximas etapas |

La escena se modifica en `default_scene()` de `src/scene/scene.cpp`. Las dimensiones
predeterminadas están en `Options`, en `src/app/options.hpp`. La cámara está en
`(0, 0.45, 2.0)` y apunta a `(0, 0.25, -7.5)`; ambos vectores se ajustan en
`src/main.cpp`. El campo de visión vertical es de 36° y está en `src/scene/camera.cpp`.
Cada muestra genera un rayo a una posición dentro del píxel, corrigiendo la
relación ancho/alto y normalizando su dirección.

Sustituir `R(t)=O+tD` en `|R(t)-C|²=r²` produce
`a*t² + 2*half_b*t + c = 0`. Se toma la raíz positiva más cercana entre todas
las esferas. En cada impacto, `transport.cpp` calcula el siguiente segmento:

- Mate: dispersión en el hemisferio orientado por la normal, ponderada por coseno.
  El color multiplica la luz que sobrevive a ese rebote.
- Metal: reflexión especular, perturbada según la rugosidad.
- Vidrio: refracción por la ley de Snell y reflexión por la aproximación de Schlick,
  incluyendo reflexión interna total. Se modela vidrio sólido en aire, sin absorción.

Cuando el rayo sale de la escena, recoge luz del cielo. Los objetos bloquean
parte de esa luz y generan sombras suaves de contacto. Ya no se usa el término
ambiental constante ni la luz puntual de la primera versión. Las trayectorias
que alcanzan el límite de rebotes sin escapar aportan negro. Se desplaza el origen
de cada rebote ligeramente al lado de salida para evitar autoimpactos numéricos.

Se promedia la luz en espacio lineal y se aplica `sqrt(color)` (gamma aproximada 2)
antes de convertir a RGB de 8 bits. La semilla combina coordenadas del píxel y
una semilla fija: modificar el orden o tamaño de tiles no modifica las muestras.

`TileGrid` calcula límites superiores exclusivos, recortados en los bordes, sin
almacenar una lista de tiles. `render_tile` recorre filas y columnas y llama a
`render_pixel`. `render_tile_step` conserva el siguiente píxel en `TileProgress`
y acepta un límite K; fine lo utiliza para reanudar tareas. Sequential, coarse, SMT y CMP
usan `render_tile` para avanzar hasta el final. En coarse, las esperas interrumpibles
conservan ese estado y los callbacks de la aplicación se ejecutan en el principal.
SDL recibe la imagen completa después de terminar todos los tiles y esperar los hilos.

## Cambiar o agregar esferas

Edita `src/scene/scene.cpp`, dentro de `default_scene()`. Las primeras llamadas a
`add_sphere(x, z, radio, color, material)` definen las tres esferas principales.
Sus radios actuales son 1.0 (izquierda), 1.15 (cristal central) y 1.5 (metal
derecha). Cambia el tercer argumento para modificar su tamaño; la altura se
calcula automáticamente para que descansen sobre el suelo.

Las demás esferas se generan en posiciones pseudoaleatorias, sin filas ni
cuadrícula: hasta 75 decorativas, en su mayoría pequeñas, con algunas medianas
y grandes. Se prueban como máximo 100 posiciones por esfera y se descartan las
que no respetan la separación mínima. Dos esferas de cristal requieren un
margen mayor entre sus superficies. Los colores decorativos se generan en HSV
para repartir tonos vivos por la paleta; hay materiales mates, metálicos y de
cristal. La semilla fija `2026` permite repetir la misma escena para comparar
mediciones. Cambiarla produce otra distribución.

La composición está inspirada en **A Final Render** de
[Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html),
pero usa nuestra propia distribución y una cámara baja y frontal. Los rebotes
producen reflejos y sombras suaves por oclusión de la luz del cielo. El vidrio
no absorbe luz ni usa su color RGB como tinte; no se simulan cáusticas ni
desenfoque de cámara.

El suelo es una esfera de radio 1000, casi plana en la región de los objetos.
`add_sphere` calcula la altura para que cada esfera toque ese suelo. Para añadir
una esfera, antes de los bucles de pequeñas puedes escribir:

```cpp
//         x     z    radio     color RGB
add_sphere(-4.0, -6.0, 0.6, {0.9, 0.4, 0.1});
```

Ejecuta `make run` para recompilar y ver los cambios. Si amplías la distribución,
puede ser necesario ajustar posición y punto de mira de la cámara en `main.cpp`.

`add_sphere` acepta un material opcional después del color:

```cpp
add_sphere(-4.0, -6.0, 0.6, {0.9, 0.7, 0.4}, {MaterialType::metal, 0.05});
add_sphere(4.0, -6.0, 0.6, {1, 1, 1}, {MaterialType::glass, 0.0, 1.5});
```

En el vidrio transparente el color RGB no se utiliza como tinte. La rugosidad
está pensada para valores entre 0 y 1, y el índice de refracción debe ser positivo.

## Métricas del proyecto

Según [docs/proyecto.md](docs/proyecto.md), se reportan estas métricas:

| Métrica | Cálculo |
| --- | --- |
| Tiempo total de ejecución del modelo | Tiempo de pared con `steady_clock`, desde el inicio del modelo hasta terminar todos los tiles y sus hilos |
| Speedup respecto a sequential | `S = T_secuencial / T_modelo` para la misma carga |
| Eficiencia paralela | `E = S / P`; también se muestra `100 × E` en porcentaje |
| Escalabilidad | Comparación de tiempo, speedup y eficiencia al variar P con la misma escena y tiles |

La ejecución normal muestra `tiempo_total_ms`. Para obtener también speedup y
eficiencia, el modo benchmark ejecuta la referencia secuencial y el modelo elegido
en cada repetición. No abre ventana ni guarda imágenes; construye la escena,
cámara y buffers antes de medir. El tiempo incluye planificación, creación de
hilos, esperas simuladas y finalización dentro del modelo. Las comparaciones de
píxeles, escritura CSV y presentación quedan fuera del intervalo.

### Ejecución rápida de los cuatro modelos

Estos comandos hacen **cinco repeticiones por configuración**, sin ventana,
y generan CSV, reportes y gráficas automáticamente. Todos comparan el modelo
elegido contra **sequential con la misma carga**; no necesitas ejecutar la
referencia aparte. Ejecutarlos desde la carpeta `ray tracing`.

```sh
# Fine: un hilo real, round-robin cada K = 1 píxel completo
make benchmark ARGS="--model fine --quantum 1 --samples 8 --repetitions 5 --metrics-output results/fine.csv"

# Coarse: barrido de 1, 2, 4 y 8 trabajadores
make benchmark ARGS="--model coarse --thread-counts 1,2,4,8 --samples 8 --repetitions 5 --metrics-output results/coarse.csv"

# CMP: trabajadores en núcleos físicos distintos
make benchmark ARGS="--model cmp --thread-counts 1,2,4,8 --samples 8 --repetitions 5 --metrics-output results/cmp.csv"

# SMT: comparar 8 y 12 trabajadores
make benchmark ARGS="--model smt --thread-counts 8,12 --samples 8 --repetitions 5 --metrics-output results/smt.csv"

# Comparar todos los CSV anteriores en las mismas gráficas
make plots CSV="results/*.csv"
```

Las listas anteriores son un ejemplo para un equipo de **8 núcleos físicos y
12 CPUs lógicas**. Consulta `make run ARGS="--cpu-info"` y ajústalas si tu equipo
es distinto; CMP no permite P mayor que los núcleos físicos disponibles.
Para comparar políticas con igual cantidad de trabajadores, coarse, CMP y SMT
incluyen P = 8. Cambiar P dentro de un modelo estudia escalabilidad, conservando
la misma imagen y los mismos tiles. Fine usa un hilo real y permite variar K.
`--samples 8` son ocho muestras por píxel, no ocho repeticiones del experimento.

Para la campaña final, sustituye `--repetitions 5` por `--repetitions 200` en
cada comando. Mantén iguales escena, resolución, tiles, muestras, rebotes y
semilla entre modelos. Las gráficas individuales quedan en
`results/plots/fine/`, `coarse/`, `cmp/` y `smt/`; la comparación conjunta se
guarda en `results/plots/`.

`--benchmark` activa `--headless` automáticamente. También puede usarse dentro de
`make run ARGS="--benchmark ..."` para obtener solo CSV. `make benchmark` genera
también las gráficas al terminar y requiere Matplotlib. Por defecto hace 200 repeticiones; se admiten
al menos dos para pruebas cortas. `--thread-counts` es un barrido exclusivo de
coarse, SMT o CMP y no se combina con `--threads`. P no puede superar la cantidad de tiles;
en CMP tampoco puede superar los núcleos físicos disponibles.
Cambiar P conserva el tamaño y la cantidad de tiles, así se estudia escalabilidad
con carga fija. Fine usa P = 1: sus tareas lógicas no se cuentan como hilos
reales para inflar el denominador de la eficiencia.

Se exportan dos archivos:

- `results/coarse.csv`: cada ejecución, configuración, tiempo total, referencia
  secuencial, speedup y eficiencia. Se valida que las imágenes sean idénticas.
- `results/coarse-summary.csv`: una fila por configuración con medias,
  desviaciones estándar muestrales e intervalos aproximados de confianza del 95 %.

Sin `--baseline-csv`, el benchmark individual conserva la comparación pareada:
el resumen promedia los speedups calculados en cada par de ejecuciones; no es
el cociente de las medias de tiempos. Cada configuración tiene un calentamiento
no exportado y se alterna el orden secuencial/modelo para reducir sesgos de orden.
El IC de la media usa `media ± 1.96 × desviación / sqrt(n)`; con pocas repeticiones
es un dato diagnóstico. No describe el rango de todas las ejecuciones. Una
sola configuración permite calcular speedup y eficiencia, pero no una curva de
escalabilidad. La eficiencia puede superar 1 por ruido o efectos de caché y no
se recorta artificialmente; no es el porcentaje de uso de CPU.

`make run-all` usa una **referencia compartida**. También puedes reutilizarla en
un benchmark individual con `--baseline-csv resultsrt/sequential.csv`, siempre
que coincidan resolución, tiles, muestras, rebotes, semilla e imagen. En ese modo
no se ejecuta sequential; se utiliza el cociente de medias y un IC95 por método
delta que incluye la variación de la referencia y del modelo. Las columnas
de desviación de speedup usan la aproximación delta, no la desviación muestral
de los cocientes por fila. Las columnas
`referencia_tipo`, `referencia_csv`, `imagen_hash` y `referencia_smt` identifican
el método y la procedencia. Conserva `sequential.csv` junto a los CSV de los
modelos para poder regenerar las gráficas.

Los intervalos y las esperas de coarse forman parte de su configuración y deben
mantenerse fijos al comparar P. Las mediciones finales deben hacerse en hardware
físico, con la justificación de repeticiones y análisis que pide el enunciado.
`make profile` incorpora perfilado con perf. La campaña física con SMT
activado/desactivado y su análisis siguen pendientes.

### Perfilado con perf

Para observar ciclos, instrucciones, IPC, fallos de caché y cambios de contexto
conservando una referencia secuencial y gráficas por evento:

```sh
# Cada comando incluye cinco perfiles del modelo y cinco de sequential
make profile ARGS="--model fine --quantum 1 --samples 8" PROFILE_REPETITIONS=5
make profile ARGS="--model coarse --threads 4 --samples 8" PROFILE_REPETITIONS=5
make profile ARGS="--model smt --threads 12 --samples 8" PROFILE_REPETITIONS=5
make profile ARGS="--model cmp --threads 4 --samples 8" PROFILE_REPETITIONS=5
```

Opciones adicionales de perfilado:

```sh
# Contadores separados por CPU lógica; incluye otras tareas del sistema
make profile PROFILE_SCOPE=cpu ARGS="--model smt --threads 12 --samples 8" PROFILE_REPETITIONS=5

# Si Linux deniega acceso a los contadores y tienes permiso de administrador
make profile PERF="sudo perf" ARGS="--model smt --threads 12 --samples 8" PROFILE_REPETITIONS=5

# Regenerar gráficas de una captura existente, sin volver a perfilar
# Sustituye nombre-de-campaña por la carpeta creada en results/perf/
make profile-plots PROFILE_CAPTURE=results/perf/nombre-de-campaña
```

Ajusta P a tu topología (`make run ARGS="--cpu-info"`). Cada comando perfila el
modelo y sequential por separado; guarda capturas, CSV, metadata, reporte y
boxplots en una nueva carpeta de `results/perf/`. Para ver contadores por CPU
lógica añade `PROFILE_SCOPE=cpu`; ese alcance incluye actividad ajena al programa.
No sustituye las 200 repeticiones de `make benchmark` sin perf.

El significado de los eventos, permisos, alcance, multiplexación y el procedimiento
SMT activado/desactivado están en [docs/perfilado.md](docs/perfilado.md).

### Gráficas y repeticiones

El renderizador sigue siendo C++; un script Python con Matplotlib analiza los
CSV después de medir, sin añadir trabajo al intervalo del benchmark. En Ubuntu:

```sh
sudo apt install python3-matplotlib

# Graficar los resultados que ya tienes, sin volver a renderizar
make plots

# Medir 200 veces por configuración y generar las gráficas automáticamente
make benchmark ARGS="--model coarse --thread-counts 1,2,4,8 --samples 8 --repetitions 200"
```

`make benchmark` guarda las gráficas en `results/plots/<nombre-del-csv>/`:
por ejemplo, `results/plots/fine/`, `results/plots/coarse/`, `results/plots/smt/`, `results/plots/cmp/` o
`results/plots/metrics/`. Así se conservan las gráficas de ambos modelos.
`make plots` permite volver a graficar CSV existentes o compararlos juntos y
guarda esa comparación en `results/plots/`. `make benchmark-plots` sigue
disponible como alias de `make benchmark`. Se generan:

- `comparacion-tiempo.png`: tiempo medio en segundos, junto con la referencia
  secuencial de cada comparación. Menos tiempo es mejor.
- `comparacion-speedup.png`: aceleración media con referencia en 1×;
  por ejemplo, 2× significa el doble de rapidez respecto a sequential.
- `comparacion-eficiencia.png`: eficiencia en porcentaje, con referencia ideal 100 %.
- Boxplots e histogramas de tiempo, speedup y eficiencia: usan todas las
  iteraciones de cada configuración, incluidos los valores atípicos.
- Boxplot de los tiempos de la referencia secuencial de cada comparación.
- `escalabilidad.png`: speedup al variar trabajadores, con línea ideal S = P.
  Tiempo y eficiencia se muestran en `escalabilidad-tiempo.png` y
  `escalabilidad-eficiencia.png`; cada imagen responde una pregunta.
- `reporte.md`: cantidad de repeticiones, medias, desviaciones, intervalos y
  justificación inicial del número de ejecuciones. Indica si n es menor que 200.

Todas usan colores consistentes: gris para sequential, violeta para fine,
naranja para coarse, turquesa para CMP y rosa para SMT. Las barras principales
muestran valores y el IC95 aproximado. Al pie se indica la carga (resolución,
tiles, muestras y rebotes); las etiquetas incluyen cantidad de repeticiones.
Los boxplots son apoyo para estudiar variabilidad: caja = mitad central,
línea = mediana y puntos = valores atípicos. Las unidades de los tiempos
en las gráficas son segundos; los CSV y el reporte conservan milisegundos.

Para comparar los cuatro modelos, usa los comandos de
[ejecución rápida](#ejecución-rápida-de-los-cuatro-modelos), conserva un archivo
por modelo y genera la comparación conjunta con `make plots CSV="results/*.csv"`.

El script omite los CSV de resumen y rechaza cargas diferentes (resolución,
tiles, muestras, rebotes o semilla) para evitar comparaciones engañosas. Si se
incluyen varios archivos de la misma configuración, sus iteraciones se agrupan;
no incluyas copias de las mismas mediciones. K y los bloqueos distinguen políticas.
Una única configuración produce un punto, no demuestra escalabilidad por sí sola.
Puedes cambiar `CSV`, `PLOT_DIR` y `PYTHON` en los comandos Make. Los archivos CSV
y las gráficas se sobrescriben al volver a usar la misma ruta: conserva campañas
distintas en rutas diferentes.

Se toman **200 repeticiones por configuración** como punto de partida para
cumplir la referencia del enunciado y estimar variabilidad y precisión. El error
estándar disminuye como `desviación / sqrt(n)` bajo independencia. Para justificar
la cantidad final, evalúa el margen relativo del IC95 (`margen / media`) de tiempo
y speedup; si resulta demasiado grande para tu comparación, aumenta n. No basta
con alcanzar 200 para garantizar una conclusión: documenta hardware, carga del
equipo y posibles cambios térmicos o correlación entre ejecuciones.
Las pruebas cortas con `--repetitions 5` generan las mismas gráficas, pero el
reporte las marca como pendientes de la campaña final.

## Verificación

```sh
make test
make test-plots
make test-profile
make test-display

# Comparar exactamente las imágenes, con bordes parciales
./build/sdl/raytracer --headless --width 641 --height 479 --samples 2 --model sequential --output build/seq.ppm
./build/sdl/raytracer --headless --width 641 --height 479 --samples 2 --model fine --quantum 7 --output build/fine.ppm
./build/sdl/raytracer --headless --width 641 --height 479 --samples 2 --model coarse --output build/coarse.ppm
cmp build/seq.ppm build/fine.ppm
cmp build/seq.ppm build/coarse.ppm
```

Las pruebas verifican imagen idéntica, tiles de borde, turnos round-robin,
reanudación después de bloqueos, notificación de tiles desde el principal y
cancelación durante esperas largas. El controlador SDL `dummy` permite probar
eventos y presentación sin un escritorio real.
Los resultados y las limitaciones del entorno están en [docs/validacion.md](docs/validacion.md).

## Referencias y asistencia

Se consultaron las secciones de geometría, antialiasing, materiales difusos,
metales, dieléctricos, gamma y escena final de
[Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
y los parámetros de presentación con
[SDL_UpdateTexture](https://wiki.libsdl.org/SDL2/SDL_UpdateTexture).


## Dependencias entre módulos

Cada `.hpp` declara la interfaz y su `.cpp` implementa el comportamiento.
`main` conecta las opciones, la presentación y el modelo de ejecución elegido.
`sequential` utiliza `raytracer`, que calcula píxeles a partir de la escena,
la cámara y la geometría y los escribe en `image` según los límites de `tiles`.
`display` solo necesita imagen y tiles; el núcleo no incluye SDL.

`RenderHooks` permite notificar tiles terminados y consultar cancelación sin
acoplar el recorrido a una ventana. Sin callbacks, funciona sin interfaz gráfica.
`fine.cpp` planifica tareas de tiles y `coarse.cpp` asigna los tiles a hilos.
`parallel.cpp` distribuye los tiles de SMT/CMP y `cpu.cpp` define su asignación a CPUs.
Los comentarios de estos módulos señalan el reparto, el progreso y la sincronización.
`sequential.cpp` conserva el recorrido de referencia para comparar.

La ampliación visual implementa esos fundamentos con nuestros módulos, un bucle
iterativo de rebotes y muestras locales por píxel. No se importó el código fuente
del libro. Es una ampliación del alcance inicial solicitada para mejorar la escena;
el proyecto conserva su referencia secuencial y la partición común entre modelos.

## Tiles y avance respecto a la propuesta

La unidad de trabajo sigue siendo el tile: con 640×480 y tiles 32×32 hay 300
regiones. Los rayos secundarios consultan la geometría, sin depender del buffer
de otros píxeles. Por ello la ampliación visual conserva la independencia de tiles.

Los diagramas y las decisiones para los modelos de ejecución están en
[el documento de diseño](docs/diseno.md). Allí se explica cómo se reparten los
tiles, qué significa K píxeles completos y qué falta implementar.
