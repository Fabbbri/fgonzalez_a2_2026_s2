#!/usr/bin/env bash
# Verifica la interfaz de consola; el primer argumento es la ruta del ejecutable.
# Requiere las adquisiciones locales y devuelve un estado no nulo si falla una prueba.
set -euo pipefail
exe=${1:-./build/interferogram}
"$exe" --help | grep -q 'Uso:'
output=$("$exe" --width 16 --height 16 --x 12000 --y 750)
grep -q 'Samples: 256' <<< "$output"
grep -q 'Mode: sequential' <<< "$output"
grep -q 'Kernel time:' <<< "$output"
# El último píxel del burst debe poder leerse como una ventana de 1×1.
"$exe" --width 1 --height 1 --x 24444 --y 1499 >/dev/null
# Ejecuta el programa con los argumentos recibidos y exige un código de error.
# Si se aceptan, detiene la prueba con estado 1.
fails() {
    if "$exe" "$@" >/dev/null 2>&1; then
        echo "FAIL: argumentos aceptados: $*" >&2
        exit 1
    fi
}
fails --width 0
fails --height -1
fails --width 24446
fails --height 1501
fails --x -1
fails --x 24445
fails --width 9999999999999999999999
fails --width 12abc
fails --width
fails --a --b
fails --threads 0
fails --a /nonexistent/slc.tiff
fails --b /nonexistent/slc.tiff
echo 'PASS: CLI válida, límites y errores'

fails --mode unknown
fails --tile-width 0
fails --tile-height -1
fails --tile-width
reference=$("$exe" --x 12000 --y 750 --width 16 --height 16)
tiled=$("$exe" --x 12000 --y 750 --width 16 --height 16 --mode tiled --tile-width 6 --tile-height 5)
grep -q 'Mode: tiled' <<< "$tiled"
grep -q 'Tiles: 12' <<< "$tiled"
[[ $(grep 'Result sum' <<< "$reference") == $(grep 'Result sum' <<< "$tiled") ]]
echo 'PASS: CLI de tiles y resultado equivalente'

fails --mode fine-grained --quantum 0
fails --mode fine-grained --quantum -1
fails --quantum abc
fails --quantum
fine=$("$exe" --x 12000 --y 750 --width 16 --height 16 --mode fine-grained --tile-width 6 --tile-height 5 --quantum 2)
grep -q 'Mode: fine-grained' <<< "$fine"
grep -q 'Tasks: 12' <<< "$fine"
grep -q 'Rounds: 15' <<< "$fine"
grep -q 'Dispatches: 128' <<< "$fine"
[[ $(grep 'Result sum' <<< "$reference") == $(grep 'Result sum' <<< "$fine") ]]
echo 'PASS: CLI Fine-Grained y equivalencia'

fails --threads -1
fails --threads abc
coarse=$("$exe" --x 12000 --y 750 --width 16 --height 16 --mode coarse-grained --tile-width 6 --tile-height 5 --threads 3)
grep -q 'Workers: 3' <<< "$coarse"
[[ $(grep 'Result sum' <<< "$reference") == $(grep 'Result sum' <<< "$coarse") ]]
echo 'PASS: CLI Coarse-Grained y equivalencia'

# Restringir solo el proceso de prueba y sus hijos, no la terminal del usuario.
python3 - "$exe" <<'PYTEST'
import os
import subprocess
import sys

if not hasattr(os, 'sched_getaffinity'):
    print('SKIP: integración SMT requiere Linux')
    sys.exit(0)
os.sched_setaffinity(0, {min(os.sched_getaffinity(0))})
base = [sys.argv[1], '--x', '12000', '--y', '750', '--width', '16', '--height', '16']
reference = subprocess.check_output(base, text=True)
expected = next(line for line in reference.splitlines() if line.startswith('Result sum'))
for extra in ([], ['--threads', '3']):
    output = subprocess.check_output(base + ['--mode', 'smt'] + extra, text=True)
    assert expected in output
    assert 'Available physical cores: 1' in output
    assert 'Workers: ' + ('3' if extra else '2') in output
    assert 'SMT approximation' in output
failed = subprocess.run(base + ['--mode', 'smt', '--threads', '1'], capture_output=True, text=True)
assert failed.returncode != 0 and 'sobresuscripción' in failed.stderr
print('PASS: SMT integración y equivalencia')
PYTEST
