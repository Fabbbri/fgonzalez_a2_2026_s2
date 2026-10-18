#!/usr/bin/env bash
# Adaptador Bash de una configuración hacia el coordinador Python.
# No mide tiempos propios ni implementa el paralelismo de fuerzas.
# Interfaz compatible para un modelo; Make ejecuta la campaña completa en Python.
# Abortar por errores, variables no definidas y fallos de tubería.
set -euo pipefail
# Primer argumento obligatorio: ejecutable que contiene la configuración y los modelos.
binary=${1:?Falta el ejecutable}
# Cantidad predeterminada de observaciones si el llamante no la especifica.
repetitions=${2:-200}
# Intérprete seleccionado por Make o por el llamante.
python=${3:-python3}
# Retira parámetros del adaptador; los restantes se transmiten como opciones de campaña.
shift 3
# Reemplaza el proceso shell por Python conservando argumentos como unidades independientes.
exec "$python" "$(dirname "$0")/benchmark.py" "$binary" --repetitions "$repetitions" "$@"
