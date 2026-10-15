#!/usr/bin/env bash
# Interfaz compatible: salida, repeticiones y argumentos del interferograma.
set -euo pipefail
cd "$(dirname "$0")/.."
out=${1:-resultsen}
repetitions=${2:-200}
if (( $# >= 2 )); then shift 2; else shift "$#"; fi
if [[ $out == auto ]]; then out="resultsen/benchmark-$(date +%Y%m%d-%H%M%S)-$$"; fi
selection=(--models sequential fine-grained coarse-grained smt cmp)
if [[ -n ${BENCHMARK_MODEL:-} ]]; then
    selection=(--model "$BENCHMARK_MODEL")
    if [[ $BENCHMARK_MODEL != sequential && $BENCHMARK_MODEL != fine-grained ]]; then
        selection+=(--threads "${BENCHMARK_THREADS:-8}")
    fi
fi
exec "${PYTHON:-python3}" scripts/benchmark.py ./build/interferogram \
    --results-dir "$out" --repetitions "$repetitions" --warmups "${WARMUPS:-3}" \
    --smt-states "${SMT_STATES:-keep}" "${selection[@]}" "$@"
