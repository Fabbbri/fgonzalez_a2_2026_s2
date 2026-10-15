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
bash scripts/acquire_p09.sh 2025-01-10 2025-01-22
```

El [script del proyecto](../../scripts/acquire_p09.sh) obtiene las coordenadas de P09-R2 desde `data/critical_points.csv` y ejecuta `sentinel1_burst_extractor_spatiotemporal.sh` con polarización VV y el intervalo solicitado.

El [extractor oficial](https://github.com/eu-cdse/utilities/blob/main/sentinel1_burst_extractor_spatiotemporal.sh) consulta la API OData en `https://catalogue.dataspace.copernicus.eu/odata/v1/Bursts`, filtrando por punto, fechas y polarización. Luego el [extractor individual](https://github.com/eu-cdse/utilities/blob/main/sentinel1_burst_extractor.sh) consulta `Products` para obtener la ruta S3 y accede a los datos mediante `https://eodata.dataspace.copernicus.eu` con las claves exportadas.

Los resultados quedan en `sentinel1-SLC/data/raw/P09-R2/`, en carpetas `.SAFE` con TIFF y metadatos. El intervalo puede devolver varios bursts; la pareja de datos utilizada por el programa es IW2/VV, burst 179298, de 2025-01-10 y 2025-01-22.

Al terminar, retirar las claves de la sesión:

```bash
unset AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY
```
