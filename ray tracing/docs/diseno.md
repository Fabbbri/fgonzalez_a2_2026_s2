# Diseño del ray tracing por tiles

La base actual renderiza en un solo hilo de CPU. La imagen se divide en tiles,
pero `render_sequential` termina uno antes de comenzar el siguiente y SDL2
presenta el resultado al finalizar. Fine-grained y coarse-grained son diseños
para etapas posteriores: ambos usarán la **misma escena, cámara, imagen y
partición en tiles**. Lo que cambiará será la política de ejecución.

## Unidad común de trabajo

Para una imagen de ancho `W` y alto `H`, y tiles de `Tw × Th` píxeles:

`M = ceil(W / Tw) × ceil(H / Th)`

`TileGrid` produce los `M` rectángulos con límites superiores exclusivos
`[x0, x1) × [y0, y1)`. Recorta los de la última fila y columna si la resolución
no es divisible por el tamaño del tile. Con 640 × 480 y tiles de 32 × 32 hay
20 × 15 = **300 tiles**. Cada píxel pertenece exactamente a uno.

La **tarea** que se reparte es un tile; el trabajo dentro de ella es calcular
sus píxeles. `render_pixel` completa todas las muestras y rebotes de un píxel;
`render_tile` recorre los píxeles del rectángulo. Una tarea puede guardar el
índice del siguiente píxel para continuar después. La escena y la cámara se
comparten solo para lectura. Cada tile escribe una región exclusiva del buffer,
por lo que no hace falta bloquear cada píxel si la imagen no se redimensiona ni
se presenta mientras los trabajadores escriben. La semilla depende de las
coordenadas del píxel: cambiar el orden de tiles no cambia sus muestras.

```mermaid
flowchart TD
    A[Imagen W por H] --> B[TileGrid: M rectángulos]
    B --> C[Política de ejecución]
    C --> D[Tile i: píxeles de su región]
    D --> E[render_pixel: muestras y rebotes completos]
    E --> F[Buffer RGB: región exclusiva del tile i]
    F --> G[Esperar todos los tiles]
    G --> H[Presentar o guardar imagen completa]
```

La parte que se sustituirá está en el recorrido de `sequential.cpp`. El cálculo
de `render_pixel` y la geometría son comunes a los modelos. En la base actual,
`render_tile` termina el rectángulo completo; durante el cálculo la ventana
permanece negra y receptiva. En los modelos con hilos, la presentación y los
eventos SDL permanecerán en el hilo principal. Los trabajadores avisarán cuando
termine un tile; al cerrar la ventana, una señal de cancelación se comprobará
entre píxeles, se esperará la salida de los trabajadores y no se guardará una
imagen parcial.

## Fine-grained: turnos cortos entre tareas de tiles

Se crea **una tarea lógica por tile**, con su rectángulo y un índice `p` del
siguiente píxel pendiente. Un planificador cooperativo toma las tareas activas
en orden circular. En cada turno calcula como máximo **K píxeles completos**
del tile elegido; si aún quedan píxeles, guarda `p` y pasa obligatoriamente al
siguiente tile, aunque no haya ocurrido ningún bloqueo. Al terminar un tile,
lo retira de la ronda. K es una cantidad de píxeles, no una duración en tiempo.

Para reanudar, el ancho real del tile es `x1 - x0`. A partir de `p` se obtiene
`x = x0 + p % ancho` y `y = y0 + p / ancho`. Así también funcionan los tiles
recortados de los bordes. El turno acaba **entre** píxeles: no se interrumpe una
muestra ni un rebote, y no hay que guardar el estado interno de un rayo.
Actualmente `render_tile` siempre llega al final; hará falta una variante que
acepte `p` y el límite K. Por ejemplo, con `K = 64`, la ronda procesa los
píxeles 0–63 del tile 0, luego 0–63 del tile 1, y vuelve al tile 0 para sus
píxeles 64–127.

```mermaid
flowchart TD
    A[TileGrid: M tiles] --> B[Crear M tareas: tile y p igual a 0]
    B --> C{Quedan tareas activas}
    C -- Sí --> D[Elegir siguiente tarea en round-robin]
    D --> E[Calcular hasta K píxeles completos]
    E --> F{Tile terminado}
    F -- No --> G[Guardar p y volver al final de la ronda]
    G --> C
    F -- Sí --> H[Retirar tarea terminada]
    H --> C
    C -- No --> I[Imagen completa: presentar o guardar]
```

Este intercalado puede implementarse con **un solo hilo del sistema**. En ese
caso simula los cambios frecuentes de tarea, pero no calcula dos tiles a la
vez ni ofrece speedup por varios núcleos. Si se añadieran trabajadores, habría
que documentar esa decisión como otra configuración del modelo.

## Coarse-grained: hilos que avanzan hasta un bloqueo

La propuesta LaTeX asigna **un hilo del sistema por tile**. Cada hilo avanza
continuamente por los píxeles de su tile. Solo el evento de bloqueo costoso
simulado provoca una cesión **a nivel de la lógica del modelo**: el hilo conserva
el siguiente índice `p`, espera y luego continúa su mismo tile. No hay un turno
obligatorio cada K píxeles ni una cola round-robin de tareas.

