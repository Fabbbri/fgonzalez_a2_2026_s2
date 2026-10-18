# Ray Tracing

Implementación del problema de ray tracing para el [proyecto de Arquitectura de
Computadores II](../proyecto.md). Los diagramas y las decisiones de ejecución
están en [docs/diseno.md](docs/diseno.md).

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
BIOS/UEFI; los detalles están en
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

### Comparar resultados con la ley de Amdahl

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

### Ejecutar la campaña de mediciones

Para ejecutar los benchmarks y el perfilado:

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
El script comprueba el acceso a los contadores antes de iniciar las mediciones.

El speedup global es `media(sequential) / media(modelo)`, y la eficiencia es
`speedup / trabajadores`. El IC95 aproximado usa el método delta y considera
la incertidumbre de ambas medias. Los cocientes de cada fila del CSV son
descriptivos; su promedio no es el speedup global. Los boxplots de sequential
usan sus tiempos originales, sin repetir artificialmente una media constante.
El CSV registra el estado SMT de la referencia: con `SMT_STATES=on,off` se toma
con SMT activado y se reutiliza también para SMT desactivado. La comparación on/off usa la misma carga y P. La referencia común no aísla
los cambios térmicos o de frecuencia entre fases.

El cambio de SMT afecta a todo el sistema y requiere `sudo`. La campaña intenta
restaurar el estado inicial al terminar, incluso ante un error o Ctrl+C; puede
volver a pedir la contraseña. Si la restauración falla, muestra el comando para
recuperarlo. Un cierre forzado del proceso o del equipo puede impedirla.

