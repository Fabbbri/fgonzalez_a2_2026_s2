#!/bin/bash
# ============================================================
# medir_csv.sh  -  genera UN CSV de tiempos por tanda.
#
# El estado de SMT se lee de /sys/devices/system/cpu/smt/active y decide
# el nombre del archivo:
#   resultados/tiempos_smt_on.csv   (SMT activo)
#   resultados/tiempos_smt_off.csv  (SMT desactivado en BIOS)
#
# Columnas: modelo,hilos,repeticion,tiempo_total_ms,tiempo_secuencial_ms,
#           smt_activo_linux,cpus,checksum,epocas
# tiempo_secuencial_ms = media de las repeticiones de main EN ESTA tanda.
#
# Configuraciones:
#   main   1 hilo   (taskset CPU 0)
#   fino   1 hilo   (taskset CPU 0)
#   grueso 1,2,4,8  (taskset: una CPU por nucleo fisico)   *requiere ./grueso <epocas> <hilos>
#   smt    8,12     (CPUs logicas 0..T-1; si T > CPUs, smt reparte con modulo)
#   cmp    1,2,4,8  (cmp elige solo un hilo por nucleo fisico)
#
# Repeticiones (variables de entorno): REP_MAIN=10 REP_GRUESO=10
#   REP_SMT=5 REP_CMP=5 REP_FINO=3.   EPOCAS=1000.
# Requiere sudo (gobernador performance) y cargador conectado.
# ============================================================

set -u
EPOCAS=${EPOCAS:-1000}
REP_MAIN=${REP_MAIN:-10}
REP_GRUESO=${REP_GRUESO:-10}
REP_SMT=${REP_SMT:-5}
REP_CMP=${REP_CMP:-5}
REP_FINO=${REP_FINO:-3}
PAUSA=${PAUSA:-2}

CPU=/sys/devices/system/cpu

# ---------- Estado de SMT ----------
SMT_ACTIVO=$(cat $CPU/smt/active 2>/dev/null || echo NA)
case "$SMT_ACTIVO" in
    1) ETQ=on ;;
    0) ETQ=off ;;
    *) echo "Error: no se pudo leer $CPU/smt/active"; exit 1 ;;
esac
NCPU=$(nproc --all)
echo "SMT activo en Linux: $SMT_ACTIVO ($ETQ) | CPUs logicas: $NCPU"

# ---------- Comprobaciones ----------
for exe in main fino grueso smt cmp; do
    [ -x "./$exe" ] || { echo "Error: falta ./$exe (make)"; exit 1; }
done

AC=$(cat /sys/class/power_supply/AC*/online 2>/dev/null | head -1)
if [ "$AC" != "1" ] && [ "${FORZAR:-0}" != "1" ]; then
    echo "Error: el portatil no esta conectado a la corriente (FORZAR=1 para ignorar)."
    exit 1
fi

# ---------- Gobernador: performance durante la tanda, restaurar al salir ----------
GOV_PREV=$(cat $CPU/cpu0/cpufreq/scaling_governor 2>/dev/null || echo powersave)
restaurar() {
    echo "Restaurando gobernador: $GOV_PREV"
    for g in $CPU/cpu*/cpufreq/scaling_governor; do
        echo "$GOV_PREV" | sudo tee "$g" >/dev/null
    done
}
trap restaurar EXIT
for g in $CPU/cpu*/cpufreq/scaling_governor; do
    echo performance | sudo tee "$g" >/dev/null || { echo "Error fijando gobernador"; exit 1; }
done

# ---------- Utilidades de CPUs ----------
# Primera CPU logica de cada nucleo fisico, las N primeras.
cpus_nucleos() {
    local n=$1 lista="" vistos=" " c core pkg clave cont=0
    for ((c=0; c<NCPU; c++)); do
        core=$(cat $CPU/cpu$c/topology/core_id 2>/dev/null) || continue
        pkg=$(cat $CPU/cpu$c/topology/physical_package_id 2>/dev/null) || continue
        clave="$pkg:$core"
        case "$vistos" in *" $clave "*) continue ;; esac
        vistos="$vistos$clave "
        lista="${lista:+$lista,}$c"
        cont=$((cont+1))
        [ "$cont" -eq "$n" ] && break
    done
    echo "$lista"
}

# CPUs logicas consecutivas 0..min(N,NCPU)-1
cpus_logicas() {
    local n=$1 lista="" c
    [ "$n" -gt "$NCPU" ] && n=$NCPU
    for ((c=0; c<n; c++)); do lista="${lista:+$lista,}$c"; done
    echo "$lista"
}

