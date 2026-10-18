# Organización del código y paralelismo por tiles

El renderizador calcula una imagen mediante trayectorias de luz independientes
por píxel. Los modelos de ejecución comparten la escena, la cámara, la imagen
y la partición. La diferencia entre modelos corresponde a la planificación y
a la asignación de tareas a trabajadores.

## Variable paralelizada

El dominio de trabajo es el **índice global de tile**, de `0` a `M - 1`.
Cada índice selecciona un rectángulo de `TileGrid`. Se distribuyen iteraciones
del recorrido de tiles entre trabajadores; las coordenadas de los objetos y
los rebotes de una trayectoria no constituyen la dimensión de reparto.

| Símbolo o variable | Significado | Ámbito |
| --- | --- | --- |
| `M = tiles.size()` | Número de regiones de la imagen | Común a todos los modelos |
| `P` | Número de trabajadores reales | Configuración de ejecución |
| `i` en `sequential.cpp` | Índice del tile del recorrido de referencia | Privado del ejecutor |
| `next` en `parallel.cpp` | Próximo índice global que puede reclamarse | Compartido y atómico |
| `index` en `parallel.cpp` | Tile reclamado por un trabajador | Privado del trabajador |
| `i` en la creación de hilos de `parallel.cpp` | Identificador del trabajador y posición en `cpus` | Copiado en su lambda |
| `next_tile` en `coarse.cpp` | Próximo tile en el grupo limitado de trabajadores | Compartido y atómico |
| `tile_index` en `coarse.cpp` | Tile que el trabajador conserva durante cálculo y bloqueos | Privado del trabajador |
| `TileProgress.next` | Índice del siguiente píxel dentro de un tile | Privado de la tarea |
| `quantum` | Máximo de píxeles completos por turno de fine | Parámetro de planificación |
| `cpus[i]` | CPU lógica asignada al trabajador i | Destino de afinidad |

`next` y `TileProgress.next` tienen nombres similares, pero unidades distintas:
el primero cuenta **tiles reclamados**; el segundo cuenta **píxeles terminados
dentro de un tile**. `i` también cambia de función según el bucle: durante la
creación de hilos identifica trabajadores y durante el recuento identifica tiles.

## De la imagen a las tareas

Para una resolución `W × H` y un tile nominal `Tw × Th`:

```text
columnas = ceil(W / Tw)
filas    = ceil(H / Th)
M        = columnas × filas
columna  = índice % columnas
fila     = índice / columnas          (división entera)
x0       = columna × Tw
y0       = fila × Th
x1       = min(x0 + Tw, W)
y1       = min(y0 + Th, H)
```

`TileGrid::at` produce límites semiabiertos `[x0,x1) × [y0,y1)`. Un píxel del
borde derecho de un tile pertenece al siguiente, sin escritura duplicada.
Con `640 × 480` y tiles de `32 × 32`, hay `20 × 15 = 300` tareas. El número
M permanece constante cuando el benchmark cambia P.

## Distribución dinámica en SMT y CMP

La operación central de `render_pool` en
[parallel.cpp](../src/execution/parallel.cpp) es:

```cpp
const auto index = next.fetch_add(1);
if (index >= tiles.size()) break;
const bool done = render_tile(scene, camera, image, tiles.at(index),
                              [&] { return !cancel.load(); });
complete[index].store(done);
```

`fetch_add(1)` lee el valor anterior e incrementa el contador en una operación
atómica. Si dos hilos solicitan trabajo simultáneamente, reciben valores
distintos, por ejemplo 0 y 1. El orden de los solicitantes depende del sistema
operativo; la exclusividad del índice es independiente de ese orden.

Un trabajador que termina reclama otro tile. El reparto es dinámico: no asigna
previamente la misma cantidad de tiles a cada hilo. Esto permite que un hilo
que recibe regiones baratas continúe mientras otro calcula regiones con más
intersecciones o rebotes. El último incremento puede devolver M o un valor
mayor; la comprobación del límite ocurre antes de obtener el rectángulo.

CMP selecciona una CPU lógica por núcleo físico. SMT utiliza los contextos
lógicos y puede reutilizar destinos si P supera las CPUs disponibles. Esta
selección modifica dónde se ejecuta cada trabajador, sin cambiar el algoritmo
de reclamación de tiles ni el cálculo óptico.

## Reparto y bloqueos en coarse

En [coarse.cpp](../src/execution/coarse.cpp), `worker_limit == 0` selecciona
la asignación estática: el hilo i calcula el tile i y termina. Con 300 tiles
se crean 300 trabajadores.

Con `--threads P`, `next_tile.fetch_add(1)` obtiene el próximo índice y lo
almacena en `tile_index`. El trabajador puede procesar varios tiles de forma
sucesiva. Durante una espera simulada conserva el mismo tile y el avance de
su recorrido; el bloqueo no entrega esa región a otro hilo.

`block_interval_ms` determina cuándo se solicita la espera, usando tiempo de
pared desde la última reanudación. `block_duration_ms` determina su duración.
La variable de condición libera el mutex durante la espera y permite que la
cancelación despierte al trabajador antes de agotar el plazo.

## Recorrido local y planificación cooperativa

[raytracer.cpp](../src/render/raytracer.cpp) transforma el índice local de
píxel `p = progress.next` mediante:

```text
ancho_real = tile.x1 - tile.x0
x = tile.x0 + p % ancho_real
y = tile.y0 + p / ancho_real
```

