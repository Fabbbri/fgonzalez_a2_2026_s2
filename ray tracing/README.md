# Ray tracer secuencial — Arquitectura de Computadores II

Renderizador secuencial en C++17 con múltiples rebotes de luz (path tracing):
cámara fija, esferas mates, metálicas y de vidrio sobre un piso, iluminadas por el cielo. El ray tracing se calcula en CPU en el hilo principal. SDL2
presenta el buffer RGB completo al terminar y atiende eventos durante el cálculo. La imagen permanece hasta cerrar la
ventana o pulsar Esc. No se implementan modelos multihilo en esta etapa.

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

No hay pausas artificiales ni vsync solicitado. La ventana permanece negra
durante el cálculo y muestra la escena completa al terminar, sin barrido visible.
Los tiles se recorren internamente en un único hilo; se mantienen para comparar
las políticas de ejecución futuras. Se puede cerrar la ventana durante el cálculo.

## Calidad y tiempo de espera

`make run` usa 128 muestras por píxel y hasta 12 rebotes. La ventana permanece
negra mientras calcula; ahora puede tardar varias decenas de segundos según el
CPU. No se añadió ninguna pausa artificial. Para probar la composición rápido:

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

| Archivo | Responsabilidad |
| --- | --- |
| `src/geometry.hpp`, `src/geometry.cpp` | Vectores, rayos, esferas e intersección geométrica |
| `src/scene.hpp`, `src/scene.cpp` | Objetos, materiales, cielo y configuración de calidad |
| `src/camera.hpp`, `src/camera.cpp` | Cámara y rayos hacia muestras dentro del píxel |
| `src/image.hpp`, `src/image.cpp` | Buffer RGB y escritura PPM |
| `src/tiles.hpp`, `src/tiles.cpp` | Partición rectangular y recorte de bordes |
| `src/material.hpp` | Tipo de material, rugosidad e índice de refracción |
| `src/sampling.hpp` | Muestras reproducibles por píxel sin estado global |
| `src/transport.hpp`, `src/transport.cpp` | Rebotes, reflexión, refracción e iluminación del cielo |
| `src/raytracer.hpp`, `src/raytracer.cpp` | Promedio de muestras, gamma y cálculo de un tile |
| `src/sequential.hpp`, `src/sequential.cpp` | Recorrido secuencial, tiempos y notificaciones |
| `src/options.hpp`, `src/options.cpp` | Configuración y validación de argumentos |
| `src/display.hpp`, `src/display.cpp` | Recursos SDL2, presentación y eventos |
| `src/main.cpp` | Conectar módulos, mostrar resultados y controlar la salida |
| `tests/test_core.cpp` | Geometría, iluminación, cobertura y cancelación |
| `tests/test_display.cpp` | Presentación y eventos con SDL dummy |
| `docs/diseno.md` | Diagramas y variables para próximas etapas |

La escena se modifica en `default_scene()` de `src/scene.cpp`. Las dimensiones
predeterminadas están en `Options`, en `src/options.hpp`. La cámara se coloca en `(7, 4, 5)` y apunta a `(0, -0.2, -6)`,
con campo vertical de 50°. Estos dos vectores se configuran en `main.cpp`.
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
`render_pixel`; `render_sequential` decide qué tile sigue. El callback opcional atiende
eventos antes de cada píxel, sin cambiar de tile. Esto permite cancelar incluso con tiles grandes; no es un planificador.
SDL recibe la imagen calculada solo después de completar todos los tiles.

## Cambiar o agregar esferas

Edita `src/scene.cpp`, dentro de `default_scene()`. Cada esfera tiene este formato:

```cpp
{{x, y, z}, radio, {rojo, verde, azul}}
```

Los colores están entre 0 y 1. La escena combina tres esferas principales con
muchas pequeñas sobre un suelo gris. La vista es oblicua. La composición está
inspirada en **A Final Render** de
[Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html).
La escena usa ahora mates, metales y vidrio, con sombras por oclusión del cielo
y reflejos producidos por los rebotes. No incorpora desenfoque de cámara.

En `default_scene()`, las primeras llamadas a `add_sphere` definen las tres
esferas principales. `rows = 10` y `columns = 13` generan 130 candidatos pequeños;
se omiten los que se solapan con otras esferas. La semilla `2026` fija las
posiciones, radios y colores entre ejecuciones. Cambiarla produce otra distribución.
Esa aleatoriedad de construcción ocurre antes de medir. Los rebotes usan otro
generador, local a cada píxel, y su costo sí forma parte del renderizado.

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

