# Framework experimental de modelos de ejecución Multithreading

Abre `index.html` en un navegador. Funciona directamente desde el disco y no
requiere instalación, conexión a Internet ni un servidor. La navegación superior
selecciona un problema; cada vista ofrece sus resultados, filtros por modelo,
exportación CSV y las cuatro gráficas originales ampliables.

Toda la vista está contenida en `defensa/`. Puedes copiar o compartir esta carpeta
sin incluir el resto del repositorio:

```text
defensa/
  index.html
  styles.css
  app.js
  data.js
  generar_datos.py
  README.md
  resultados/
    dl/
    nbody/
    ray/
    sentinel/
```

Cada carpeta de resultados contiene las cuatro gráficas de comparación, el
reporte y su análisis de Amdahl con la imagen correspondiente. Son copias de los
artefactos originales; las mediciones de los proyectos conservan su ubicación.

Después de regenerar los reportes en el repositorio, actualiza las copias de
reportes, imágenes y tablas desde la raíz:

```sh
python3 defensa/generar_datos.py --actualizar
```

Para regenerar solamente `data.js` desde los reportes incluidos en una copia
independiente de `defensa/`, ejecuta `python3 generar_datos.py` dentro de ella.

Para compartir por un puerto de VS Code, abre la terminal en `defensa/` y ejecuta:

```sh
python3 -m http.server 8000 --bind 127.0.0.1
```

Comparte el puerto 8000 desde el panel Puertos. Así el servidor muestra únicamente
el contenido de esta carpeta.

El encabezado «Referencia 200» conserva los valores originales de cada reporte.
En DL y Sentinel la fuente llama a esa columna «n referencia»; la vista lo aclara
y conserva la cantidad real. No se inventan ni completan repeticiones.

Cada problema tiene una escena en la cabecera y un fondo animado propio que
acompaña toda la vista: conexiones neuronales, galaxias, rayos reflejados o curvas
de terreno con barrido radar. Son ilustraciones independientes de las mediciones.
El botón de pausa controla ambos; respetan la preferencia de movimiento reducido
y se detienen cuando la pestaña no está visible. El fondo se dibuja a un máximo
de 24 fotogramas por segundo.