El ancho real contempla tiles parciales del borde. Después de calcular todas
las muestras y rebotes de `(x,y)`, el trabajador escribe su RGB e incrementa
`progress.next`. La cancelación se comprueba antes del siguiente píxel.

Fine conserva una tarea `TileProgress` por tile en una cola. Extrae la primera,
avanza hasta K píxeles y reinserta al final la tarea incompleta. Todo sucede
en un único hilo real. Existe concurrencia lógica entre tareas, pero no cálculo
simultáneo en varios núcleos. Sequential completa cada tile antes del siguiente.

## Datos compartidos y sincronización

| Dato | Acceso | Mecanismo |
| --- | --- | --- |
| `scene`, `camera`, `tiles` | Lectura por todos los trabajadores | Referencias constantes durante el render |
| `image` | Escritura concurrente en elementos distintos | Propiedad exclusiva por índice de tile |
| `next` / `next_tile` | Lectura e incremento por trabajadores | `std::atomic` y `fetch_add` |
| `cancel` | Publicación y consulta de cancelación | Bandera atómica |
| `complete[t]` | Escritura por dueño de t; lectura por coordinador | Bandera atómica por tile |
| `finished` | Incremento por trabajadores; lectura por coordinador | Contador atómico y coordinación con mutex |
| `errors[i]` | Escritura por trabajador i; lectura después de `join` | Casillas disjuntas y reunión de hilos |
| `notified` | Lectura y escritura solo por coordinador | Sin acceso concurrente |
| Generador, muestras, suma RGB, rayos | Lectura y escritura dentro de un píxel | Variables privadas |

La ausencia de un mutex por píxel depende de dos propiedades: tiles sin
superposición y buffer con tamaño fijo. Compartir una línea de caché entre
regiones próximas puede afectar al rendimiento sin implicar una carrera
sobre el mismo objeto `Pixel`.

Los atómicos usan el orden de memoria predeterminado, secuencialmente
consistente. La escritura de `complete[t]` sucede después del cálculo de su
región. El coordinador consulta esa bandera antes de notificar el tile.
`join` sincroniza el final de todos los trabajadores antes de devolver la
imagen al código de presentación o serialización.

Los callbacks de aplicación pertenecen al coordinador. Un error en un trabajador
se almacena en `errors[i]`, publica cancelación y se relanza después de reunir
los hilos. Un error del coordinador también ejecuta cancelación y `join`,
conservando la vida útil de las referencias compartidas.

## Cálculo óptico y reproducibilidad

`render_pixel` construye una semilla con las coordenadas `(x,y)` y la semilla
de escena. El generador no depende del identificador del hilo. Las muestras
se suman siempre en el mismo orden dentro del píxel, conservando el orden
aritmético de punto flotante al cambiar P o el reparto.

`trace_path` busca la esfera más próxima para cada segmento. El material
selecciona dispersión difusa, reflexión metálica o reflexión/refracción en
vidrio. El impacto actual determina origen y dirección del siguiente segmento;
por ello los rebotes de una trayectoria tienen dependencia secuencial.
Al escapar, la trayectoria recoge la radiancia del cielo. El promedio lineal
de las muestras se transforma a RGB mediante gamma aproximada de 2.

## Convenciones de lectura del código

- `#include` incorpora declaraciones de otro archivo; `#pragma once` evita
  incluir repetidamente una cabecera en la misma unidad de compilación.
- `namespace rt` agrupa los símbolos del renderizador. Un espacio de nombres
  anónimo limita las funciones auxiliares al archivo de implementación.
- `const T&` recibe un objeto sin copiarlo y con acceso de lectura por esa
  referencia. `T&` permite modificar el objeto del llamante.
- `auto` deduce el tipo a partir de la expresión. `std::size_t` representa
  tamaños e índices de contenedores; no identifica por sí mismo la unidad.
- `struct` agrupa datos; `class` permite ocultar representación mediante
  miembros privados. Los métodos `const` no modifican el estado ordinario.
- Una lambda `[&, i]` comparte por referencia el contexto y copia `i`.
  El `join` anterior al retorno conserva la vida útil de esas referencias.
- RAII vincula recursos a la vida útil de objetos: archivos, mutex y punteros
  propietarios liberan sus recursos al salir del ámbito, incluso por excepción.
- `throw` propaga un fallo. `catch (...)` permite cancelar y reunir trabajadores
  antes de relanzarlo; `std::exception_ptr` transporta el fallo entre hilos.
- Las llaves delimitan ámbitos y la duración de sus variables automáticas.
  En `lock_guard` o `unique_lock`, el cierre del ámbito libera el mutex.

## Archivos auxiliares

Los comentarios de cada archivo describen su responsabilidad, funciones y
variables. `main.cpp` conecta configuración, escena y ejecución; `app/` contiene
argumentos y presentación; `scene/` contiene geometría, materiales y muestreo;
`render/` contiene imagen y cálculo; `execution/` contiene planificación y
afinidad; `metrics/` contiene medición y estadística.

Los scripts de `scripts/` coordinan campañas, interpretan contadores y generan
figuras después de medir. Los ensayos de `tests/` utilizan imágenes y valores
conocidos para comprobar equivalencia, planificación, cancelación y fórmulas.
El `Makefile` construye variantes y enlaza esas acciones. Sus comentarios y
los de los scripts distinguen la coordinación de procesos del paralelismo
interno de tiles.
