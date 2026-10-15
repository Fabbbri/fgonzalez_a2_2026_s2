#!/usr/bin/env bash
# Prueba la orquestación sin acceso a los contadores de hardware.
set -euo pipefail
project=$(cd "$(dirname "$0")/../.." && pwd)
temporary=$(mktemp -d /tmp/interferogram-perf.XXXXXX)
trap 'rm -rf "$temporary"' EXIT
mkdir -p "$temporary/build"
cat > "$temporary/build/interferogram" <<'MOCK'
#!/usr/bin/env bash
[[ ${MOCK_FAILURE:-} != program ]] || exit 1
if [[ ${MOCK_FAILURE:-} == bad-output ]]; then echo 'salida incompleta'; exit; fi
mode=sequential
threads=1
while (( $# > 0 )); do
    case $1 in
        --mode) mode=$2; shift ;;
        --threads) threads=$2; shift ;;
    esac
    shift
done
printf 'Mode: %s\nKernel time: 1 ms\nResult sum (real, imag): 1, 0\nWorkers: %s\n' "$mode" "$threads"
MOCK
cat > "$temporary/perf" <<'MOCK'
#!/usr/bin/env bash
set -euo pipefail
if [[ $1 == --version ]]; then echo 'perf mock'; exit; fi
if [[ ${MOCK_FAILURE:-} == denied ]]; then echo 'Permission denied' >&2; exit 255; fi
while [[ $1 != -- ]]; do
    if [[ $1 == -o ]]; then file=$2; shift; fi
    shift
done
shift
if [[ ${MOCK_FAILURE:-} == empty ]]; then
    : > "$file"
elif [[ ${MOCK_FAILURE:-} == unsupported ]]; then
    echo '<not supported>;;cycles;0;0' > "$file"
elif [[ ${MOCK_FAILURE:-} == not-counted && $1 != true ]]; then
    echo '<not counted>;;cycles;0;0' > "$file"
else
    echo '100;;cycles;100;100.00' > "$file"
fi
"$@"
MOCK
cat > "$temporary/taskset" <<'MOCK'
#!/usr/bin/env bash
[[ $1 == -c ]] || exit 1
shift 2
"$@"
MOCK
chmod +x "$temporary/perf" "$temporary/taskset" "$temporary/build/interferogram"
cd "$temporary"
export PERF="$temporary/perf"
export PATH="$temporary:$PATH"
bash "$project/scripts/profile_perf.sh" ok smt 8 2 > /dev/null
[[ $(cat ok/status.txt) == complete ]]
[[ -s ok/perf-1.csv && -s ok/perf-2.csv && -s ok/program-2.log ]]
rg -q -- '-a -A -C' ok/command.txt
rg -q -- '--quantum 2' ok/command.txt
PERF_CPUS=0,1 bash "$project/scripts/profile_perf.sh" affinity smt 8 1 > /dev/null
[[ $(cat affinity/status.txt) == complete ]]
rg -q -- '-a -A -C' affinity/command.txt
rg -q '0,1' affinity/environment.txt
for failure in denied empty unsupported not-counted program bad-output; do
    if MOCK_FAILURE=$failure bash "$project/scripts/profile_perf.sh" "$failure" smt 8 1 > /dev/null 2>&1; then
        echo "No detectó el fallo $failure" >&2; exit 1
    fi
    [[ $(cat "$failure/status.txt") == incomplete ]]
done
echo 'Pruebas de perfilado correctas (perf simulado).'
