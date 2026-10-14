#!/usr/bin/env bash
set -euo pipefail
# 12 configuraciones, N mediciones por configuración.
# Uso: benchmark.sh [SALIDA|auto] [REPETICIONES] [argumentos comunes del interferograma]
out=${1:-auto}
repetitions=${2:-200}
shift 2 || true
[[ $repetitions =~ ^[1-9][0-9]*$ ]] && (( repetitions >= 2 )) || { echo 'Se requieren al menos dos repeticiones' >&2; exit 2; }
if (( repetitions >= 200 )); then
    if [[ -r /proc/version ]] && grep -qi microsoft /proc/version; then
        echo 'Las mediciones principales requieren hardware físico, no WSL' >&2; exit 2
    fi
    if command -v systemd-detect-virt >/dev/null && systemd-detect-virt --quiet; then
        echo 'Las mediciones principales requieren hardware físico, no VM/contenedor' >&2; exit 2
    fi
fi
mkdir -p results
if [[ $out == auto ]]; then out="results/benchmark-$(date +%Y%m%d-%H%M%S)-$$"; fi
if [[ -e $out ]]; then echo 'El directorio de salida debe ser nuevo' >&2; exit 2; fi
./build/benchmark "$out" "$repetitions" ./build/interferogram "$@"
{
    date -u
    uname -a
    lscpu
    sha256sum ./build/interferogram ./build/benchmark
    printf 'commit='; git rev-parse HEAD 2>/dev/null || true
    git status --short 2>/dev/null || true
} > "$out/environment.txt" || { printf 'incomplete\n' > "$out/status.txt"; exit 1; }
