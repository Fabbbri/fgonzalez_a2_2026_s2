#!/usr/bin/env bash
# Captura de perf stat sobre el proceso C++ y sus hilos heredados.
# Cada repetición conserva contadores y diagnósticos; los tiempos instrumentados
# son independientes del benchmark. Una firma de topología detecta cambios entre capturas.
# Contadores del proceso y sus hilos heredados. Cada repetición conserva la
# salida original de perf; no se mezclan tiempos instrumentados con benchmark.
# Modo estricto: un fallo evita publicar el lote como completo.
set -euo pipefail
# Ruta del simulador y número de repeticiones recibidos del objetivo profile.
binary=${1:?Falta el ejecutable}
repetitions=${2:?Faltan las repeticiones}
# Las opciones restantes corresponden a una configuración física y un modelo.
shift 2
# Comando de perf y eventos definidos por entorno, con valores predeterminados.
perf_binary=${PERF:-perf}
events=${PERF_EVENTS:-task-clock,context-switches,cpu-migrations,page-faults,cycles,instructions,cache-references,cache-misses}
[[ $(uname -s) == Linux ]] || { printf 'El perfilado con perf requiere Linux.\n' >&2; exit 1; }
[[ $repetitions =~ ^[1-9][0-9]*$ ]] || { printf 'Repeticiones debe ser un entero positivo.\n' >&2; exit 1; }
command -v "$perf_binary" >/dev/null || { printf 'No se encontró perf; instale la versión correspondiente a su kernel.\n' >&2; exit 1; }
# Cuenta selecciones explícitas de modelo para evitar banderas duplicadas.
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
# Carpeta única por lote: conserva evidencia aunque falle una ejecución.
result_dir=$(mktemp -d "output/profile-$(date +%Y%m%d-%H%M%S)-XXXXXX")
printf 'Perfilado: %s\n' "$result_dir"
# Solo un lote terminado publica status=complete. Los fallos conservan evidencia.
# Estado inicial transaccional; solo se sustituye al terminar todas las capturas.
printf 'incomplete\n' > "$result_dir/status.txt"
# Bloque de procedencia: fecha UTC, SO, perf, política PMU, CPU, commit, binario y flags.
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
# Compara el estado actual del simulador con la instantánea previa del lote.
check_topology() {
    "$binary" --topology > "$result_dir/current-topology.txt"
    cmp -s "$result_dir/topology.txt" "$result_dir/current-topology.txt" || {
        printf 'Cambió la topología/afinidad o SMT; lote incompleto.\n' >&2; exit 1;
    }
}
# Verifica permisos y selección de eventos antes de ejecutar el trabajo costoso.
# Sondeo con true verifica permisos y eventos antes de la carga costosa;
# LC_ALL=C fija el formato numérico que utiliza el analizador.
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
# Capturas secuenciales: un archivo crudo por repetición y sin competencia entre modelos.
for ((run=1; run<=repetitions; run++)); do
    check_topology
    # Array de argumentos para perf y el simulador; la trayectoria se controla desde el perfilador.
    command=("$perf_binary" stat --no-big-num -x ';' -e "$events"
             -o "$result_dir/perf-$run.csv" -- "$binary" "$@"
             --no-output --trajectory "$result_dir/trajectory.nbody")
    printf '%q ' "${command[@]}" >> "$result_dir/command.txt"
    printf '\n' >> "$result_dir/command.txt"
    # Fallo del proceso o de perf conserva la captura parcial y su diagnóstico.
    if ! LC_ALL=C "${command[@]}" > "$result_dir/metrics-$run.csv" 2> "$result_dir/run-$run.log"; then
        cat "$result_dir/run-$run.log" >&2
        printf 'Falló el perfilado %s; lote incompleto: %s\n' "$run" "$result_dir" >&2
        exit 1
    fi
    check_topology
    # Eventos no soportados o sin conteo impiden etiquetar el lote como válido.
    if grep -Eq '<not supported>|<not counted>' "$result_dir/perf-$run.csv"; then
        printf 'Contadores no disponibles en perf-%s.csv; lote incompleto.\n' "$run" >&2
        exit 1
    fi
    printf 'Perfilado: %s/%s\n' "$run" "$repetitions"
done
rm "$result_dir/current-topology.txt"
# Publicación final únicamente después de verificar todas las repeticiones.
printf 'complete\n' > "$result_dir/status.txt"
printf 'Contadores completos: %s/perf-*.csv\n' "$result_dir"
