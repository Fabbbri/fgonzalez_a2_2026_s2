#!/usr/bin/env bash
# Orquestación secuencial de una campaña con carga fija y referencia reutilizada.
# Los parámetros posicionales proceden del Makefile. Bash coordina procesos,
# pero el paralelismo de tiles pertenece exclusivamente al ejecutable C++.
# Las variables de estado SMT describen configuración global del sistema.
# Una campaña usa la misma carga y partición para todos los modelos.
# Modo estricto: aborta ante fallos, variables no definidas y errores en tuberías.
set -euo pipefail
# Argumentos 1..3: comando Make, ejecutable C++ e intérprete de análisis.
make_command=$1
binary=$2
python=$3
# Argumentos 4..8: repeticiones, barridos de P, estados SMT y carpeta de resultados.
repetitions=$4
thread_counts=$5
smt_counts=$6
states=$7
destination=$8
# Argumentos 9..15: comando perf, repeticiones, alcance, eventos, referencia, modo y logs.
perf_command=$9
profile_repetitions=${10}
profile_scope=${11}
profile_events=${12}
baseline_repetitions=${13}
count_mode=${14}
profile_logs=${15}
# Retira parámetros del coordinador; los restantes son opciones de carga del render.
shift 15
# Array de argumentos: preserva cada palabra y sus espacios sin una segunda evaluación.
workload=("$@")

# Valida enteros y rangos antes de crear campañas o cambiar estados de hardware.
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
# Dominio permitido: todo el proceso o CPUs seleccionadas.
case $profile_scope in
    process|cpu) ;;
    *) echo 'PROFILE_SCOPE debe ser process o cpu' >&2; exit 1 ;;
esac
# Observed conserva conteos; scaled permite extrapolación de perf.
case $count_mode in
    observed|scaled) ;;
    *) echo 'PROFILE_COUNT_MODE debe ser observed o scaled' >&2; exit 1 ;;
esac
case $profile_logs in
    0|1) ;;
    *) echo 'PROFILE_LOGS debe ser 0 o 1' >&2; exit 1 ;;
esac
# Lista cerrada de secuencias admitidas para controlar y restaurar SMT.
case $states in
    on,off|off,on|on|off|keep) ;;
    *) echo 'SMT_STATES debe ser on,off, off,on, on, off o keep' >&2; exit 1 ;;
