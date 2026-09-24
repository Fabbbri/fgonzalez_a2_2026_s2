# Adquisición Sentinel-1 SLC: P09-R2

Se utiliza directamente la [utilidad oficial de CDSE](https://github.com/eu-cdse/utilities).
No se procesa todavía el interferograma.

## Punto

`data/critical_points.csv` conserva latitud `9.739180718` y longitud
`-83.953806951`, obtenidas de CRTM-05 (EPSG:5367), E=505068, N=1076899.
La transformación está documentada en
`data/metadata/P09-R2/coordinates.json`; no se repite al descargar.
`route_validation.json` registra una distancia aproximada de 10,5 m a la
Carretera Interamericana Sur (ref. 2), compatible con la ubicación indicada.

## Imagen oficial

Si la imagen no existe:

```bash
docker build --no-cache 'https://github.com/eu-cdse/utilities.git#main' -t cdse_utilities
```

## Ejecutar

En la misma terminal donde se ejecutará la descarga:

```bash
set +x
read -rsp "CDSE Access Key ID: " AWS_ACCESS_KEY_ID; echo
read -rsp "CDSE Secret Access Key: " AWS_SECRET_ACCESS_KEY; echo
export AWS_ACCESS_KEY_ID AWS_SECRET_ACCESS_KEY
bash scripts/acquire_p09.sh
```

La prueba inicial solicita VV para el 10 de enero de 2025 (UTC), día con una
adquisición registrada en los metadatos locales. El extractor oficial resuelve los bursts y subswaths. 

```bash
bash scripts/acquire_p09.sh 2025-01-09 2025-01-11
```

Los resultados se guardan en `data/raw/P09-R2/`. 

## Verificación tras la descarga

El script enumera archivos y tamaño local. La existencia de TIFF no demuestra
que la descarga esté completa, antes de reportar bursts válidos hay que abrir
los TIFF con GDAL, comprobar dimensiones y tipo complejo y contrastar las
anotaciones XML (Sentinel-1, SLC, VV, modo, fechas, un burst por producto y
cobertura del punto). Los JSON del catálogo existentes son metadatos, no datos SLC descargados. El informe final debe contar solo los bursts validados.
