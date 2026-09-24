#!/usr/bin/env bash
set +x
set -euo pipefail
if [[ -z ${AWS_ACCESS_KEY_ID:-} || -z ${AWS_SECRET_ACCESS_KEY:-} ]]; then
    echo 'Carga AWS_ACCESS_KEY_ID y AWS_SECRET_ACCESS_KEY en esta terminal.' >&2
    exit 1
fi
project_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
IFS=, read -r code easting northing latitude longitude rest < <(
    awk -F, '$1 == "P09-R2" {print; exit}' "$project_dir/data/critical_points.csv"
)
[[ $code == P09-R2 && -n $latitude && -n $longitude ]]
mkdir -p "$project_dir/data/raw/P09-R2"
# Upstream hardcodes /home/ubuntu when invoking its child extractor.
docker run --rm --user "$(id -u):$(id -g)" \
    -v "$project_dir/data/raw/P09-R2:/home/ubuntu" \
    -e AWS_ACCESS_KEY_ID -e AWS_SECRET_ACCESS_KEY \
    cdse_utilities sentinel1_burst_extractor_spatiotemporal.sh \
    -o /home/ubuntu -s "${1:-2025-01-10}" -e "${2:-2025-01-10}" \
    -x "$longitude" -y "$latitude" -p vv
# Upstream can return success even if a child download failed.
if ! find "$project_dir/data/raw/P09-R2" -type f -name '*.tiff' -size +0c -print -quit | grep -q .; then
    echo 'No hay TIFF no vacíos. Revisa los errores antes de ampliar el intervalo.' >&2
    exit 1
fi
echo 'Archivos locales (pendientes de validación con GDAL y XML):'
find "$project_dir/data/raw/P09-R2" -type f -printf '%P\t%s bytes\n'
du -sh "$project_dir/data/raw/P09-R2"
