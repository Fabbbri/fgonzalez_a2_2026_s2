# Perfilado con perf y comportamiento bajo SMT

Dos hilos lógicos de un mismo núcleo físico comparten recursos
de ejecución y algunas cachés. SMT puede aprovechar recursos que un solo hilo
deja libres, pero también puede provocar competencia. Crear más trabajadores no
garantiza mayor rendimiento. Perf registra contadores de ejecución para
comparar el uso de esos recursos.

El ray tracer continúa calculando tiles en C++. `scripts/profile_perf.py` ejecuta
`perf stat` alrededor del binario; la herramienta no modifica sus algoritmos ni
su planificación. El análisis y las gráficas son posteriores a cada captura.

## Comandos

`make profile-compare` genera una comparación general desde `resultsrt/perf/`
en `comparison/`, sin ejecutar perf ni modificar CSV. Hay una gráfica por
categoría, con filas por modelo/configuración y paneles separados para las PMU.
Las referencias importadas no se duplican; las referencias pareadas independientes
se mantienen separadas. No se mezclan cargas, CPU, eventos, alcances ni modos de
conteo diferentes. `PROFILE_COMPARE_DIR=results/perf` permite otra carpeta.
`make run-all` incorpora la comparación al finalizar su campaña.

Las capturas nuevas usan `PROFILE_COUNT_MODE=observed` por defecto. Se aplica
`--no-scale` y se agrupan ciclos/instrucciones/cache-misses por PMU. Así se evita
extrapolar una residencia mínima en E-cores a todo el proceso y se intenta medir
los eventos de un mismo cociente juntos. Los conteos observados con poco tiempo
en cpu_atom se conservan; esto no significa que representen un total completo
si hubo multiplexado. No es posible reparar retrospectivamente una captura
escalada regenerando las gráficas: debe capturarse de nuevo. Los metadatos
registran `modo_conteo` y `flags_perf`; se rechaza una referencia compartida con
un modo distinto. `PROFILE_COUNT_MODE=scaled` conserva el procedimiento anterior.

Para regenerar todas las gráficas guardadas: `make profile-plots-all`. No ejecuta
perf ni cambia CSV. Reportes y gráficas conservan todos los conteos numéricos.
Un `porcentaje_activo` menor al 50 % o desconocido genera un aviso y rayado
naranja en barras y cajas, sin eliminar muestras del promedio, IC ni boxplot.
Las etiquetas indican el porcentaje medido y el número usado frente al total.
Las capturas antiguas se identifican como escaladas: una extrapolación desde
un periodo corto puede amplificar mucho el conteo; dibujarlo no corrige ese
sesgo. Solo se excluyen los casos sin medición, nunca se sustituyen por cero.
Una configuración sin conteos se muestra con un aviso y sin barra ni caja.
El IPC se recalcula desde ciclos e instrucciones de la misma
repetición, CPU y PMU, incluso al releer CSV antiguos; su cobertura es la menor
de ambos contadores. Los datos crudos se conservan para auditoría.

`make run-all PERF="sudo perf"` ejecuta benchmarks y perfiles de todos los
modelos. Mide sequential una sola vez sin instrumentación para el benchmark
(200 tiempos por defecto) y una sola vez bajo perf (`PROFILE_REPETITIONS=5`
por defecto). Los perfiles restantes importan estos últimos contadores mediante
`--baseline-capture`, sin ejecutar sequential otra vez. Las capturas compartidas
se guardan en `resultsrt/perf/sequential/` y su topología original queda registrada
en los metadatos, incluso si después se desactiva SMT. No se sustituyen contadores
de perf por tiempos sin instrumentar. Cada fase tiene su calentamiento no medido.

Los comandos individuales siguientes conservan la referencia pareada propia
si no se indica una captura compartida.

Verifica `perf --version` y consulta la topología con `make run ARGS="--cpu-info"`.
Los ejemplos siguientes usan P = 4 y P = 12: ajústalos a tu equipo. En CMP
P no puede superar C. Las configuraciones P > C asignan varios trabajadores
por núcleo; la comparación on/off usa los mismos P en ambos estados.

