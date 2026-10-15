#!/usr/bin/env bash
# Una campaña usa la misma carga y partición para todos los modelos.
set -euo pipefail
make_command=$1
binary=$2
python=$3
repetitions=$4
thread_counts=$5
smt_counts=$6
states=$7
destination=$8
perf_command=$9
profile_repetitions=${10}
profile_scope=${11}
profile_events=${12}
baseline_repetitions=${13}
count_mode=${14}
profile_logs=${15}
shift 15
workload=("$@")

if [[ ! $repetitions =~ ^[0-9]+$ ]] || (( repetitions < 2 )); then
    echo 'REPETITIONS debe ser un entero >= 2' >&2
    exit 1
fi
if [[ ! $baseline_repetitions =~ ^[0-9]+$ ]] || (( baseline_repetitions < 2 )); then
    echo 'BASELINE_REPETITIONS debe ser un entero >= 2' >&2
    exit 1
fi
if [[ ! $profile_repetitions =~ ^[0-9]+$ ]] || (( profile_repetitions < 1 )); then
    echo 'PROFILE_REPETITIONS debe ser un entero >= 1' >&2
    exit 1
fi
case $profile_scope in
    process|cpu) ;;
    *) echo 'PROFILE_SCOPE debe ser process o cpu' >&2; exit 1 ;;
esac
case $count_mode in
    observed|scaled) ;;
    *) echo 'PROFILE_COUNT_MODE debe ser observed o scaled' >&2; exit 1 ;;
esac
case $profile_logs in
    0|1) ;;
    *) echo 'PROFILE_LOGS debe ser 0 o 1' >&2; exit 1 ;;
esac
case $states in
    on,off|off,on|on|off|keep) ;;
    *) echo 'SMT_STATES debe ser on,off, off,on, on, off o keep' >&2; exit 1 ;;
esac
# Evitar que ARGS cambie el modelo, la referencia o los archivos de la campaña.
for option in "${workload[@]}"; do
    case $option in
        --model|--threads|--thread-counts|--repetitions|--metrics-output|--baseline-csv|--output|--benchmark|--cpu-info|--help)
            echo "Usa las variables de run-all, no $option dentro de ARGS" >&2
            exit 1 ;;
    esac
