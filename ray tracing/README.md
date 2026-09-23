# Ray Tracing

Para conocer el diseño del proyecto y ver los diagramas de ejecución por tiles,
consulta [docs/diseno.md](docs/diseno.md).

Proyecto de Arquitectura de Computadores II en C++17. La escena contiene esferas
mates, metálicas y de vidrio sobre un suelo, iluminadas por el cielo mediante
múltiples muestras y rebotes (path tracing). La cámara ofrece una vista baja y
frontal. El cálculo se hace en CPU, sin hilos de renderizado adicionales; SDL2
muestra el buffer RGB completo al terminar y atiende eventos durante el cálculo.
La imagen permanece visible hasta cerrar la ventana o pulsar Esc. 

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
| `docs/diseno.md` | Diagramas y variables para próximas etapas |

La escena se modifica en `default_scene()` de `src/scene.cpp`. Las dimensiones
predeterminadas están en `Options`, en `src/options.hpp`. La cámara está en
`(0, 0.45, 2.0)` y apunta a `(0, 0.25, -7.5)`; ambos vectores se ajustan en
`src/main.cpp`. El campo de visión vertical es de 36° y está en `src/camera.cpp`.
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

Edita `src/scene.cpp`, dentro de `default_scene()`. Las primeras llamadas a
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


## Referencias y asistencia

Se consultaron las secciones de geometría, antialiasing, materiales difusos,
metales, dieléctricos, gamma y escena final de
[Ray Tracing in One Weekend](https://raytracing.github.io/books/RayTracingInOneWeekend.html)
y los parámetros de presentación con
[SDL_UpdateTexture](https://wiki.libsdl.org/SDL2/SDL_UpdateTexture).


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

Los diagramas y las decisiones para fine-grained y coarse-grained están en
[el documento de diseño](docs/diseno.md). Allí se explica cómo se reparten los
tiles, qué significa K píxeles completos y qué falta implementar.