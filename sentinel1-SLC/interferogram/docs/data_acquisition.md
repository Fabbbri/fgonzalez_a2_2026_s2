# Adquisición de datos Sentinel-1 SLC

El proyecto utiliza [CDSE Utilities](https://github.com/eu-cdse/utilities#copernicus-data-space-ecosystem-cdse-utilities) para extraer bursts Sentinel-1 SLC sobre P09-R2. Se requiere Linux, Bash, Docker disponible y una cuenta de Copernicus Data Space Ecosystem (CDSE).

## 1. Obtener las claves de acceso

1. Crear y activar una cuenta en [Copernicus Data Space](https://dataspace.copernicus.eu/).
2. Abrir el [administrador de claves S3](https://eodata-s3keysmanager.dataspace.copernicus.eu/) e iniciar sesión con esa cuenta.
3. Seleccionar **Add Credentials**, indicar la fecha de vencimiento y pulsar **Confirm**.
4. Guardar **Access Key** y **Secret Key**. La clave secreta se muestra una sola vez.

Estas credenciales permiten acceder a la [API S3 de CDSE](https://documentation.dataspace.copernicus.eu/APIs/S3.html). El script las recibe como `AWS_ACCESS_KEY_ID` y `AWS_SECRET_ACCESS_KEY`; corresponden a las claves generadas en CDSE.

## 2. Preparar la herramienta

Construir la imagen Docker indicada por CDSE Utilities:

```bash
docker build --no-cache https://github.com/eu-cdse/utilities.git#main -t cdse_utilities
```

En una terminal Bash, cargar las claves sin escribirlas directamente en el historial:

```bash
set +x
read -r -s -p "Access Key de CDSE: " AWS_ACCESS_KEY_ID; echo
read -r -s -p "Secret Key de CDSE: " AWS_SECRET_ACCESS_KEY; echo
export AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY
```

No guardar las claves en el código ni subirlas al repositorio.

## 3. Descargar los datos

Desde la raíz del repositorio, en la misma terminal:

```bash
cd sentinel1-SLC
bash scripts/acquire_p09.sh 2023-06-14 2023-06-14
bash scripts/acquire_p09.sh 2024-06-08 2024-06-08
bash scripts/acquire_p09.sh 2024-06-20 2024-06-20
```

El [script del proyecto](../../scripts/acquire_p09.sh) obtiene las coordenadas de P09-R2 desde `data/critical_points.csv` y ejecuta `sentinel1_burst_extractor_spatiotemporal.sh` con polarización VV y el intervalo solicitado.

El [extractor oficial](https://github.com/eu-cdse/utilities/blob/main/sentinel1_burst_extractor_spatiotemporal.sh) consulta la API OData en `https://catalogue.dataspace.copernicus.eu/odata/v1/Bursts`, filtrando por punto, fechas y polarización. Luego el [extractor individual](https://github.com/eu-cdse/utilities/blob/main/sentinel1_burst_extractor.sh) consulta `Products` para obtener la ruta S3 y accede a los datos mediante `https://eodata.dataspace.copernicus.eu` con las claves exportadas.

Los resultados quedan en `sentinel1-SLC/data/raw/P09-R2/`, en carpetas `.SAFE` con TIFF y metadatos. Cada fecha puede devolver varios bursts; el corregistro utiliza los bursts IW2/VV de 2023-06-14 y 2024-06-08.

Al terminar, retirar las claves de la sesión:

```bash
unset AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY
```

## 4. Instalar SNAP

El corregistro se realiza con `gpt`, la herramienta de línea de comandos de ESA SNAP.

1. Descargar el instalador desde la [página de descargas de SNAP](https://step.esa.int/main/download/snap-download/). Para Linux:

   ```bash
   wget https://download.esa.int/step/snap/14.0/installers/esa-snap_sentinel_linux-14.0.0.sh
   bash esa-snap_sentinel_linux-14.0.0.sh
   ```

2. Abrir SNAP y entrar a **Tools -> Plugins** (Herramientas -> Complementos).
3. En **Available Plugins**, buscar **Sentinel-1 Toolbox**.
4. Marcarlo, pulsar **Install** y esperar a que termine. Reiniciar SNAP si lo solicita.

Instalar Python con NumPy y GDAL, utilizados por el script de corregistro:

```bash
sudo apt install python3 python3-numpy python3-gdal gdal-bin
python3 -c "import numpy; from osgeo import gdal; print(gdal.VersionInfo())"
```

## 5. Corregistrar las imágenes

Desde `sentinel1-SLC/`:

```bash
python3 scripts/coregister.py \
  --a "data/raw/P09-R2/S1A_SLC_20230614T234847_195513_IW2_VV_386086.SAFE" \
  --b "data/raw/P09-R2/S1A_SLC_20240608T234849_195513_IW2_VV_432350.SAFE" \
  --out data/coregistered/P09-R2-2023-2024 \
  --swath IW2 --pol VV \
  --already-split \
  --gpt "$HOME/esa-snap/bin/gpt"
```

`--gpt` debe apuntar al ejecutable `gpt` de la instalación local de SNAP.

La salida esperada es similar a:

```text
Grafo: .../sentinel1-SLC/data/coregistered/P09-R2-2023-2024/graph.xml
Listo: .../P09-R2-2023-2024/A.tif y .../P09-R2-2023-2024/B.tif. Selecciona una ventana sin NoData.
```

`A.tif` y `B.tif` son los TIFF complejos corregistrados que utiliza el programa.

## 6. Compilar

```bash
sudo apt install build-essential libgdal-dev python3-pil
make check-gdal
make
```

Con esto quedan disponibles los datos y el ejecutable `build/interferogram`.

## 7. Ejecutar los modelos

Los objetivos de Make utilizan por defecto `data/coregistered/P09-R2-2023-2024/A.tif` y `B.tif`:

```bash
# Secuencial
make run-sequential REPETITIONS=200

# Fine-Grained
make run-fine-grained REPETITIONS=200

# Coarse-Grained
make run-coarse-grained REPETITIONS=200 THREADS=4

# SMT por sobresuscripción
make run-smt REPETITIONS=200 THREADS=12

# SMT con el SMT del equipo habilitado
make run-smt REPETITIONS=200 THREADS=16

# CMP: trabajadores en núcleos físicos distintos
make run-cmp REPETITIONS=200 THREADS=8
```

Campaña completa:

```bash
# Comprobar el flujo con dos repeticiones
make benchmark REPETITIONS=2

# Todos los casos
make benchmark REPETITIONS=200
```

Generar y abrir las imágenes del resultado:

```bash
make images IMAGE_MODEL=cmp ARGS="--threads 8" IMAGE_OUTPUT=results/imagen-cmp
make view-images IMAGE_OUTPUT=results/imagen-cmp
```
