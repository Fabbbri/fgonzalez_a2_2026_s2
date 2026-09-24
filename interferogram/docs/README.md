# Cálculo de interferogramas con Sentinel-1 SLC

## Estado actual

La base secuencial en C++17 incluye:

* lectura de dos adquisiciones Sentinel-1 SLC reales correspondientes a P09-R2;
* selección de una misma ventana rectangular en ambas adquisiciones;
* conservación de las componentes real e imaginaria de cada muestra mediante GDAL;
* multiplicación conjugada píxel a píxel con complejidad $O(N)$;
* almacenamiento lineal de las muestras en orden por filas;
* validación de argumentos, dimensiones, ventanas, bandas y tipo de dato complejo;
* medición del tiempo del kernel separada de la lectura de archivos, validaciones e impresión;
* resumen numérico del resultado para comprobar las ejecuciones;
* pruebas del kernel matemático, del lector SLC y de la interfaz de línea de comandos.

Los modelos Fine-Grained, Coarse-Grained, SMT y CMP todavía no están implementados. Sus diagramas preliminares documentan cómo se plantea dividir y ejecutar el cálculo en cada modelo.

## Modelo computacional

Cada muestra de los productos SLC se representa mediante un número complejo:

$$
z = a + bi
$$

En la implementación, cada muestra almacena explícitamente sus componentes real e imaginaria:

```cpp
struct Complex {
    double real;
    double imag;
};
```

Para dos muestras correspondientes a una misma posición $i$:

$$
A_i = a_i + b_i i
$$

$$
B_i = c_i + d_i i
$$

el kernel calcula la multiplicación conjugada:

$$
I_i = A_i \overline{B_i}
$$

Como:

$$
\overline{B_i} = c_i - d_i i
$$

entonces:

$$
I_i =
(a_i c_i + b_i d_i)
+
(b_i c_i - a_i d_i)i
$$

La implementación secuencial realiza directamente estas operaciones:

```cpp
result[i].real =
    a[i].real * b[i].real +
    a[i].imag * b[i].imag;

result[i].imag =
    a[i].imag * b[i].real -
    a[i].real * b[i].imag;
```

Por ejemplo:

$$
(2+3i)\overline{(4+5i)} = 23+2i
$$

Cada resultado depende únicamente de `A[i]` y `B[i]`. Por esta razón, las iteraciones del kernel son independientes y constituyen la región principal que posteriormente será paralelizada.

Las ventanas se almacenan en vectores lineales utilizando orden por filas:

```text
index = row × width + column
```

## Objetivo del cálculo

Las dos adquisiciones Sentinel-1 SLC contienen información compleja de la señal radar obtenida sobre una misma zona en fechas diferentes. El objetivo del cálculo es comparar las muestras de ambas adquisiciones mediante la multiplicación conjugada.

Para cada posición $i$ se obtiene:

$$
I_i = A_i\overline{B_i}
$$

Al repetir esta operación para todas las muestras de la ventana seleccionada se genera una nueva matriz compleja del mismo tamaño. Cada posición de esta matriz contiene el resultado de combinar la muestra de la primera adquisición con la correspondiente de la segunda.

La importancia de esta operación es que el producto conserva información relacionada con la diferencia de fase entre ambas señales. En un procesamiento interferométrico completo, estas diferencias de fase funcionan para analizar cambios observados entre adquisiciones.

La implementación actual realiza esta operación sobre las mismas posiciones de ambas matrices.

## Datos utilizados

La pareja seleccionada corresponde al punto P09-R2 y utiliza dos adquisiciones Sentinel-1:

| Campo                   | Adquisición A | Adquisición B |
| ----------------------- | ------------- | ------------- |
| Fecha                   | 2025-01-10    | 2025-01-22    |
| Plataforma              | S1A           | S1A           |
| Producto / modo         | SLC / IW      | SLC / IW      |
| Subswath / polarización | IW2 / VV      | IW2 / VV      |
| Dirección orbital       | DESCENDING    | DESCENDING    |
| Órbita absoluta         | 57381         | 57556         |
| Órbita relativa         | 84            | 84            |
| Burst                   | 179298        | 179298        |
| Dimensiones             | 24445 × 1500  | 24445 × 1500  |
| Tipo / bandas           | CInt16 / 1    | CInt16 / 1    |
| Datatake                | 462858        | 464635        |


La implementación actual debe considerarse un kernel computacional sobre datos Sentinel-1 SLC reales. Todavía no constituye un interferograma SAR científicamente validado ni se utiliza para estimar deformaciones del terreno.