esac
# Evitar que ARGS cambie el modelo, la referencia o los archivos de la campaña.
# La carga no puede sustituir modelo, P, referencia o rutas administradas por el script.
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
# Convierte PERF en un array de palabras, por ejemplo [sudo,perf]; no usa eval.
read -r -a perf_parts <<< "$perf_command"
if (( ${#perf_parts[@]} == 0 )); then
    echo 'PERF no puede estar vacío' >&2
    exit 1
fi
# Sondeo de disponibilidad de eventos sin ejecutar una escena.
probe_flags=(stat --no-big-num -x ';' -e "$profile_events")
if [[ $profile_scope == cpu ]]; then
    probe_flags=(stat --no-big-num -x ';' -e "${profile_events//task-clock/cpu-clock}" -a -A)
fi
if [[ $count_mode == observed ]]; then probe_flags+=(--no-scale); fi
if ! "${perf_parts[@]}" "${probe_flags[@]}" -- true; then
    echo 'No se pudo usar perf. Revisa los eventos y permisos; prueba make run-all PERF="sudo perf".' >&2
    exit 1
fi

# Estado inicial y bandera de restauración; se usan desde el manejador de salida.
original_state=
restore_needed=0
# Finalizador de campaña: conserva el código de salida y restaura SMT si se modificó.
restore() {
    # Captura el resultado previo antes de ejecutar comandos del finalizador.
    status=$?
    # Desactiva la trampa para no llamarla recursivamente al terminar restore.
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
# Registra finalización normal y por error; señales INT/TERM producen códigos convencionales.
trap restore EXIT
trap 'exit 130' INT
trap 'exit 143' TERM

# Solo las campañas que alternan hardware necesitan guardar estado y credenciales.
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

# La topología procede de cpu.cpp y refleja las CPUs permitidas al ejecutable.
topology=$("$binary" --cpu-info)
printf '%s\n' "$topology"
# C y L se extraen de la línea estructurada; no se supone L=2*C.
cores=$(awk -F'[ =]' '/^nucleos_fisicos_disponibles=/{print $2; exit}' <<< "$topology")
logical=$(awk -F'[ =]' '/^nucleos_fisicos_disponibles=/{print $4; exit}' <<< "$topology")
if [[ ! $cores =~ ^[1-9][0-9]*$ || ! $logical =~ ^[1-9][0-9]*$ ]]; then
    echo 'No se pudo detectar la topología de CPU' >&2
    exit 1
fi
# Barrido automático CMP/coarse: potencias de dos válidas y C como punto final.
if [[ $thread_counts == auto ]]; then
    counts=()
    for p in 1 2 4 8; do
        if (( p <= cores )); then counts+=("$p"); fi
    done
    if [[ ${counts[-1]} != "$cores" ]]; then counts+=("$cores"); fi
    thread_counts=$(IFS=,; echo "${counts[*]}")
fi
# SMT compara C con L; sin hermanos disponibles usa 2*C por sobresuscripción.
if [[ $smt_counts == auto ]]; then
    smt_counts=$cores
    if (( logical > cores )); then smt_counts+=",$logical"; else smt_counts+=",$((cores * 2))"; fi
fi

if [[ -z $destination ]]; then
    destination=resultsrt
fi
# Reutilizar la carpeta: C++ trunca los CSV y Matplotlib reemplaza sus PNG.
# No borrar capturas de perf ni otros archivos ajenos a estos benchmarks.
# Prepara la carpeta sin eliminar archivos ajenos a la campaña.
mkdir -p "$destination"
# Metadatos de campaña: parámetros y topología asociados a los CSV que se generan.
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
# Lista explícita de archivos de esta ejecución para evitar mezclar campañas históricas.
generated_csv=("$destination/sequential.csv")
# Mide un modelo reutilizando sequential.csv; después grafica su archivo individual.
measure() {
    # Nombre local de captura; shift deja únicamente las opciones del modelo.
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

# Lista de capturas instrumentadas, independiente de los CSV temporales sin perf.
generated_profiles=()
# Perfila una configuración; solo sequential crea la referencia instrumentada común.
profile_model() {
    local label=$1
    shift
    echo "=== Perf: $label ($profile_repetitions repeticiones) ==="
    # Array opcional vacío para la base o con --baseline-capture para los demás modelos.
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

# Descompone un barrido P1,P2,... y captura cada P en una carpeta identificable.
profile_workers() {
    local model=$1
    local label=$2
    local list=$3
    local workers p
    # Usa coma como separador únicamente para leer la lista de trabajadores.
    IFS=, read -r -a workers <<< "$list"
    for p in "${workers[@]}"; do
        profile_model "$label-p$p" --model "$model" --threads "$p"
    done
}

# Perf necesita su propia referencia instrumentada, también medida UNA sola vez.
# Las llamadas siguientes son secuenciales: no ejecutan renders de modelos distintos a la vez.
profile_model sequential --model sequential
# Todos los modelos reutilizan los mismos tiempos; no ejecutan sequential.
measure fine --model fine
measure coarse --model coarse --thread-counts "$thread_counts"
measure cmp --model cmp --thread-counts "$thread_counts"
profile_model fine --model fine
profile_workers coarse coarse "$thread_counts"
profile_workers cmp cmp "$thread_counts"
IFS=, read -r -a state_list <<< "$states"
# Cada fase fija el estado global y conserva la misma carga y P explícito de SMT.
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
