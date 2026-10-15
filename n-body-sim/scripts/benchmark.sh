#!/usr/bin/env bash
# Interfaz compatible para un modelo; Make ejecuta la campaña completa en Python.
set -euo pipefail
binary=${1:?Falta el ejecutable}
repetitions=${2:-200}
python=${3:-python3}
shift 3
exec "$python" "$(dirname "$0")/benchmark.py" "$binary" --repetitions "$repetitions" "$@"
