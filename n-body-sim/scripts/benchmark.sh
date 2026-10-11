#!/usr/bin/env bash
# Una ejecución por proceso, sin concurrencia ni ventana. Cada ejecución conserva
# el mismo cálculo y captura; se sobrescribe solo la trayectoria de este lote.
set -euo pipefail
binary=${1:?Falta el ejecutable}
repetitions=${2:-200}
python=${3:-python3}
shift 3
if [[ ! $repetitions =~ ^[1-9][0-9]*$ ]] || (( repetitions < 2 )); then
    printf 'Se requieren al menos 2 repeticiones.\n' >&2
    exit 1
fi
for arg in "$@"; do
    case "$arg" in
        --replay|--trajectory|--no-output|--topology)
            printf 'El benchmark controla --trajectory y --no-output; no admite --replay ni --topology.\n' >&2
            exit 1 ;;
    esac
done
"$python" -c 'import sys; assert sys.version_info >= (3, 8), "Se requiere Python 3.8 o posterior"'
mkdir -p output
result_dir=$(mktemp -d "output/benchmark-$(date +%Y%m%d-%H%M%S)-XXXXXX")
printf 'Resultados: %s\n' "$result_dir"
printf 'command:' > "$result_dir/command.txt"
printf ' %q' "$binary" "$@" --no-output --trajectory "$result_dir/trajectory.nbody" >> "$result_dir/command.txt"
printf '\nrepetitions: %s\n' "$repetitions" >> "$result_dir/command.txt"
{
    date -u
    uname -a
    if command -v lscpu >/dev/null; then lscpu; fi
} > "$result_dir/environment.txt"
"$binary" --topology > "$result_dir/topology.txt"
cat "$result_dir/topology.txt" >> "$result_dir/environment.txt"
check_topology() {
    "$binary" --topology > "$result_dir/current-topology.txt"
    if ! cmp -s "$result_dir/topology.txt" "$result_dir/current-topology.txt"; then
        printf 'Cambió la topología/afinidad o el estado SMT; lote interrumpido: %s\n' "$result_dir" >&2
        exit 1
    fi
}
printf 'repetition,model,workers,bodies,steps,dt,seed,quantum,total_seconds\n' > "$result_dir/samples.csv"
for (( run=1; run<=repetitions; run++ )); do
    check_topology
    if ! "$binary" "$@" --no-output --trajectory "$result_dir/trajectory.nbody" \
            > "$result_dir/current.csv" 2> "$result_dir/last-run.log"; then
        cat "$result_dir/last-run.log" >&2
        printf 'Falló la repetición %s; las muestras anteriores se conservan en %s.\n' "$run" "$result_dir" >&2
        exit 1
    fi
    check_topology  # No incorporar una muestra medida durante un cambio detectado.
    # No se cuentan procesos fallidos ni salidas con un esquema inesperado.
    awk -F, -v run="$run" '
        NR==1 {if ($0!="model,workers,bodies,steps,dt,seed,quantum,total_seconds") exit 1}
        NR==2 {if (NF!=8 || $8 !~ /^[0-9.eE+-]+$/ || $8<=0) exit 1; row=$0}
        END {if (NR!=2 || row=="") exit 1; print run "," row}
    ' "$result_dir/current.csv" >> "$result_dir/samples.csv"
    if (( run == 1 || run % 10 == 0 || run == repetitions )); then
        printf 'Repeticiones: %s/%s\n' "$run" "$repetitions"
    fi
done
rm "$result_dir/current.csv"
rm "$result_dir/current-topology.txt"
"$python" "$(dirname "$0")/analyze.py" "$result_dir/samples.csv"
