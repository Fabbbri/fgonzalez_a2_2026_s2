#!/usr/bin/env bash
set -euo pipefail
# Perfilado separado del benchmark: un perf-N.csv por ejecución.
out=${1:?Falta directorio de salida}
mode=${2:?Falta modelo}
threads=${3:?Faltan hilos}
repetitions=${4:-3}
shift 3
if (( $# > 0 )); then shift; fi
[[ ! -e $out ]] || { echo 'El directorio debe ser nuevo' >&2; exit 2; }
[[ $threads =~ ^[1-9][0-9]*$ && $repetitions =~ ^[1-9][0-9]*$ ]] || { echo 'Hilos/repeticiones inválidos' >&2; exit 2; }
[[ $mode == sequential || $mode == fine-grained || $mode == coarse-grained || $mode == smt || $mode == cmp ]] || { echo 'Modelo inválido' >&2; exit 2; }
if [[ $mode == sequential || $mode == fine-grained ]] && (( threads != 1 )); then echo 'Este modelo usa un hilo' >&2; exit 2; fi
perf_bin=${PERF:-perf}
events=${PERF_EVENTS:-task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,cache-references,cache-misses}
command -v "$perf_bin" >/dev/null || { echo 'No se encontró perf' >&2; exit 2; }
cpus=${PERF_CPUS:-$(sed -n 's/^Cpus_allowed_list:[[:space:]]*//p' /proc/self/status)}
[[ $cpus =~ ^[0-9]+([,-][0-9]+)*$ ]] || { echo 'No se pudo determinar una lista válida de CPUs lógicas' >&2; exit 2; }
command -v taskset >/dev/null || { echo 'Falta taskset' >&2; exit 2; }
taskset -c "$cpus" true || { echo 'PERF_CPUS incluye CPUs no permitidas o desconectadas' >&2; exit 2; }
stat_args=(-a -A -C "$cpus")
affinity=(taskset -c "$cpus")
program_args=("$@")
if (( ${#program_args[@]} == 0 )); then
    program_args=(--a "${SLC_A:-data/coregistered/P09-R2-split/A.tif}"
        --b "${SLC_B:-data/coregistered/P09-R2-split/B.tif}"
        --x 0 --y 0 --width 24445 --height 1500 --allow-nodata --quantum 2)
fi
for arg in "${program_args[@]}"; do
    case $arg in
        --mode|--threads|--help) echo 'El perfilador controla el modelo y los hilos' >&2; exit 2 ;;
    esac
done
mkdir -p "$out"
printf 'incomplete\n' > "$out/status.txt"
{
    date -u
    uname -a
    "$perf_bin" --version
    lscpu
    printf 'perf_event_paranoid='; cat /proc/sys/kernel/perf_event_paranoid
    printf 'perf_cpus=%s\n' "$cpus"
    sha256sum ./build/interferogram
} > "$out/environment.txt"
topology() {
    sed -n '/Cpus_allowed_list/p' /proc/self/status
    cat /sys/devices/system/cpu/online /sys/devices/system/cpu/smt/active
    lscpu -p=CPU,CORE,SOCKET,ONLINE
}
topology > "$out/topology.txt"
check_topology() {
    topology > "$out/current-topology.txt"
    cmp -s "$out/topology.txt" "$out/current-topology.txt" || { echo 'Cambió topología/SMT; lote incompleto' >&2; exit 1; }
}
if ! LC_ALL=C "$perf_bin" stat --no-big-num -x ';' "${stat_args[@]}" -e "$events" \
    -o "$out/preflight.csv" -- "${affinity[@]}" true 2> "$out/preflight.log"; then
    cat "$out/preflight.log" >&2
    echo "perf no pudo abrir contadores: $out" >&2
    exit 1
fi
if [[ ! -s $out/preflight.csv ]] || grep -Eq '<not supported>|<not counted>' "$out/preflight.csv"; then echo 'Evento no disponible' >&2; exit 1; fi
thread_args=()
if [[ $mode != sequential && $mode != fine-grained ]]; then thread_args=(--threads "$threads"); fi
printf 'modelo=%s\nhilos=%s\nrepeticiones=%s\neventos=%s\ncpus=%s\n' "$mode" "$threads" "$repetitions" "$events" "$cpus" > "$out/command.txt"
for ((run=1; run<=repetitions; run++)); do
    check_topology
    command=("$perf_bin" stat --no-big-num -x ';' "${stat_args[@]}" -e "$events"
        -o "$out/perf-$run.csv" -- "${affinity[@]}" ./build/interferogram
        --mode "$mode" "${thread_args[@]}" "${program_args[@]}")
    printf '%q ' "${command[@]}" >> "$out/command.txt"; printf '\n' >> "$out/command.txt"
    if ! LC_ALL=C "${command[@]}" > "$out/program-$run.log" 2> "$out/run-$run.log"; then
        cat "$out/run-$run.log" >&2; echo "Falló perfilado $run: $out" >&2; exit 1
    fi
    check_topology
    if [[ ! -s $out/perf-$run.csv ]] || grep -Eq '<not supported>|<not counted>' "$out/perf-$run.csv"; then echo 'Contadores incompletos' >&2; exit 1; fi
    if ! grep -Fxq "Mode: $mode" "$out/program-$run.log" ||
       ! grep -q '^Kernel time: ' "$out/program-$run.log" ||
       ! grep -q '^Result sum (real, imag): ' "$out/program-$run.log"; then
        echo 'Salida del interferograma incompleta; lote incompleto' >&2; exit 1
    fi
    if [[ $mode != sequential && $mode != fine-grained ]] &&
       ! grep -Fxq "Workers: $threads" "$out/program-$run.log"; then
        echo 'Cantidad de hilos incorrecta; lote incompleto' >&2; exit 1
    fi
    printf 'Perfilado %s:%s %s/%s\n' "$mode" "$threads" "$run" "$repetitions"
done
rm "$out/current-topology.txt"
printf 'complete\n' > "$out/status.txt"
