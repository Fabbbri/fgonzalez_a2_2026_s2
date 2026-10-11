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
for options in '--model unknown' '--model fine-grained --quantum 0' '--quantum 2'; do
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
printf 'Pruebas de CLI, métricas, orden del flujo y reproducibilidad correctas.\n'
