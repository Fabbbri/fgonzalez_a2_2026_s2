#!/usr/bin/env bash
# Ensayo de orquestación perf con comandos temporales que emiten datos sintéticos.
# No accede a la PMU ni cambia permisos. Comprueba éxito, contadores ausentes,
# fallos del simulador y cambios de topología sin fabricar evidencia de hardware.
# Prueba la orquestación sin requerir acceso a la PMU ni cambiar permisos.
set -euo pipefail
# Resuelve el perfilador real; perf y simulador de este ensayo son fixtures.
script=$(cd "$(dirname "$0")/../scripts" && pwd)/profile.sh
# Aísla comandos y archivos de la prueba en una carpeta temporal.
test_dir=$(mktemp -d /tmp/nbody-profile.XXXXXX)
trap 'rm -rf "$test_dir"' EXIT
# Mock de perf: escribe contadores conocidos o diagnósticos según MOCK_MODE.
cat > "$test_dir/perf" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
if [[ $1 == --version ]]; then printf 'perf mock\n'; exit; fi
if [[ ${MOCK_MODE:-} == denied ]]; then printf 'Permission denied\n' >&2; exit 255; fi
while [[ $1 != -- ]]; do
    if [[ $1 == -o ]]; then output=$2; shift; fi
    shift
done
shift
if [[ ${MOCK_MODE:-} == unsupported ]]; then
    printf '<not supported>;;cycles;0;0\n' > "$output"
elif [[ ${MOCK_MODE:-} == not-counted && $1 != true ]]; then
    printf '<not counted>;;cycles;0;0\n' > "$output"
else
    printf '100;;cycles;100;100.00\n200;;instructions;100;100.00\n' > "$output"
fi
"$@"
MOCK
# Mock del simulador: informa topología o una fila CSV temporal conocida.
cat > "$test_dir/nbody" <<'MOCK'
#!/usr/bin/env bash
if [[ $1 == --topology ]]; then
    if [[ ${MOCK_MODE:-} == topology-changed ]]; then
        count=0
        if [[ -f topology-count ]]; then read -r count < topology-count; fi
        printf '%s\n' "$((count+1))" > topology-count
        printf 'physical_cores=%s,logical_cpus=4\n' "$((count+1))"
    else
        printf 'physical_cores=2,logical_cpus=4\n'
    fi
    exit
fi
if [[ ${MOCK_MODE:-} == simulation-failed ]]; then exit 1; fi
printf 'model,workers,bodies,steps,dt,seed,quantum,total_seconds\nsequential,1,4,2,0.002,42,0,0.01\n'
MOCK
chmod +x "$test_dir/perf" "$test_dir/nbody"
cd "$test_dir"
# El script instrumentador ejecuta la fixture en lugar del perf instalado.
export PERF="$test_dir/perf"
bash "$script" "$test_dir/nbody" 2 --model sequential > success.log
result=$(sed -n 's/^Perfilado: \(output\/profile-.*\)$/\1/p' success.log | head -1)
test "$(cat "$result/status.txt")" = complete
test -s "$result/perf-1.csv"
test -s "$result/perf-2.csv"
test -s "$result/metrics-2.csv"
test -s "$result/topology.txt"
test -s "$result/command.txt"
# Cada fallo esperado debe dejar status incomplete y código de salida no exitoso.
for mode in denied unsupported not-counted simulation-failed topology-changed; do
    if MOCK_MODE=$mode bash "$script" "$test_dir/nbody" 1 > "$mode.log" 2>&1; then
        printf 'No detectó el fallo %s\n' "$mode" >&2; exit 1
    fi
    result=$(sed -n 's/^Perfilado: \(output\/profile-.*\)$/\1/p' "$mode.log" | head -1)
    test "$(cat "$result/status.txt")" = incomplete
done
if bash "$script" "$test_dir/nbody" 0 > /dev/null 2>&1; then exit 1; fi
if bash "$script" "$test_dir/nbody" 1 --trajectory other > /dev/null 2>&1; then exit 1; fi
printf 'Pruebas de perfilado correctas (perf simulado).\n'
