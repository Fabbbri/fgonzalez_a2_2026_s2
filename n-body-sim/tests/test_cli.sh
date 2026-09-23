#!/usr/bin/env bash
set -euo pipefail
binary=${1:-./build/nbody}
test_dir=$(mktemp -d /tmp/nbody-cli.XXXXXX)
args=(--bodies-per-galaxy 5 --steps 25 --dt 0.002 --no-output)

# Un controlador gráfico inexistente prueba que --no-output no inicia SDL.
SDL_VIDEODRIVER=unavailable "$binary" "${args[@]}" > "$test_dir/stdout.txt" 2> "$test_dir/first.txt"
test ! -s "$test_dir/stdout.txt"
"$binary" "${args[@]}" > /dev/null 2> "$test_dir/second.txt"
sed -n '/^Diagnóstico físico/,$p' "$test_dir/first.txt" > "$test_dir/physics-first.txt"
sed -n '/^Diagnóstico físico/,$p' "$test_dir/second.txt" > "$test_dir/physics-second.txt"
cmp "$test_dir/physics-first.txt" "$test_dir/physics-second.txt"
test -s "$test_dir/physics-first.txt"

"$binary" --steps 0 --no-output > "$test_dir/zero.txt" 2> /dev/null
test ! -s "$test_dir/zero.txt"
if "$binary" --no-output --dt nan > /dev/null 2>&1; then exit 1; fi
if "$binary" --bodies-per-galaxy 0 > /dev/null 2>&1; then exit 1; fi
if "$binary" --output-every 10 > /dev/null 2>&1; then exit 1; fi
printf 'Pruebas de CLI, ejecución sin gráficos y reproducibilidad correctas.\n'