## Medición

Se utiliza `std::chrono::steady_clock`, un reloj monotónico:

- `render_ms`: suma del cálculo de tiles; excluye presentación y escritura.
  En modo ventana se resta el tiempo dentro del callback de eventos, aunque
  queda la pequeña sobrecarga de invocarlo y medirlo. Usar `--headless` para comparar.
- `recorrido_ms`: recorrido completo, con selección de tiles y medición;
  en modo ventana incluye eventos (y posibles redibujados del fondo). Excluye
  inicialización, presentación de la escena final, escritura y espera del usuario.
- `escritura_ms`: apertura, escritura y cierre del PPM, solo si se solicita.
  No fuerza persistencia física con fsync.

Estas comprobaciones funcionales no sustituyen las mediciones del enunciado en
hardware físico, sus repeticiones y su análisis estadístico.

## Verificación

```sh
make test
make test-display

# Igualdad entre particiones
./build/sdl/raytracer --headless --width 641 --height 479 --tile-size 32 --output a.ppm
./build/sdl/raytracer --headless --width 641 --height 479 --tile-width 71 --tile-height 23 --output b.ppm
cmp a.ppm b.ppm

# Igualdad entre modos; cerrar la ventana al terminar
./build/sdl/raytracer --width 641 --height 479 --output c.ppm
cmp a.ppm c.ppm
```

Las pruebas verifican impactos, tangencia, origen interior, objetos detrás,
impacto más cercano, reflexión, refracción, sombra por oclusión, gamma,
cancelación y cobertura de cada píxel
exactamente una vez. También comparan tiles en orden inverso con el cálculo
directo. Los resultados y limitaciones del entorno están en
[`docs/validacion.md`](docs/validacion.md).

## Referencias y asistencia

Se leyeron el enunciado y la propuesta en `../.local`, conservados sin modificaciones.
Se consultaron las secciones de geometría, antialiasing, materiales difusos,
metales, dieléctricos, gamma y escena final de
[Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
y los parámetros de presentación con
[SDL_UpdateTexture](https://wiki.libsdl.org/SDL2/SDL_UpdateTexture).

Código, comentarios, documentación y pruebas se prepararon con asistencia de
Codex. Para el anexo exigido por el enunciado, el equipo debe registrar además
su revisión humana y sus validaciones. Esta entrega cubre la base secuencial:
las ejecuciones triviales medidas de los modelos multihilo siguen pendientes.

## Dependencias entre módulos

Cada `.hpp` declara la interfaz y su `.cpp` implementa el comportamiento.
`main` conecta las opciones, la presentación y la ejecución secuencial.
`sequential` utiliza `raytracer`, que calcula píxeles a partir de la escena,
la cámara y la geometría y los escribe en `image` según los límites de `tiles`.
`display` solo necesita imagen y tiles; el núcleo no incluye SDL.

`RenderHooks` permite notificar tiles terminados y consultar cancelación sin
acoplar el recorrido a una ventana. Sin callbacks, funciona sin interfaz gráfica.
Los comentarios `FUTURA PARALELIZACIÓN` están en `sequential.cpp` (asignación)
y `raytracer.cpp` (cálculo). La versión actual sigue siendo secuencial.

La ampliación visual implementa esos fundamentos con nuestros módulos, un bucle
iterativo de rebotes y muestras locales por píxel. No se importó el código fuente
del libro. Es una ampliación del alcance inicial solicitada para mejorar la escena;
el proyecto conserva su ejecución secuencial y la partición común para futuros modelos.

## Tiles y avance respecto a la propuesta

La unidad de trabajo sigue siendo el tile: con 640×480 y tiles 32×32 hay 300
regiones. Los rayos secundarios consultan la geometría, sin depender del buffer
de otros píxeles. Por ello la ampliación visual conserva la independencia de tiles.

La correspondencia con el LaTeX, la precisión de K como píxeles completos y los
pendientes de la demostración 2 están en [el documento de diseño](docs/diseno.md#correspondencia-con-la-propuesta-latex).
Los diagramas actuales están en Markdown; el LaTeX original de la demostración 1
no fue modificado. Los modelos multihilo y sus ejecuciones triviales medidas aún
no están implementados.
