# Código de N-body y distribución del cálculo

La simulación representa dos poblaciones de cuerpos en el plano XY. En cada
fase, cada cuerpo recibe la atracción de todos los demás, incluidas las fuentes
de la otra galaxia. El costo dominante es la evaluación completa de pares,
con complejidad O(N²) por fase y N cuerpos totales.

## Variable paralelizada

La variable de descomposición es **`i`, índice del cuerpo receptor de la
aceleración**. Cada trabajador calcula una parte del dominio `i ∈ [0,N)` y
escribe exclusivamente los elementos `accelerations[i]` de su intervalo.

| Variable | Significado | Propiedad |
| --- | --- | --- |
| `bodies.size()` | N cuerpos de ambas galaxias | Constante dentro de una fase |
| `i` / `index` | Cuerpo receptor de la aceleración | Privado de la tarea o trabajador |
| `j` | Cuerpo fuente que ejerce atracción | Bucle local en orden ascendente |
| `worker` | Identidad del trabajador en `[0,P)` | Copiada al crear el hilo |
| `workers` | P trabajadores persistentes | Parámetro fijo del grupo |
| `begin`, `end` | Límites del intervalo de receptores | Privados del trabajador |
| `accelerations[i]` | Vector de salida a_i | Escritura exclusiva de su dueño |
| `Task.next_source` | Próxima fuente j de una tarea fine | Contexto privado del receptor i |
| `Task.partial` | Suma parcial a_i durante turnos fine | Contexto privado del receptor i |
| `generation_` | Identidad de la fase publicada | Compartida bajo mutex |
| `seen` | Última fase atendida por un trabajador | Privada del trabajador |
| `remaining_` | Trabajadores que aún no finalizaron la fase | Compartida bajo mutex |
| `step` | Paso temporal del integrador | Coordinador secuencial |

Los pasos temporales dependen del estado anterior. El paralelismo ocurre dentro
de una fase de aceleraciones. El índice `j` recorre fuentes para cada receptor
y conserva su orden de suma; no se realiza una reducción paralela de fuerzas
sobre una misma salida.

## Kernel gravitacional

[physics.cpp](../src/common/physics.cpp) calcula:

```text
a_i = Σ(j != i) G * m_j * (r_j - r_i)
                 / (|r_j - r_i|² + epsilon²)^(3/2)
```

`dx` y `dy` son componentes de `r_j-r_i`. `distance_squared` incorpora el
suavizado `epsilon²`; `factor` contiene el coeficiente escalar que multiplica
ambas componentes. La masa m_i del receptor se cancela al pasar de fuerza a
aceleración. La fuente j==i se omite.

`acceleration_for_body` inicia un `Vec2` en cero y llama a
`accumulate_accelerations` sobre todas las fuentes `[0,N)`. Este acumulador
vive en la pila del ejecutor de i. La función reanudable también acepta un
subintervalo de fuentes, utilizado por fine para continuar la misma suma.

## Partición estática y grupo persistente

[coarse_grained.cpp](../src/models/coarse_grained.cpp) contiene el grupo común
a coarse, SMT y CMP. Para el trabajador w:

```text
q = N / P                     (división entera)
r = N % P
begin_w = w*q + min(w,r)
end_w   = begin_w + q + (w<r ? 1 : 0)
```

Los primeros r trabajadores reciben q+1 cuerpos y los demás q. Los intervalos
son semiabiertos y consecutivos: `end_w = begin_(w+1)`. Cubren `[0,N)` sin
repeticiones. Por ejemplo, con N=10 y P=3:

| Trabajador | Intervalo i | Receptores |
| --- | --- | --- |
| 0 | `[0,4)` | 0,1,2,3 |
| 1 | `[4,7)` | 4,5,6 |
| 2 | `[7,10)` | 7,8,9 |

El bucle ejecutado por cada hilo es:

```cpp
for (std::size_t i = begin; i < end; ++i) {
    accelerations[i] = acceleration_for_body(bodies, i, gravity, epsilon);
}
```

La versión coarse intercala comprobaciones de tiempo antes de cada cuerpo.
Cuando ocurre un bloqueo, el trabajador conserva i y no entrega su intervalo
a otro hilo. El código utiliza asignación estática, sin contador atómico para
reclamar el próximo cuerpo. Si P>N, algunos trabajadores reciben intervalos
vacíos y participan igualmente en la barrera de fase.

Los P hilos se crean una vez en el constructor y se reutilizan para las fases
sucesivas. Las lambdas copian su identidad y P, y conservan `this` para acceder
al estado del grupo. Su vida útil termina con la parada y `join` del destructor.

## Publicación y barrera de fase

El coordinador redimensiona la salida antes de publicar el trabajo. Bajo
`mutex_`, guarda referencias prestadas a entrada y salida, copia G y epsilon,
asigna `remaining_=P` e incrementa `generation_`. Luego notifica
`work_ready_` y espera el predicado `remaining_==0` sobre `phase_done_`.

Cada trabajador espera `stopping_ || generation_ != seen`. Al recibir una
nueva fase, actualiza `seen`, obtiene sus alias y parámetros y libera el mutex.
El cálculo costoso ocurre sin mantenerlo adquirido. Al finalizar, recupera el
mutex, acumula bloqueos y decrementa `remaining_`. El último hilo notifica al
coordinador.

La secuencia de liberaciones y adquisiciones del mismo mutex publica las
escrituras de los trabajadores. Cuando la espera del coordinador termina,
ningún trabajador sigue leyendo las posiciones de esa fase. Entonces se
retiran los punteros prestados y el integrador puede modificar el estado.

