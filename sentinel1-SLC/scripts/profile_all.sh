#!/usr/bin/env bash
# Ejecuta los casos de perfilado en un directorio nuevo.
set -euo pipefail
out=${1:?Falta salida}
repetitions=${2:?Faltan repeticiones}
cases=${3:?Faltan casos}
shift 3

if command -v systemd-detect-virt >/dev/null && systemd-detect-virt --quiet; then
    echo 'El perfilado principal requiere hardware físico' >&2
    exit 2
fi
if [[ $out == auto ]]; then
    out="results/profile-$(date +%Y%m%d-%H%M%S)-$$"
fi
if [[ -e $out ]]; then
    echo 'El directorio debe ser nuevo' >&2
    exit 2
fi
mkdir -p "$out"
printf 'incomplete\n' > "$out/status.txt"
read -r -a selected_cases <<< "$cases"
for spec in "${selected_cases[@]}"; do
    mode=${spec%:*}
    threads=${spec#*:}
    bash scripts/profile_perf.sh "$out/$mode-$threads" "$mode" "$threads" "$repetitions" "$@"
done
printf 'complete\n' > "$out/status.txt"
printf 'Perfilado: %s\n' "$out"