```sh
# Cinco perfiles de SMT y cinco de sequential con la misma carga
make profile ARGS="--model smt --threads 12 --samples 8" PROFILE_REPETITIONS=5

# CMP contra sequential
make profile ARGS="--model cmp --threads 4 --samples 8" PROFILE_REPETITIONS=5

# Contadores separados por CPU lógica (alcance de todo el sistema en esas CPUs)
make profile PROFILE_SCOPE=cpu ARGS="--model smt --threads 12 --samples 8"

# También se pueden perfilar los modelos fine y coarse
make profile ARGS="--model fine --quantum 1 --samples 8"
make profile ARGS="--model coarse --threads 4 --samples 8"
```

Por defecto son cinco ejecuciones perfiladas por configuración, con calentamiento
sin perf y orden alternado entre modelo y referencia. No se usa `perf -r` para
conservar cada iteración por separado. Para repetir 200 veces: `PROFILE_REPETITIONS=200`.
Esto no reemplaza `make benchmark`, que mide rendimiento sin el costo de perf.
`--repetitions` y `--thread-counts` pertenecen al benchmark; en profile se usa
`PROFILE_REPETITIONS` y una sola cantidad de trabajadores por comando.

Las campañas se guardan en carpetas nuevas dentro de `results/perf/`, identificadas
por modelo y fecha UTC, sin sobrescribir la anterior:

- `*.perf.csv`: salida cruda de perf, separada por punto y coma.
- `*.stdout.txt` y `*.stderr.txt`: resultado del programa y diagnósticos por
  ejecución si se activa `PROFILE_LOGS=1`. Por defecto, `PROFILE_LOGS=0`
  omite estos logs salvo stderr con avisos o errores. Los CSV crudos se conservan.
- `contadores.csv`: valores por modelo, repetición, CPU y evento/PMU; conserva
  estados como `<not supported>` sin convertirlos en ceros.
- `metadata.json`: comandos, CPUs observadas, topología, estado SMT informado por
  Linux, modelo de CPU, sistema, versión de perf y política de acceso a contadores.
- `reporte.md`: medias y desviación muestral de los valores válidos por evento.
- `evento-*.png`: barras de medias con valores e IC95 aproximado, colores por
  modelo, título y explicación del significado de cada evento. Una repetición
  no permite estimar IC95. Los conteos grandes se presentan en miles o millones
  y los clocks en segundos; las unidades originales permanecen en el CSV.
- `variabilidad-evento-*.png`: boxplots de todos los valores válidos por evento,
  incluyendo IPC derivado cuando hay ambos conteos. Caja = mitad central,
  línea = mediana; los valores atípicos se conservan.

Para actualizar las gráficas de una captura existente sin volver a ejecutar perf:

```sh
make profile-plots PROFILE_CAPTURE=results/perf/nombre-de-campaña
```

La carpeta debe contener `contadores.csv` y `metadata.json`. Se actualizan solo
gráficas y reporte; las capturas crudas y los CSV permanecen intactos.

## Qué mide cada alcance

`PROFILE_SCOPE=process` observa el programa y sus trabajadores mediante la
herencia normal de contadores de perf. La medida incluye el proceso completo:
construcción de escena, cámara, buffers, render y finalización. No abre ventana
ni escribe PPM. El intervalo no coincide exactamente con `tiempo_total_ms`,
que mide únicamente la ejecución del modelo. No mezclar ambos tiempos.

`PROFILE_SCOPE=cpu` usa `perf stat -a -A -C ...` sobre las CPUs permitidas por la
afinidad del principal y conserva un resultado por CPU lógica. Incluye procesos
ajenos, kernel e interrupciones: **no atribuir todos esos contadores al ray tracer**.
Usar hardware físico dedicado, minimizar actividad externa y relacionar las
CPUs con la afinidad de los trabajadores registrada en stdout (activar
`PROFILE_LOGS=1` para conservar esa salida). No se restringen
las CPUs del principal ni se desactiva SMT desde el programa.

## Contadores e interpretación

