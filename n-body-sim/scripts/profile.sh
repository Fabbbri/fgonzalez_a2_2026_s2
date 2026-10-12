#!/usr/bin/env bash
# Contadores del proceso y sus hilos heredados. Cada repetición conserva la
# salida original de perf; no se mezclan tiempos instrumentados con benchmark.
set -euo pipefail
binary=${1:?Falta el ejecutable}
repetitions=${2:?Faltan las repeticiones}
shift 2
perf_binary=${PERF:-perf}
events=${PERF_EVENTS:-task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,cache-references,cache-misses}
[[ $(uname -s) == Linux ]] || { printf 'El perfilado con perf requiere Linux.\n' >&2; exit 1; }
[[ $repetitions =~ ^[1-9][0-9]*$ ]] || { printf 'Repeticiones debe ser un entero positivo.\n' >&2; exit 1; }
command -v "$perf_binary" >/dev/null || { printf 'No se encontró perf; instale la versión correspondiente a su kernel.\n' >&2; exit 1; }
model_options=0
for arg in "$@"; do
    case "$arg" in
        --model)
            model_options=$((model_options + 1))
            (( model_options <= 1 )) || { printf 'Seleccione modelos con PROFILE_MODELS.\n' >&2; exit 1; } ;;
        --replay|--trajectory|--no-output|--topology|--help)
            printf 'El perfilador controla la captura y requiere una simulación.\n' >&2; exit 1 ;;
    esac
done
mkdir -p output
result_dir=$(mktemp -d "output/profile-$(date +%Y%m%d-%H%M%S)-XXXXXX")
printf 'Perfilado: %s\n' "$result_dir"
# Solo un lote terminado publica status=complete. Los fallos conservan evidencia.
printf 'incomplete\n' > "$result_dir/status.txt"
{
    date -u
    uname -a
    "$perf_binary" --version
    printf 'perf_event_paranoid='; cat /proc/sys/kernel/perf_event_paranoid
    if command -v lscpu >/dev/null; then lscpu; fi
    printf 'commit='; git rev-parse HEAD 2>/dev/null || true
    git status --short 2>/dev/null || true
    sha256sum "$binary"
    printf 'compiler=%s\nflags=%s\n' "${PROFILE_COMPILER:-unknown}" "${PROFILE_FLAGS:-unknown}"
    if [[ -n ${PROFILE_COMPILER:-} ]]; then "$PROFILE_COMPILER" --version; fi
} > "$result_dir/environment.txt"
"$binary" --topology > "$result_dir/topology.txt"
check_topology() {
    "$binary" --topology > "$result_dir/current-topology.txt"
    cmp -s "$result_dir/topology.txt" "$result_dir/current-topology.txt" || {
        printf 'Cambió la topología/afinidad o SMT; lote incompleto.\n' >&2; exit 1;
    }
}
# Verifica permisos y selección de eventos antes de ejecutar el trabajo costoso.
if ! LC_ALL=C "$perf_binary" stat --no-big-num -x ';' -e "$events" \
        -o "$result_dir/preflight.csv" -- true 2> "$result_dir/preflight.log"; then
    cat "$result_dir/preflight.log" >&2
    printf 'perf no pudo abrir los contadores. Revise permisos y perf list; diagnóstico en %s.\n' "$result_dir" >&2
    exit 1
fi
if grep -q '<not supported>' "$result_dir/preflight.csv"; then
    printf 'Hay eventos no soportados; consulte preflight.csv y seleccione PERF_EVENTS con perf list.\n' >&2
    exit 1
fi
printf 'repetitions=%s\nevents=%s\n' "$repetitions" "$events" > "$result_dir/command.txt"
for ((run=1; run<=repetitions; run++)); do
    check_topology
    command=("$perf_binary" stat --no-big-num -x ';' -e "$events"
             -o "$result_dir/perf-$run.csv" -- "$binary" "$@"
             --no-output --trajectory "$result_dir/trajectory.nbody")
    printf '%q ' "${command[@]}" >> "$result_dir/command.txt"
    printf '\n' >> "$result_dir/command.txt"
    if ! LC_ALL=C "${command[@]}" > "$result_dir/metrics-$run.csv" 2> "$result_dir/run-$run.log"; then
        cat "$result_dir/run-$run.log" >&2
        printf 'Falló el perfilado %s; lote incompleto: %s\n' "$run" "$result_dir" >&2
        exit 1
    fi
    check_topology
    if grep -Eq '<not supported>|<not counted>' "$result_dir/perf-$run.csv"; then
        printf 'Contadores no disponibles en perf-%s.csv; lote incompleto.\n' "$run" >&2
        exit 1
    fi
    printf 'Perfilado: %s/%s\n' "$run" "$repetitions"
done
rm "$result_dir/current-topology.txt"
printf 'complete\n' > "$result_dir/status.txt"
printf 'Contadores completos: %s/perf-*.csv\n' "$result_dir"