done
"$python" -c 'import matplotlib' || {
    echo 'Instala Matplotlib: sudo apt install python3-matplotlib' >&2
    exit 1
}
# Fallar antes de una campaña larga si perf no tiene permisos o eventos válidos.
read -r -a perf_parts <<< "$perf_command"
if (( ${#perf_parts[@]} == 0 )); then
    echo 'PERF no puede estar vacío' >&2
    exit 1
fi
probe_flags=(stat --no-big-num -x ';' -e "$profile_events")
if [[ $profile_scope == cpu ]]; then
    probe_flags=(stat --no-big-num -x ';' -e "${profile_events//task-clock/cpu-clock}" -a -A)
fi
if [[ $count_mode == observed ]]; then probe_flags+=(--no-scale); fi
if ! "${perf_parts[@]}" "${probe_flags[@]}" -- true; then
    echo 'No se pudo usar perf. Revisa los eventos y permisos; prueba make run-all PERF="sudo perf".' >&2
    exit 1
fi

original_state=
restore_needed=0
restore() {
    status=$?
    trap - EXIT
    if (( restore_needed )); then
        echo "Restaurando SMT=$original_state..."
        if ! "$make_command" --no-print-directory smt "SMT=$original_state"; then
            echo "No se pudo restaurar SMT. Ejecuta: make smt SMT=$original_state" >&2
            status=1
        fi
    fi
    exit "$status"
}
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

if [[ $states != keep ]]; then
    original_state=$(cat /sys/devices/system/cpu/smt/control)
    case $original_state in
        on|off) ;;
        *) echo "No se puede alternar SMT: $original_state. Usa SMT_STATES=keep." >&2; exit 1 ;;
    esac
    # Pedir credenciales antes de iniciar mediciones. El programa no usa sudo.
    sudo -v
    restore_needed=1
    "$make_command" --no-print-directory smt SMT=on
fi

topology=$("$binary" --cpu-info)
printf '%s\n' "$topology"
cores=$(awk -F'[ =]' '/^nucleos_fisicos_disponibles=/{print $2; exit}' <<< "$topology")
logical=$(awk -F'[ =]' '/^nucleos_fisicos_disponibles=/{print $4; exit}' <<< "$topology")
if [[ ! $cores =~ ^[1-9][0-9]*$ || ! $logical =~ ^[1-9][0-9]*$ ]]; then
    echo 'No se pudo detectar la topología de CPU' >&2
    exit 1
fi
if [[ $thread_counts == auto ]]; then
    counts=()
    for p in 1 2 4 8; do
        if (( p <= cores )); then counts+=("$p"); fi
    done
    if [[ ${counts[-1]} != "$cores" ]]; then counts+=("$cores"); fi
    thread_counts=$(IFS=,; echo "${counts[*]}")
fi
if [[ $smt_counts == auto ]]; then
    smt_counts=$cores
    if (( logical > cores )); then smt_counts+=",$logical"; else smt_counts+=",$((cores * 2))"; fi
fi

if [[ -z $destination ]]; then
    destination=resultsrt
fi
# Reutilizar la carpeta: C++ trunca los CSV y Matplotlib reemplaza sus PNG.
# No borrar capturas de perf ni otros archivos ajenos a estos benchmarks.
mkdir -p "$destination"
printf 'repeticiones=%s\nhilos=%s\nhilos_smt=%s\nestados_smt=%s\n' \
    "$repetitions" "$thread_counts" "$smt_counts" "$states" > "$destination/benchmark.txt"
printf 'carga=' >> "$destination/benchmark.txt"
printf '%q ' "${workload[@]}" >> "$destination/benchmark.txt"
printf '\n%s\n' "$topology" >> "$destination/benchmark.txt"
printf 'repeticiones_perf=%s\nalcance_perf=%s\neventos_perf=%s\n' \
    "$profile_repetitions" "$profile_scope" "$profile_events" >> "$destination/benchmark.txt"
printf 'referencia=sequential.csv\nrepeticiones_sequential=%s\n' "$baseline_repetitions" >> "$destination/benchmark.txt"
printf 'modo_conteo_perf=%s\n' "$count_mode" >> "$destination/benchmark.txt"
printf 'logs_perf=%s\n' "$profile_logs" >> "$destination/benchmark.txt"

echo "=== Referencia sequential ÚNICA: $baseline_repetitions repeticiones ==="
"$binary" --benchmark "${workload[@]}" --model sequential \
    --repetitions "$baseline_repetitions" --metrics-output "$destination/sequential.csv"
"$python" scripts/plot_metrics.py "$destination/sequential.csv" --output-dir "$destination/plots/sequential"
generated_csv=("$destination/sequential.csv")
measure() {
    local name=$1
    shift
    echo "=== Benchmark: $name ($repetitions repeticiones por configuración) ==="
    "$binary" --benchmark "${workload[@]}" "$@" \
        --baseline-csv "$destination/sequential.csv" \
        --repetitions "$repetitions" --metrics-output "$destination/$name.csv"
    generated_csv+=("$destination/$name.csv")
    "$python" scripts/plot_metrics.py "$destination/$name.csv" \
        --output-dir "$destination/plots/$name"
}

generated_profiles=()
profile_model() {
    local label=$1
    shift
    echo "=== Perf: $label ($profile_repetitions repeticiones) ==="
    local reference=()
    if [[ $label != sequential ]]; then
        reference=(--baseline-capture "$destination/perf/sequential")
    fi
    "$python" scripts/profile_perf.py --perf "$perf_command" \
        --count-mode "$count_mode" \
        --logs "$profile_logs" \
        --scope "$profile_scope" --events "$profile_events" \
        --repetitions "$profile_repetitions" --fixed-output \
        "${reference[@]}" \
        --output-dir "$destination/perf/$label" "$binary" "${workload[@]}" "$@"
    generated_profiles+=("$destination/perf/$label")
}

profile_workers() {
    local model=$1
    local label=$2
    local list=$3
    local workers p
    IFS=, read -r -a workers <<< "$list"
    for p in "${workers[@]}"; do
        profile_model "$label-p$p" --model "$model" --threads "$p"
    done
}

# Perf necesita su propia referencia instrumentada, también medida UNA sola vez.
profile_model sequential --model sequential
# Todos los modelos reutilizan los mismos tiempos; no ejecutan sequential.
measure fine --model fine
measure coarse --model coarse --thread-counts "$thread_counts"
measure cmp --model cmp --thread-counts "$thread_counts"
profile_model fine --model fine
profile_workers coarse coarse "$thread_counts"
profile_workers cmp cmp "$thread_counts"
IFS=, read -r -a state_list <<< "$states"
for state in "${state_list[@]}"; do
    "$make_command" --no-print-directory smt "SMT=$state"
    measure "smt-$state" --model smt --thread-counts "$smt_counts"
    profile_workers smt "smt-$state" "$smt_counts"
done

# Restaurar antes de graficar: la visualización no requiere SMT desactivado.
if (( restore_needed )); then
    "$make_command" --no-print-directory smt "SMT=$original_state"
    restore_needed=0
fi
# Solo incluir los datos de esta ejecución, sin mezclar CSV antiguos o manuales.
"$python" scripts/plot_metrics.py "${generated_csv[@]}" --output-dir "$destination/plots/comparison"
"$python" scripts/plot_profile_comparison.py "$destination/perf" --captures "${generated_profiles[@]}"
echo "Resultados actualizados: $destination"