| Evento | Qué aporta |
| --- | --- |
| `task-clock` | Tiempo CPU acumulado del proceso y sus trabajadores; puede superar el tiempo de pared |
| `cycles` | Ciclos contados en el alcance seleccionado; sumar hilos lógicos no equivale a contar ciclos físicos únicos |
| `instructions` | Instrucciones retiradas; ayuda a contrastar trabajo y sobrecarga de planificación |
| IPC derivado | Instrucciones / ciclos de la misma CPU, PMU y repetición; se conserva cada dominio separado |
| `context-switches` | Cambios de contexto observados por el SO; los turnos cooperativos de fine no necesariamente aparecen aquí |
| `cpu-migrations` | Migraciones observadas por el SO; los trabajadores SMT/CMP usan afinidad, pero el principal puede migrar |
| `cache-misses` | Fallos de caché según el evento disponible; no es una medida directa de stalls ni su duración |

En modo cpu, `task-clock` se sustituye por `cpu-clock`. Estos clocks no ofrecen
por sí solos un porcentaje de utilización específico de cada trabajador.
El uso por hilo y los stalls detallados pueden requerir una captura adicional
con muestreo o eventos específicos de la microarquitectura.

En CPUs híbridas, perf puede presentar `cpu_core/...` y `cpu_atom/...`. No se
suman como si fueran el mismo contador. El script deriva IPC solo con el par
cycles/instructions de igual CPU y PMU. Un IPC menor con SMT no demuestra por
sí solo peor rendimiento: puede coincidir con un menor tiempo de ejecución.

Perf puede multiplexar eventos si no hay suficientes contadores físicos. La
salida conserva `porcentaje_activo` y `tiempo_contador`; los valores suelen estar
escalados. Con porcentajes bajos o IPC dudoso, repetir con menos eventos o un
grupo compatible de ciclos e instrucciones. Cambiar eventos con:

```sh
make profile PROFILE_EVENTS="{cycles,instructions}" ARGS="--model smt --threads 12 --samples 8"
perf list
```

Los eventos de stalls y top-down varían por CPU. `perf list` muestra los
disponibles en el equipo. Un evento no soportado queda marcado en la captura
y no aporta una medición.

## Comparación de SMT activado y desactivado

La comparación utiliza la misma escena, tiles y cantidades de trabajadores en
ambos estados. Con P > C y SMT desactivado, los trabajadores adicionales se
alternan sobre las CPUs disponibles mediante el sistema operativo. Con SMT
activado, los hermanos lógicos comparten recursos de ejecución del núcleo.

Los metadatos registran topología y estado SMT informado por Linux. No distinguen
un cambio por sysfs de uno realizado desde BIOS/UEFI. El perfilado tampoco aísla
cambios de temperatura, frecuencia o actividad externa entre fases. En CPUs
híbridas, el tipo de núcleo asignado también influye en tiempos y contadores.

## Permisos y limitaciones

El script comprueba los eventos antes de renderizar. Si Linux impide abrirlos,
falla con el diagnóstico de perf; no modifica `perf_event_paranoid` ni configura BIOS.
Puedes consultar la política con `cat /proc/sys/kernel/perf_event_paranoid`.
En un equipo administrado por ti, y si tienes autorización para perfilar con
privilegios, puedes ejecutar únicamente perf como administrador:

```sh
make profile PERF="sudo perf" ARGS="--model smt --threads 12 --samples 8"
```

Make, el renderizador cuando calienta y el análisis se ejecutan como usuario;
el proceso observado por `sudo perf` puede heredar privilegios. Esto cambia
las condiciones de ejecución respecto a una captura sin sudo.
El kernel, la PMU o la virtualización pueden impedir el acceso a eventos de
hardware; los eventos de software miden fenómenos distintos.

Referencias: [manual de perf stat](https://man7.org/linux/man-pages/man1/perf-stat.1.html),
[tutorial oficial de perf](https://perfwiki.github.io/main/tutorial/),
[seguridad de perf](https://www.kernel.org/doc/html/latest/admin-guide/perf-security.html).
