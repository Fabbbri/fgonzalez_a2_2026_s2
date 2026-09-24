#!/usr/bin/env bash
# Descarga bursts VV sobre P09-R2 entre START_DATE y END_DATE (YYYY-MM-DD).
# Requiere Docker, la imagen cdse_utilities y claves S3 de CDSE en el entorno.
# Guarda SAFE en data/raw/P09-R2; rechaza intervalos con archivos ya existentes.
# Desactivar la traza evita mostrar credenciales durante la ejecución.
set +x
set -euo pipefail
if [[ $# -ne 2 ]]; then
    echo 'Uso: bash scripts/acquire_p09.sh START_DATE END_DATE (YYYY-MM-DD)' >&2
    exit 2
fi
start_date=$1
end_date=$2
# Comprobar formato y fecha real antes de comparar el intervalo.
for value in "$start_date" "$end_date"; do
    if [[ ! $value =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]] ||
       [[ $(date -u -d "$value" +%F 2>/dev/null) != "$value" ]]; then
        echo 'Fecha inválida.' >&2
        exit 2
    fi
done
[[ $start_date > $end_date ]] && { echo 'Intervalo invertido.' >&2; exit 2; }
# Resolver las rutas desde la ubicación del script, no desde la terminal del usuario.
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
# Extraer del catálogo local las coordenadas geográficas del punto P09-R2.
IFS=, read -r code easting northing latitude longitude rest < <(
    awk -F, '$1 == "P09-R2" {print; exit}' "$project_dir/data/critical_points.csv"
)
[[ $code == P09-R2 && -n $latitude && -n $longitude ]]
mkdir -p "$project_dir/data/raw/P09-R2"
# Evitar sobrescribir extracciones SAFE, incluso si están incompletas.
# Un patrón sin coincidencias produce una lista vacía, no un nombre literal.
shopt -s nullglob
for safe in "$project_dir"/data/raw/P09-R2/S1*_SLC_*.SAFE; do
    name=${safe##*/}
    stamp=${name:8:8}
    safe_date="${stamp:0:4}-${stamp:4:2}-${stamp:6:2}"
    if [[ ! $safe_date < $start_date && ! $safe_date > $end_date ]]; then
        echo "Ya existe un SAFE en el intervalo: $name. No se vuelve a descargar." >&2
        exit 1
    fi
done
if [[ -z ${AWS_ACCESS_KEY_ID:-} || -z ${AWS_SECRET_ACCESS_KEY:-} ]]; then
    echo 'Carga AWS_ACCESS_KEY_ID y AWS_SECRET_ACCESS_KEY en esta terminal.' >&2
    exit 1
fi
# El extractor invoca su proceso hijo usando /home/ubuntu como salida.
# Montar allí los datos locales y pasar las claves mediante variables de entorno.
docker run --rm --user "$(id -u):$(id -g)" \
    -v "$project_dir/data/raw/P09-R2:/home/ubuntu" \
    -e AWS_ACCESS_KEY_ID -e AWS_SECRET_ACCESS_KEY \
    cdse_utilities sentinel1_burst_extractor_spatiotemporal.sh \
    -o /home/ubuntu -s "$start_date" -e "$end_date" \
    -x "$longitude" -y "$latitude" -p vv
# El extractor puede terminar con éxito aunque falle una descarga.
# Exigir al menos un TIFF no vacío en el intervalo; no valida aún su contenido.
found=0
for safe in "$project_dir"/data/raw/P09-R2/S1*_SLC_*.SAFE; do
    name=${safe##*/}
    stamp=${name:8:8}
    safe_date="${stamp:0:4}-${stamp:4:2}-${stamp:6:2}"
    if [[ ! $safe_date < $start_date && ! $safe_date > $end_date ]]; then
        for tiff in "$safe"/measurement/*.tiff; do
            [[ -s $tiff ]] && found=1
        done
    fi
done
if [[ $found == 0 ]]; then
    echo 'No hay TIFF nuevos en el intervalo solicitado; revisa los errores de CDSE.' >&2
    exit 1
fi
echo 'Archivos locales (pendientes de validación con GDAL y XML):'
find "$project_dir/data/raw/P09-R2" -type f -printf '%P\t%s bytes\n'
du -sh "$project_dir/data/raw/P09-R2"