| Dato | Acceso durante la fase | Sincronización |
| --- | --- | --- |
| `bodies` | Lectura compartida | El integrador espera el retorno de la fase |
| `accelerations` | Elementos distintos por trabajador | Partición exclusiva y tamaño fijo |
| `begin,end,i,j` y acumulador | Privados de cada hilo | Sin acceso concurrente |
| `generation_,remaining_,stopping_` | Lectura/escritura compartida | `mutex_` |
| `stats_.blocks` | Suma al terminar cada bloque | `mutex_` |
| `stats_.rounds,dispatches` | Coordinador después de terminar la fase | Orden de fases |
| `bodies_,accelerations_,gravity_,epsilon_` | Publicación bajo mutex y copias por fase | `work_ready_` y predicado de generación |

Una línea de caché puede contener salidas vecinas de trabajadores diferentes;
esa proximidad puede producir contención sin implicar escritura del mismo
objeto. La exclusividad se establece por elemento `Vec2`, no por línea de caché.

`stop()` publica `stopping_`, despierta las esperas y reúne los hilos. El
constructor también aplica esa limpieza si falla crear un trabajador o fijar
su afinidad. Los bloqueos temporizados utilizan `wait_for`, que libera el
mutex durante la suspensión y permite que la parada la interrumpa.

## Los cinco modelos

| Modelo | Trabajo asignado | Ejecución |
| --- | --- | --- |
| Sequential | Receptores i en orden | Un ejecutor calcula cada suma completa |
| Fine | Una tarea por receptor i | Un hilo intercala hasta K interacciones por turno |
| Coarse | Intervalos estáticos de i | P hilos persistentes con bloqueos temporizados |
| SMT | Los mismos intervalos de i | Grupo común sin bloqueos, con destinos lógicos |
| CMP | Los mismos intervalos de i | Grupo común sin bloqueos, con un destino por núcleo físico |

En [fine_grained.cpp](../src/models/fine_grained.cpp), `next_source` permite
reanudar la tarea sin empezar su suma desde cero. Al cruzar la posición propia,
el intervalo se amplía cuando es posible, porque la autoatracción no consume
una interacción de quantum. Al terminar todas las fuentes, `partial` se copia
a la salida del receptor y se reduce `pending`. Todo ocurre en un hilo real.

[smt.cpp](../src/models/smt.cpp) selecciona primero una CPU lógica por núcleo,
después sus hermanos y, para P>L, repite destinos. Esa repetición significa
sobresuscripción del SO. [cmp.cpp](../src/models/cmp.cpp) limita P a C núcleos
físicos disponibles. La afinidad decide dónde corre un trabajador; no cambia
su intervalo de receptores ni la fórmula gravitacional.

## Integración y captura

El estado inicial requiere calcular a(0). Cada paso posterior usa Euler
simpléctico:

```text
v(t+dt) = v(t) + a(t)*dt
r(t+dt) = r(t) + v(t+dt)*dt
fase: calcular a(t+dt) con todas las posiciones ya actualizadas
```

La actualización se realiza en el coordinador. Después de la barrera de
aceleraciones, `TrajectoryWriter` guarda el frame completo. El archivo contiene
el estado inicial más los S pasos, para un total de S+1 frames.

[trajectory.cpp](../src/common/trajectory.cpp) escribe una cabecera de 40 bytes
y registros de 60 bytes por cuerpo: masa, posición XY, velocidad XY,
aceleración XY y galaxia int32. Serializa campos explícitos para no depender
del padding de `Body`. El lector valida tamaño y orden de bytes antes del
acceso aleatorio.

## Intervalos medidos y reproducción

[main.cpp](../src/main.cpp) inicia el cronómetro después de crear las galaxias
y el buffer auxiliar. Incluye construcción del modelo, grupo de trabajadores,
archivo, fase inicial, pasos, captura, cierre de hilos, vaciado y cierre del
archivo. Hashes y análisis estadístico se ejecutan posteriormente en Python.

[viewer.cpp](../src/viewer.cpp) reproduce los estados guardados. Pausa, búsqueda,
cámara, estelas y velocidad no vuelven a integrar física. El historial de estelas
almacena posiciones reales con muestreo en tiempo simulado y longitud acotada.
Los halos y puntos de fondo pertenecen exclusivamente a la presentación.

## Rutas de implementación

El Makefile construye `build/nbody` con `src/main.cpp`, `src/viewer.cpp`,
`src/common/` y `src/models/`, utilizando `src/nbody.hpp`.

La ruta anterior utiliza las cabeceras `include/nbody/` y los archivos
`src/config.cpp`, `src/simulation.cpp`, `src/native_view.cpp`,
`src/diagnostics.cpp` y `src/output.cpp`. Contiene simulación secuencial integrada
con visualización y diagnósticos físicos. Esos archivos no están en SOURCES del
Makefile actual. Sus tipos y valores predeterminados se documentan dentro de
esa ruta, sin combinarlos con las definiciones de la interfaz multihilo.

## Scripts y verificación

`scripts/benchmark.py` coordina ejecuciones secuenciales de los modelos, verifica
SHA256 de trayectorias, conserva referencias y controla fases SMT. Su estimación
de speedup compartido usa razón de medias y propagación delta. El análisis
`scripts/analyze.py` utiliza Student t para una única configuración y genera
estadística por prefijos e histograma SVG. Son dos procedimientos distintos.

`scripts/plot_metrics.py` conserva observaciones crudas por carga, política y P,
y produce distribuciones, intervalos y ajuste descriptivo de Amdahl.
`scripts/profile.sh` captura contadores instrumentados en una campaña separada.

Los ensayos cubren ecuaciones conocidas, equivalencia entre modelos, grupos
con residuos y bloques vacíos, barreras reutilizadas, afinidad, trayectoria,
controles SDL, contratos CLI, estadística y restauración simulada de SMT.
Los comentarios de cada archivo explican sus funciones, variables y criterios
de comprobación.