```sh
# Campaña de 200 repeticiones por configuración
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
### Ejecutar benchmarks individuales

Cada comando realiza **5 repeticiones por configuración**, incluye su referencia
sequential con la misma carga y genera CSV, reportes y gráficas automáticamente.
Son pruebas cortas; para la campaña de referencia cambia `--repetitions 5` por
`--repetitions 200`.

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
multiplexado, los conteos pueden ser parciales. El tiempo de contador y la
captura cruda permiten identificar esa cobertura.

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
prueba corta. Este control modifica sysfs, no la configuración de BIOS/UEFI.

La aproximación de SMT desde software no controla la emisión de instrucciones
dentro de un núcleo. La comparación de SMT de hardware utiliza el mismo equipo,
carga y cantidades P en ambos estados, incluida alguna P > C. El programa no
cambia BIOS/UEFI. El CSV registra C, CPUs lógicas,
estado `/sys/devices/system/cpu/smt/active` y la CPU asignada a cada trabajador.
Ese estado describe lo informado por Linux; no prueba por sí solo cómo se
configuró BIOS. Si la afinidad falla, se cancela y se informa el error.
En equipos híbridos, núcleos de distinto tipo pueden tener rendimientos diferentes.

## Calidad y tiempo de espera

`make run` usa 128 muestras por píxel y hasta 12 rebotes. La ventana permanece
negra mientras calcula; la duración depende del procesador y de la calidad elegida. En sequential no se añaden pausas artificiales. Para probar la composición rápido:

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

`main.cpp` conecta los módulos. Cada subcarpeta contiene las interfaces `.hpp`
y sus implementaciones `.cpp`:

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
Todos reutilizan el cálculo de `src/render/`.

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
| `docs/diseno.md` | Diagramas, políticas de ejecución y sincronización |

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
parte de esa luz y generan sombras suaves de contacto. Las trayectorias
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

El benchmark calcula estas métricas:

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

Los comandos de [benchmarks individuales](#ejecutar-benchmarks-individuales)
permiten variar P con la misma carga. Para fine, `--quantum 1` usa el mínimo
implementado: un píxel completo por turno. La campaña guardada usa K = 64;
es una simulación de turnos cooperativos, no de ciclos de CPU.

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

Los intervalos y las esperas de coarse forman parte de su configuración;
cambiarlos durante el barrido modifica la carga además de P.
`make profile` incorpora perfilado con perf. Los datos guardados están en
[resultados disponibles](#resultados-disponibles).

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
`results/plots/metrics/`. Cada archivo conserva sus gráficas en una carpeta propia.
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
  `escalabilidad-eficiencia.png`.
- `reporte.md`: cantidad de repeticiones, medias, desviaciones, intervalos y
  cantidad de ejecuciones. Indica si n es menor que 200.

Todas usan colores consistentes: gris para sequential, violeta para fine,
naranja para coarse, turquesa para CMP y rosa para SMT. Las barras principales
muestran valores y el IC95 aproximado. Al pie se indica la carga (resolución,
tiles, muestras y rebotes); las etiquetas incluyen cantidad de repeticiones.
Los boxplots son apoyo para estudiar variabilidad: caja = mitad central,
línea = mediana y puntos = valores atípicos. Las unidades de los tiempos
en las gráficas son segundos; los CSV y el reporte conservan milisegundos.

Para comparar los cuatro modelos, usa los comandos de
[benchmarks individuales](#ejecutar-benchmarks-individuales), conserva un archivo
por modelo y genera la comparación conjunta con `make plots CSV="results/*.csv"`.

El script omite los CSV de resumen y rechaza cargas diferentes (resolución,
tiles, muestras, rebotes o semilla) para evitar comparaciones engañosas. Si se
incluyen varios archivos de la misma configuración, sus iteraciones se agrupan;
no incluyas copias de las mismas mediciones. K y los bloqueos distinguen políticas.
Una única configuración produce un punto, no demuestra escalabilidad por sí sola.
Puedes cambiar `CSV`, `PLOT_DIR` y `PYTHON` en los comandos Make. Los archivos CSV
y las gráficas se sobrescriben al volver a usar la misma ruta: conserva campañas
distintas en rutas diferentes.

La campaña usa **200 repeticiones por configuración**. El error estándar
disminuye como `desviación / sqrt(n)` bajo independencia. El margen relativo
del IC95 (`margen / media`) expresa la precisión estimada de tiempo y speedup.
Ese intervalo no corrige cambios térmicos, actividad externa ni correlación
entre ejecuciones. Las pruebas con `--repetitions 5` generan las mismas gráficas
y se identifican en el reporte como pruebas cortas.

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

## Decisiones de implementación

C++17 permite ejecutar `std::thread` en varios núcleos y controlar la afinidad
mediante la API de Linux. La escena y la cámara se comparten para lectura;
cada tile escribe una región exclusiva de la imagen. Los contadores atómicos
coordinan el reparto y la finalización. Coarse utiliza además un mutex y una
variable de condición para las esperas interrumpibles. El hilo principal atiende
SDL y espera a los trabajadores antes de presentar el buffer.

Se comparan dos formas de asignar tiles:

| Criterio | Un hilo por tile | Grupo de P trabajadores |
| --- | --- | --- |
| Creación de hilos | 300 con la imagen y tiles predeterminados | P, independiente de la cantidad de tiles |
| Reparto | Cada hilo conserva su tile | Un contador atómico entrega el siguiente tile libre |
| Costo | Más creación de hilos y posible contención | Sincronización al reclamar cada tile |
| Escalabilidad | Cambiar los tiles cambia también los hilos | Permite variar P con la partición fija |
| Uso | Coarse predeterminado | Coarse con `--threads`, SMT y CMP |

El grupo de trabajadores permite estudiar escalabilidad con carga fija. Fine
utiliza otra política: una cola de tareas lógicas sobre un hilo real. Los
[diagramas de diseño](docs/diseno.md) detallan el avance y la sincronización.
Python se utiliza únicamente para procesar CSV y generar gráficas después de
medir; el renderizador y los modelos de ejecución están escritos en C++.

## Resultados disponibles

La campaña guardada en [resultsrt/benchmark.txt](resultsrt/benchmark.txt) usa
640 × 480 píxeles, tiles de 32 × 32, 8 muestras, 12 rebotes y semilla 2026.
Contiene 200 ejecuciones por configuración y una referencia secuencial compartida
de 200 ejecuciones. El [reporte conjunto](resultsrt/plots/comparison/reporte.md)
incluye tiempos, desviaciones e IC95. Estos son algunos de sus resultados:

| Configuración | Tiempo medio (ms) | Speedup | Eficiencia |
| --- | ---: | ---: | ---: |
| Sequential | 1115.26 | 1.000 | 100.0 % |
| Fine, K = 64, P = 1 | 1116.60 | 0.999 | 99.9 % |
| Coarse, P = 8 | 252.17 | 4.423 | 55.3 % |
| CMP, P = 8 | 233.26 | 4.781 | 59.8 % |
| SMT activado, P = 12 | 229.23 | 4.865 | 40.5 % |
| SMT desactivado, P = 12 | 245.82 | 4.537 | 37.8 % |

Fine tiene un tiempo cercano al secuencial porque intercala tareas sin
paralelismo real. CMP reduce el tiempo al repartir tiles entre núcleos, aunque
con P = 8 el speedup queda por debajo del ideal 8×. En coarse, las esperas
simuladas y la planificación forman parte del tiempo medido. Con SMT activado,
P = 8 y P = 12 registran medias casi iguales (229.19 y 229.23 ms); esta campaña
no muestra una mejora clara al añadir esos cuatro trabajadores. Al comparar
on/off con P = 12, la media con SMT activado es aproximadamente un 6.7 % menor.
La referencia común, el orden de las fases y la mezcla de núcleos P/E limitan
la atribución de esa diferencia exclusivamente al SMT.

Las capturas de [perf](resultsrt/perf/comparison/README.md) incluyen 20
repeticiones por configuración en la campaña guardada. El valor predeterminado
para campañas nuevas sigue siendo 5. Los metadatos identifican un Intel Core
i5-12450H, Linux y una topología de 8 núcleos físicos y 12 CPUs lógicas con SMT
activado. Los reportes conservan las advertencias sobre tiempo de contador y
separan las PMU de P-cores y E-cores.

## Referencias

Los fundamentos de geometría, antialiasing, materiales difusos, metales,
dieléctricos, gamma y escena final se describen en
[Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html).
La presentación del buffer utiliza
[SDL_UpdateTexture](https://wiki.libsdl.org/SDL2/SDL_UpdateTexture).
