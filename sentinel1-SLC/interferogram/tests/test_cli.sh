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
fails --threads 2
fails --a /nonexistent/slc.tiff
fails --b /nonexistent/slc.tiff
echo 'PASS: CLI válida, límites y errores'