## Requisitos

* compilador compatible con C++17, como GCC o Clang;
* GNU Make;
* GDAL con archivos de desarrollo;
* `gdal-config`;
* los dos TIFF Sentinel-1 seleccionados disponibles localmente.

En Ubuntu:

```bash
sudo apt install build-essential libgdal-dev
```

Para comprobar GDAL:

```bash
gdal-config --version
```

El `Makefile` obtiene automáticamente las opciones necesarias mediante:

```bash
gdal-config --cflags
gdal-config --libs
```

## Compilación y ejecución

Desde la raíz del repositorio:

```bash
make
./build/interferogram
```

La configuración predeterminada procesa una ventana de:

```text
512 × 512
```

utilizando las dos adquisiciones P09-R2 configuradas en `config.hpp`.

También puede utilizarse:

```bash
make run
```

Para ejecutar las pruebas:

```bash
make test
```

### Ejemplos

Ventana de 256 × 256:

```bash
./build/interferogram --width 256 --height 256
```

Ventana de 512 × 512:

```bash
./build/interferogram --width 512 --height 512
```

Ventana de 1024 × 1024:

```bash
./build/interferogram --width 1024 --height 1024
```

Ventana desplazada dentro del burst:

```bash
./build/interferogram \
    --x 12000 \
    --y 750 \
    --width 16 \
    --height 16
```


## Parámetros

| Opción       | Descripción                      | Valor predeterminado |
| ------------ | -------------------------------- | -------------------- |
| `--a PATH`   | Ruta al TIFF de la adquisición A | P09-R2, 2025-01-10   |
| `--b PATH`   | Ruta al TIFF de la adquisición B | P09-R2, 2025-01-22   |
| `--width N`  | Ancho de la ventana              | `512`                |
| `--height N` | Alto de la ventana               | `512`                |
| `--x N`      | Columna inicial, base cero       | `0`                  |
| `--y N`      | Fila inicial, base cero          | `0`                  |
| `--help`     | Muestra la ayuda                 | —                    |

Las dos adquisiciones utilizan exactamente la misma ventana.


## Ejemplo de salida

Una ejecución presenta información similar a:

```text
P09-R2
Window: 512 x 512
Offset: 0, 0
Samples: 262144
Mode: sequential
Kernel time: 0.445534 ms
Result sum (real, imag): -7416021, -30561704
Computational kernel; pixelwise coregistration not established.
```

`Result sum` corresponde a la suma de las componentes real e imaginaria de todos los resultados. Se utiliza como resumen de la ejecución y no representa una medida de calidad física del interferograma.

## Opciones de Make

| Comando               | Acción                                        |
| --------------------- | --------------------------------------------- |
| `make`                | Compila `build/interferogram`                 |
| `make run`            | Compila si es necesario y ejecuta el programa |
| `make test`           | Compila y ejecuta todas las pruebas           |
| `make check-gdal`     | Comprueba que `gdal-config` esté disponible   |
| `make clean`          | Elimina los archivos generados en `build/`    |


## Estructura del proyecto

```text
fgonzalez_a2_2026_s2/
├── Makefile
├── data/
│   ├── sentinel1_catalog.csv
│   ├── metadata/
│   │   └── P09-R2/
│   └── raw/
│       └── P09-R2/
│
└── interferogram/
    ├── include/
    │   └── interferogram/
    │       ├── complex.hpp
    │       ├── config.hpp
    │       ├── interferogram.hpp
    │       └── slc_reader.hpp
    │
    ├── src/
    │   ├── main.cpp
    │   ├── config.cpp
    │   ├── interferogram.cpp
    │   └── slc_reader.cpp
    │
    ├── tests/
    │   ├── test_interferogram.cpp
    │   ├── test_slc_reader.cpp
    │   └── test_cli.sh
    │
    └── docs/
        ├── README.md
        └── data_acquisition.md
```

Responsabilidades principales:

* [`main.cpp`](../src/main.cpp): coordina lectura, ejecución, medición y presentación de resultados.
* [`config.cpp`](../src/config.cpp): procesa y valida los argumentos de línea de comandos.
* [`slc_reader.cpp`](../src/slc_reader.cpp): abre los TIFF con GDAL, valida sus propiedades y carga la ventana solicitada.
* [`interferogram.cpp`](../src/interferogram.cpp): contiene la validación de vectores y el kernel secuencial de multiplicación conjugada.
* [`complex.hpp`](../include/interferogram/complex.hpp): define la representación de las muestras complejas.