Un temporizador puede marcar cuándo toca el evento y una espera suspender
al hilo. La marca se comprobará después de terminar el píxel actual para
conservar el resultado. El temporizador **dispara un bloqueo**, no representa
un quantum
fine-grained. Los hilos reales también pueden ser desplanificados por el sistema
operativo en cualquier momento; el programa no puede garantizar cambios de
contexto exclusivamente durante los bloqueos simulados.

```mermaid
flowchart TD
    A[TileGrid: los mismos M tiles] --> B[Crear un hilo Ti para cada tile Bi]
    B --> C[Ti calcula píxeles completos de Bi continuamente]
    C --> D{Evento de bloqueo simulado}
    D -- Sí --> E[Conservar p y esperar]
    E --> F[Reanudar Ti en Bi]
    F --> C
    D -- No --> G{Bi terminado}
    G -- No --> C
    G -- Sí --> H[Ti finaliza]
    H --> I[Esperar fin de todos los hilos]
    I --> J[Imagen completa: presentar o guardar]
```

Con la configuración inicial serían **300 hilos para 300 tiles**. Eso sigue
literalmente la propuesta, aunque crear tantos hilos puede costar más que el
trabajo de tiles pequeños y aumentar la contención. Una alternativa futura sería
usar `P` trabajadores que tomen varios tiles; conservaría la partición espacial,
pero sería un cambio explícito respecto al modelo de un hilo por tile del LaTeX.
Todavía no se ha elegido ni implementado esa alternativa.

| Aspecto | Fine-grained | Coarse-grained |
| --- | --- | --- |
| Unidad asignada | Tarea lógica por tile | Hilo del sistema por tile en la propuesta |
| Avance | Hasta K píxeles completos por turno | Píxeles continuos hasta bloqueo o fin del tile |
| Motivo de cesión modelado | Fin obligatorio del turno | Evento de bloqueo simulado |
| Estado para continuar | Tile e índice `p` | Mismo tile e índice `p` |
| Paralelismo real inicial | Ninguno con un solo planificador | Posible en varios núcleos, sujeto al SO |

## Variables que afectan el paralelismo

Conviene distinguir **más trabajo disponible** de **más ejecución simultánea**.
Aumentar muestras o rebotes encarece cada píxel, pero no crea tiles nuevos.
Reducir el tamaño del tile crea más tareas independientes, pero tampoco crea
núcleos de CPU. Las variables principales son:

| Variable | Efecto al repartir tiles | Compromiso o condición de comparación |
| --- | --- | --- |
| Resolución `W × H` | Más píxeles y, con `Tw` y `Th` fijos, más tiles `M` | Aumenta trabajo total y memoria; la imagen debe ser idéntica entre modelos |
| Tamaño `Tw × Th` | Determina `M` y cuántos píxeles tiene cada tarea | Tiles pequeños facilitan repartir carga desigual, pero elevan costo de planificación; tiles grandes reducen tareas y pueden dejar núcleos ociosos |
| Trabajadores `P` y núcleos físicos `C` | Fijan cuántos tiles pueden calcularse a la vez en un modelo realmente paralelo | En trabajo de CPU, `P` mayor que `C` no implica más cómputo simultáneo; en fine-grained con un planificador `P = 1` |
| Quantum `K` | Determina cada cuánto alterna fine-grained entre tiles | K pequeño aumenta intercalado y sobrecarga; K grande se aproxima a terminar un tile por turno. No aumenta por sí solo el paralelismo físico |
| Frecuencia y duración del bloqueo | Determinan cuánto tiempo puede avanzar cada hilo coarse-grained | Más bloqueos pueden dejar hilos inactivos y añadir tiempo de espera; no cambian los píxeles de la imagen |
| Muestras `S` y rebotes máximos `R` | Aumentan trabajo por píxel y pueden hacer rentable distribuir tiles | No crean tareas nuevas; mantenerlos fijos al comparar tiempos y calidad |
| Esferas `N` y su distribución | Más objetos elevan pruebas de intersección; unas zonas pueden costar más que otras | La carga por tile puede ser desigual; conservar escena y semilla para comparar políticas |

Una cota aproximada del trabajo de intersecciones, por la búsqueda lineal actual,
es `O(W × H × S × (R + 1) × N)`. Es una **cota**, no el tiempo exacto: algunos
rayos escapan antes y otros rebotan varias veces. Con los datos iniciales hay
hasta 75 esferas decorativas, tres principales y una esfera grande que hace de
suelo. El buffer RGB ocupa `3 × W × H` bytes, sin contar estructuras auxiliares.

Para aprovechar varios núcleos se necesitan suficientes tiles listos y
trabajadores que puedan ejecutarlos: de forma ideal, los tiles activos a la
vez no superan `min(M, P, C)`. En la práctica influyen la carga desigual, el
costo de crear o planificar tareas, la memoria y los bloqueos. Por eso `M`, `P`
y `K` no deben confundirse: **M es la partición**, **P es la capacidad de
trabajo simultáneo** y **K es la duración lógica del turno fine-grained**.

Al comparar políticas se fijarán escena, cámara, resolución, tiles, muestras,
rebotes, semilla y compilación. El tiempo de renderizado paralelo será tiempo
de pared desde el comienzo del trabajo hasta que terminen todos los tiles; no
se sumarán tiempos de hilos. La ventana y la escritura PPM quedarán fuera de
ese intervalo, como en la medición sin ventana de la base actual.

