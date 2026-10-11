#!/usr/bin/env bash
set -euo pipefail
binary=${1:-./build/nbody}
test_dir=$(mktemp -d /tmp/nbody-cli.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
args=(--bodies-per-galaxy 5 --steps 25 --dt 0.002 --no-output)
# Guardar y medir no inicializa SDL.
SDL_VIDEODRIVER=unavailable "$binary" "${args[@]}" --trajectory "$test_dir/first.nbody" > "$test_dir/metrics.csv" 2> "$test_dir/first.txt"
"$binary" "${args[@]}" --trajectory "$test_dir/second.nbody" > /dev/null 2> "$test_dir/second.txt"
cmp "$test_dir/first.nbody" "$test_dir/second.nbody"
# Una muestra contiene identificación del experimento y tiempo total.
if grep -Eq 'Diagnóstico físico|Energía|Momento|Masa:' "$test_dir/first.txt"; then exit 1; fi
awk -F, 'NR==1 {if ($0!="model,workers,bodies,steps,dt,seed,quantum,total_seconds") exit 1} NR==2 {if (NF!=8 || $1!="sequential" || $2!=1 || $3!=10 || $4!=25 || $5!=0.002 || $6!=42 || $7!=0 || $8<0) exit 1} END {if (NR!=2) exit 1}' "$test_dir/metrics.csv"
"$binary" --steps 0 --no-output --trajectory "$test_dir/zero.nbody" > "$test_dir/zero.csv" 2> /dev/null
awk -F, 'NR==2 {if ($4!=0 || $8<0) exit 1}' "$test_dir/zero.csv"
# Fine-Grained conserva todos los estados de referencia para distintos quantums.
for quantum in 1 3 50; do
    "$binary" "${args[@]}" --model fine-grained --quantum "$quantum" --trajectory "$test_dir/fine.nbody" > "$test_dir/fine.csv" 2> /dev/null
    cmp "$test_dir/first.nbody" "$test_dir/fine.nbody"
    awk -F, -v quantum="$quantum" 'NR==2 {if (NF!=8 || $1!="fine-grained" || $2!=1 || $7!=quantum || $8<=0) exit 1}' "$test_dir/fine.csv"
done
for threads in 1 2 3 12; do
    "$binary" "${args[@]}" --model coarse-grained --threads "$threads" --trajectory "$test_dir/coarse.nbody" > "$test_dir/coarse.csv" 2> "$test_dir/coarse.txt"
    cmp "$test_dir/first.nbody" "$test_dir/coarse.nbody"
    awk -F, -v threads="$threads" 'NR==2 {if (NF!=8 || $1!="coarse-grained" || $2!=threads || $7!=0 || $8<=0) exit 1}' "$test_dir/coarse.csv"
    grep -q '26 fases de sincronización completadas' "$test_dir/coarse.txt"
done
"$binary" --model coarse-grained --steps 0 --no-output --trajectory "$test_dir/default-coarse.nbody" > "$test_dir/default-coarse.csv" 2> /dev/null
awk -F, 'NR==2 {if ($2!=2 || $4!=0) exit 1}' "$test_dir/default-coarse.csv"
# Un error de archivo debe cerrar el pool y no publicar métricas.
if "$binary" --model coarse-grained --no-output --trajectory "$test_dir" > "$test_dir/coarse-failed.csv" 2> /dev/null; then exit 1; fi
test ! -s "$test_dir/coarse-failed.csv"
"$binary" --topology > "$test_dir/topology.txt"
cores=$(sed -n 's/^physical_cores=\([0-9]*\),.*/\1/p' "$test_dir/topology.txt")
test "$cores" -gt 0
for threads in "$((cores+1))" "$((cores*2))"; do
    "$binary" "${args[@]}" --model smt --threads "$threads" --trajectory "$test_dir/smt.nbody" > "$test_dir/smt.csv" 2> "$test_dir/smt.txt"
    cmp "$test_dir/first.nbody" "$test_dir/smt.nbody"
    awk -F, -v threads="$threads" 'NR==2 {if (NF!=8 || $1!="smt" || $2!=threads || $7!=0 || $8<=0) exit 1}' "$test_dir/smt.csv"
done
"$binary" "${args[@]}" --model smt --trajectory "$test_dir/smt.nbody" > "$test_dir/smt-default.csv" 2> /dev/null
cmp "$test_dir/first.nbody" "$test_dir/smt.nbody"
awk -F, -v threads="$((cores*2))" 'NR==2 {if ($2!=threads) exit 1}' "$test_dir/smt-default.csv"
if command -v taskset > /dev/null && [[ $(uname -s) == Linux ]]; then
    cpu=$(sed -n 's/.*allowed_cpus=\([0-9]*\).*/\1/p' "$test_dir/topology.txt")
    taskset -c "$cpu" "$binary" --topology > "$test_dir/restricted-topology.txt"
    grep -q '^physical_cores=1,logical_cpus=1,' "$test_dir/restricted-topology.txt"
    taskset -c "$cpu" "$binary" "${args[@]}" --model smt --trajectory "$test_dir/restricted.nbody" > "$test_dir/restricted.csv" 2> /dev/null
    cmp "$test_dir/first.nbody" "$test_dir/restricted.nbody"
    awk -F, 'NR==2 {if ($2!=2) exit 1}' "$test_dir/restricted.csv"
fi
if "$binary" --model smt --threads "$cores" --no-output --steps 0 > "$test_dir/smt-invalid.csv" 2> /dev/null; then exit 1; fi
test ! -s "$test_dir/smt-invalid.csv"
for options in '--model unknown' '--model fine-grained --quantum 0' '--quantum 2' '--threads 2' '--model fine-grained --threads 2' '--model coarse-grained --threads 0' '--model coarse-grained --threads -1' '--model coarse-grained --quantum 1' '--model smt --threads 0' '--model smt --quantum 1' '--topology --steps 1'; do
    read -r -a invalid <<< "$options"
    if "$binary" "${invalid[@]}" --no-output --steps 0 > /dev/null 2>&1; then exit 1; fi
done
# Cierre y métricas preceden la apertura de la ventana.
if SDL_VIDEODRIVER=unavailable "$binary" --steps 1 --trajectory "$test_dir/before-window.nbody" > "$test_dir/before-window.csv" 2> /dev/null; then exit 1; fi
test -s "$test_dir/before-window.nbody"
test "$(wc -l < "$test_dir/before-window.csv")" -eq 2
if SDL_VIDEODRIVER=unavailable "$binary" --replay "$test_dir/before-window.nbody" > "$test_dir/replay.csv" 2> /dev/null; then exit 1; fi
test ! -s "$test_dir/replay.csv"
if "$binary" --no-output --dt nan > /dev/null 2>&1; then exit 1; fi
if "$binary" --bodies-per-galaxy 0 > /dev/null 2>&1; then exit 1; fi
if "$binary" --output-every 10 > /dev/null 2>&1; then exit 1; fi
if "$binary" --replay missing --steps 1 > /dev/null 2>&1; then exit 1; fi
if "$binary" --steps 0 --no-output --trajectory "$test_dir" > "$test_dir/failed.csv" 2> /dev/null; then exit 1; fi
test ! -s "$test_dir/failed.csv"
# El benchmark debe rechazar un cambio de topología antes de agregar muestras.
benchmark_script=$(cd "$(dirname "$0")/../scripts" && pwd)/benchmark.sh
cat > "$test_dir/changing-topology" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
count=0
if [[ -f $0.count ]]; then read -r count < "$0.count"; fi
count=$((count+1))
printf '%s\n' "$count" > "$0.count"
printf 'physical_cores=%s,logical_cpus=4,smt_control=on\n' "$count"
MOCK
chmod +x "$test_dir/changing-topology"
if (cd "$test_dir" && bash "$benchmark_script" "$test_dir/changing-topology" 2 python3) > "$test_dir/benchmark.txt" 2>&1; then exit 1; fi
grep -q 'Cambió la topología' "$test_dir/benchmark.txt"
for samples in "$test_dir"/output/benchmark-*/samples.csv; do
    test "$(wc -l < "$samples")" -eq 1
done
printf 'Pruebas de CLI, métricas, orden del flujo y reproducibilidad correctas.\n'