### Lector SLC

El lector se encarga de abrir y procesar los archivos TIFF de los productos Sentinel-1 SLC utilizando GDAL (Geospatial Data Abstraction Library), una biblioteca de software que permite leer y trabajar con diferentes formatos de datos geoespaciales y ráster, entre ellos TIFF.

Para verificar el funcionamiento del lector, se crea temporalmente un TIFF complejo sintético utilizando GDAL. Esta prueba permite comprobar:

* que las componentes real e imaginaria de las muestras del TIFF se leen correctamente;
* que es posible leer diferentes regiones del TIFF mediante offsets;
* que las ventanas solicitadas se encuentren dentro de los límites del archivo;
* que se rechacen TIFF cuyas bandas no sean de tipo complejo;
* que se detecten TIFF con dimensiones o cantidades de bandas incompatibles.


## Variables que aumentan el paralelismo disponible

El tamaño del problema depende principalmente de las dimensiones de la ventana seleccionada. La cantidad total de muestras procesadas se calcula como:

$$\
N = width \times height\
$$

El kernel secuencial procesa cada una de estas muestras mediante el siguiente ciclo:

```cpp
for (std::size_t i = 0; i < a.size(); ++i) {
    result[i].real =
        a[i].real * b[i].real +
        a[i].imag * b[i].imag;

    result[i].imag =
        a[i].imag * b[i].real -
        a[i].real * b[i].imag;
}
```

En cada iteración se utilizan únicamente las muestras `A[i]` y `B[i]` y el resultado se almacena en `result[i]`. Como el cálculo de una posición no necesita información obtenida por las demás iteraciones, las $N$ operaciones pueden considerarse independientes entre sí.

Por esta razón, `width` y `height` son las variables principales que permiten aumentar el paralelismo disponible. Al incrementar cualquiera de ellas, aumenta $N$ y, con ello, la cantidad de operaciones que pueden distribuirse entre diferentes hilos o unidades de ejecución.

Cada muestra requiere una cantidad constante de trabajo, por lo que el costo total del kernel crece como $O(N)$. De la misma forma, el espacio requerido por los vectores de entrada y salida aumenta linealmente con la cantidad de muestras.

Los parámetros `x` y `y`, en cambio, únicamente determinan la posición inicial de la ventana dentro del SLC. Modificarlos cambia la región que se procesa, pero no la cantidad de trabajo ni el paralelismo disponible mientras `width` y `height` permanezcan iguales.

La principal región que se utilizará para implementar y comparar los diferentes modelos de ejecución es el ciclo de `computeInterferogramSequential()`, ya que sus iteraciones independientes permiten distribuir el procesamiento entre distintas unidades de ejecución.


## Diagramas de los modelos de ejecución

Los siguientes modelos todavía corresponden al diseño preliminar de la etapa paralela.

### Fine-Grained Multithreading

En este modelo, el procesamiento se organiza en unidades pequeñas que se atienden mediante una planificación *round-robin*. Cada tarea tiene asignado un conjunto de muestras y procesa hasta $K$ de ellas cada vez que recibe un turno.

![Diagrama de Fine-Grained Multithreading](interferogram/images/fine_grained.png)


### Coarse-Grained Multithreading

En este modelo, las $N$ muestras se agrupan en $M$ bloques de mayor tamaño y cada bloque se asigna a un hilo diferente. A diferencia de Fine-Grained, cada hilo procesa todas las muestras de su bloque antes de finalizar.

![Diagrama de Coarse-Grained Multithreading](interferogram/images/coarse_grained.png)


### SMT — Simultaneous Multithreading

En este modelo se aprovecharán los hilos lógicos disponibles en cada núcleo físico del procesador. Si el hardware permite ejecutar dos hilos lógicos por núcleo y se dispone de $C$ núcleos físicos, se podrán utilizar hasta $2C$ hilos para distribuir el procesamiento de las muestras.

![Diagrama de Simultaneous Multithreading](interferogram/images/simultaneous.png)


### CMP — Chip Multiprocessing

En este modelo, las $N$ muestras se dividirán en $C$ bloques, donde $C$ corresponde a la cantidad de núcleos físicos utilizados. Cada bloque se asignará a un hilo que se ejecutará en un núcleo físico diferente.

![Diagrama de Chip Multiprocessing](interferogram/images/cmp.png)