# ---------- Prueba previa: interfaz de grueso y equivalencia ----------
echo "Prueba previa (2 epocas)..."
CHK_MAIN=$(taskset -c 0 ./main 2 2>&1 | grep Checksum | awk '{print $3}')
SAL_G=$(taskset -c "$(cpus_nucleos 4)" ./grueso 2 4 2>&1)
if ! echo "$SAL_G" | grep -q "^Hilos: 4"; then
    echo "Error: ./grueso no acepta '<epocas> <hilos>' (debe imprimir 'Hilos: N')."
    exit 1
fi
CHK_G=$(echo "$SAL_G" | grep Checksum | awk '{print $3}')
if [ -z "$CHK_MAIN" ] || [ "$CHK_G" != "$CHK_MAIN" ]; then
    echo "Error: checksum de grueso ($CHK_G) distinto de main ($CHK_MAIN)."
    exit 1
fi

# ---------- CSV ----------
mkdir -p resultados
CSV="resultados/tiempos_smt_${ETQ}.csv"
[ -f "$CSV" ] && mv "$CSV" "$CSV.bak.$(date +%s)"
echo "modelo,hilos,repeticion,tiempo_total_ms,tiempo_secuencial_ms,smt_activo_linux,cpus,checksum,epocas" > "$CSV"

# Ejecuta una vez y deja TIEMPO_MS y CHECKSUM
ejecutar() {
    local salida us
    salida=$("$@" 2>&1)
    us=$(echo "$salida" | grep "Tiempo total" | awk '{print $3}')
    CHECKSUM=$(echo "$salida" | grep Checksum | awk '{print $3}')
    if ! [[ "$us" =~ ^[0-9]+$ ]]; then
        echo "Error ejecutando: $*" >&2
        echo "$salida" | tail -5 >&2
        exit 1
    fi
    TIEMPO_MS=$(awk -v u="$us" 'BEGIN { printf "%.3f", u/1000 }')
}

# medir MODELO HILOS REPS CPUS_TXT CMD...
medir() {
    local modelo=$1 hilos=$2 reps=$3 cpus=$4; shift 4
    local i
    echo "  $modelo hilos=$hilos ($reps rep.) cpus=$cpus"
    for ((i=1; i<=reps; i++)); do
        ejecutar "$@"
        echo "$modelo,$hilos,$i,$TIEMPO_MS,$T_SEQ_MS,$SMT_ACTIVO,\"$cpus\",$CHECKSUM,$EPOCAS" >> "$CSV"
        sleep "$PAUSA"
    done
}

echo "============ TANDA SMT $ETQ -> $CSV ============"

# ---------- main primero: define el tiempo secuencial de la tanda ----------
echo "  main hilos=1 ($REP_MAIN rep.) cpus=0"
TIEMPOS_MAIN=()
CHKS_MAIN=()
for ((i=1; i<=REP_MAIN; i++)); do
    ejecutar taskset -c 0 ./main "$EPOCAS"
    TIEMPOS_MAIN+=("$TIEMPO_MS")
    CHKS_MAIN+=("$CHECKSUM")
    sleep "$PAUSA"
done
T_SEQ_MS=$(printf '%s\n' "${TIEMPOS_MAIN[@]}" | awk '{s+=$1} END {printf "%.3f", s/NR}')
for ((i=1; i<=REP_MAIN; i++)); do
    echo "main,1,$i,${TIEMPOS_MAIN[$((i-1))]},$T_SEQ_MS,$SMT_ACTIVO,\"0\",${CHKS_MAIN[$((i-1))]},$EPOCAS" >> "$CSV"
done
echo "  tiempo_secuencial_ms (media de main) = $T_SEQ_MS"

# ---------- grueso ----------
for T in 1 2 4 8; do
    C=$(cpus_nucleos "$T")
    medir grueso "$T" "$REP_GRUESO" "$C" taskset -c "$C" ./grueso "$EPOCAS" "$T"
done

# ---------- cmp ----------
for T in 1 2 4 8; do
    medir cmp "$T" "$REP_CMP" "auto(1 por nucleo)" ./cmp "$T" "$EPOCAS"
done

# ---------- smt (8 y 12 hilos) ----------
for T in 8 12; do
    C=$(cpus_logicas "$T")
    medir smt "$T" "$REP_SMT" "$C" ./smt "$T" "$C" "$EPOCAS"
done

# ---------- fino (lo mas lento: al final) ----------
medir fino 1 "$REP_FINO" "0" taskset -c 0 ./fino "$EPOCAS"

echo
echo "Listo: $CSV ($(($(wc -l < "$CSV") - 1)) filas)"
echo "Checksums distintos por modelo (main/grueso deben coincidir; fino/smt/cmp entre si):"
awk -F, 'NR>1 {print $1, $8}' "$CSV" | sort -u