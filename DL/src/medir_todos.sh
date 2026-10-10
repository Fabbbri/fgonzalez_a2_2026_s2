#!/bin/bash
# ============================================================
# medir_todos.sh
#
# Fase 0: guarda la topología de CPU (para el informe).
# Fase 1: prueba de equivalencia (pocas épocas, compara checksums).
# Fase 2: medición de tiempos (épocas completas), speedup y eficiencia.
#
# Parámetros por variable de entorno (todos opcionales):
#   EPOCAS_TEST   épocas de la prueba de equivalencia   (def. 2)
#   EPOCAS        épocas de la medición                 (def. 1000)
#   REPETICIONES  repeticiones por programa             (def. 5)
#   REP_FINO      repeticiones de fino (es muy lento)   (def. 1)
#   SIN_FINO=1    omite fino en la fase 2
#   CPUS_SMT      2 CPUs lógicas del MISMO núcleo físico (def. autodetecta)
#   CPUS_CMP      2 CPUs lógicas de núcleos DISTINTOS    (def. autodetecta)
#   ETIQUETA      sufijo del directorio de resultados (ej. smt_on, smt_off)
#
# Ejemplos:
#   ./medir_todos.sh
#   SIN_FINO=1 REPETICIONES=10 ./medir_todos.sh
#   ETIQUETA=smt_off ./medir_todos.sh      # tras desactivar SMT en BIOS
# ============================================================

EPOCAS_TEST=${EPOCAS_TEST:-2}
EPOCAS=${EPOCAS:-1000}
REPETICIONES=${REPETICIONES:-5}
REP_FINO=${REP_FINO:-1}
SIN_FINO=${SIN_FINO:-0}
RESULTADOS="resultados${ETIQUETA:+_$ETIQUETA}"

mkdir -p "$RESULTADOS"
rm -f "$RESULTADOS"/*.txt "$RESULTADOS"/*.csv

# ------------------------------------------------------------
# Topología: elegir CPUs para el experimento SMT y CMP
# ------------------------------------------------------------

detectar_cpus() {
    local base=/sys/devices/system/cpu
    [ -r "$base/cpu0/topology/thread_siblings_list" ] || return 0

    if [ -z "$CPUS_SMT" ]; then
        local sib
        sib=$(cat "$base/cpu0/topology/thread_siblings_list")
        sib=${sib//-/,}                       # "0-1" -> "0,1"
        CPUS_SMT=$(echo "$sib" | cut -d, -f1,2)
        # Si solo hay una CPU lógica en el núcleo, SMT está desactivado
        case "$CPUS_SMT" in *,*) ;; *) CPUS_SMT="" ;; esac
    fi

    if [ -z "$CPUS_CMP" ]; then
        local core0 total c core
        core0=$(cat "$base/cpu0/topology/core_id")
        total=$(nproc --all)
        for ((c=1; c<total; c++)); do
            core=$(cat "$base/cpu$c/topology/core_id" 2>/dev/null)
            if [ -n "$core" ] && [ "$core" != "$core0" ]; then
                CPUS_CMP="0,$c"
                break
            fi
        done
    fi
}

detectar_cpus

{
    echo "Fecha: $(date)"
    echo "Etiqueta: ${ETIQUETA:-ninguna}"
    echo "CPUS_SMT (mismo núcleo): ${CPUS_SMT:-no disponible}"
    echo "CPUS_CMP (núcleos distintos): ${CPUS_CMP:-no disponible}"
    echo
    lscpu 2>/dev/null | grep -E "Model name|Thread|Core|Socket|^CPU\(s\)|On-line"
    echo
    lscpu -e 2>/dev/null
} > "$RESULTADOS/topologia.txt"

# ------------------------------------------------------------
# Lista de programas
# ------------------------------------------------------------

PROGS=("main" "fino" "grueso" "smt1")
[ -n "$CPUS_SMT" ] && PROGS+=("smt_smt")
[ -n "$CPUS_CMP" ] && PROGS+=("smt_cmp")
PROGS+=("cmp2" "cmp4" "cmp8")

# Ejecuta un programa con N épocas
correr() {
    local prog="$1" epocas="$2"
    case "$prog" in
        main|fino|grueso) ./"$prog" "$epocas" ;;
        smt1)    ./smt 1 0 "$epocas" ;;
        smt_smt) ./smt 2 "$CPUS_SMT" "$epocas" ;;
        smt_cmp) ./smt 2 "$CPUS_CMP" "$epocas" ;;
        cmp2) ./cmp 2 "$epocas" ;;
        cmp4) ./cmp 4 "$epocas" ;;
        cmp8) ./cmp 8 "$epocas" ;;
    esac
}

# Hilos del SO que usa cada programa (para la eficiencia)
hilos_de() {
    case "$1" in
        main|fino|smt1)      echo 1 ;;
        grueso|smt_smt|smt_cmp) echo 2 ;;
        cmp2) echo 2 ;; cmp4) echo 4 ;; cmp8) echo 8 ;;
    esac
}

# Verificar que existen los ejecutables
for exe in main fino grueso smt; do
    if [ ! -x "./$exe" ]; then
        echo "Error: no existe ./$exe (ejecuta make primero)."
        exit 1
    fi
done

# ============================================================
# FASE 1: EQUIVALENCIA
# ============================================================

echo "============================================"
echo " FASE 1: EQUIVALENCIA ($EPOCAS_TEST épocas)"
echo "============================================"
echo

declare -A CHK

for p in "${PROGS[@]}"; do
    echo "  Ejecutando $p..." >&2
    salida=$(correr "$p" "$EPOCAS_TEST" 2>&1)
    echo "$salida" > "$RESULTADOS/equiv_${p}.txt"
    CHK[$p]=$(echo "$salida" | grep "Checksum" | awk '{print $3}')
done

{
    printf "%-10s %-20s %-12s %-12s\n" "Modelo" "Checksum" "vs fino" "vs main"
    echo "--------------------------------------------------------------"
    for p in "${PROGS[@]}"; do
        c="${CHK[$p]:-NA}"
        [ "$c" = "${CHK[fino]}" ] && vf="IGUAL" || vf="DIFERENTE"
        [ "$c" = "${CHK[main]}" ] && vm="IGUAL" || vm="DIFERENTE"
        [ "$c" = "NA" ] && { vf="NA"; vm="NA"; }
        printf "%-10s %-20s %-12s %-12s\n" "$p" "$c" "$vf" "$vm"
    done
} | tee "$RESULTADOS/equivalencia.txt"

echo
echo "Lectura: fino y los smt* deberían coincidir bit a bit."
echo "main y grueso usan 'W * x' de Eigen (otro orden de sumas), así que"
echo "pueden diferir en los últimos bits; si difieren, compara con tolerancia."
echo

# ============================================================
# FASE 2: TIEMPOS
# ============================================================

echo "============================================"
echo " FASE 2: TIEMPOS ($EPOCAS épocas)"
echo "============================================"
echo

echo "programa,hilos,repeticion,tiempo_us" > "$RESULTADOS/tiempos.csv"

# Deja el promedio en la variable global TIEMPO_MEDIO
# (no se usa $(...) para no capturar los mensajes de progreso)
medir() {
    local prog="$1" reps="$2" suma=0 salida tiempo i
    local archivo="$RESULTADOS/${prog}.txt"
    TIEMPO_MEDIO=0

    echo "  Ejecutando $prog ($reps repetición(es))..." >&2

    for ((i=1; i<=reps; i++)); do
        salida=$(correr "$prog" "$EPOCAS" 2>&1)

        {
            echo "===== Repetición $i ====="
            echo "$salida"
            echo
        } >> "$archivo"

        tiempo=$(echo "$salida" | grep "Tiempo total" | awk '{print $3}')

        if ! [[ "$tiempo" =~ ^[0-9]+$ ]]; then
            echo "Error: no se pudo obtener el tiempo de $prog." >&2
            return 1
        fi

        echo "$prog,$(hilos_de "$prog"),$i,$tiempo" >> "$RESULTADOS/tiempos.csv"
        suma=$((suma + tiempo))
    done

    TIEMPO_MEDIO=$((suma / reps))
}

medir "main" "$REPETICIONES" || exit 1
T_SEQ=$TIEMPO_MEDIO

echo "program,hilos,tiempo_medio_us,speedup,eficiencia" |
    sed 's/program/programa/' > "$RESULTADOS/resumen.csv"

{
    printf "%-10s %-6s %-14s %-10s %-10s\n" \
        "Modelo" "Hilos" "Tiempo(us)" "Speedup" "Eficiencia"
    echo "------------------------------------------------------------"
} > "$RESULTADOS/resumen.txt"

for p in "${PROGS[@]}"; do
    if [ "$p" = "fino" ] && [ "$SIN_FINO" = "1" ]; then
        continue
    fi

    if [ "$p" = "main" ]; then
        tiempo=$T_SEQ
    else
        reps=$REPETICIONES
        [ "$p" = "fino" ] && reps=$REP_FINO
        medir "$p" "$reps" || exit 1
        tiempo=$TIEMPO_MEDIO
    fi

    hilos=$(hilos_de "$p")

    speedup=$(LC_NUMERIC=C awk -v seq="$T_SEQ" -v par="$tiempo" \
        'BEGIN { if (par > 0) printf "%.3f", seq/par; else printf "NA" }')
    eficiencia=$(LC_NUMERIC=C awk -v s="$speedup" -v p="$hilos" \
        'BEGIN { if (s != "NA") printf "%.3f", s/p; else printf "NA" }')

    printf "%-10s %-6s %-14s %-10s %-10s\n" \
        "$p" "$hilos" "$tiempo" "$speedup" "$eficiencia" >> "$RESULTADOS/resumen.txt"
    echo "$p,$hilos,$tiempo,$speedup,$eficiencia" >> "$RESULTADOS/resumen.csv"
done

echo
cat "$RESULTADOS/resumen.txt"

echo
echo "Notas:"
echo " - 'Hilos' cuenta hilos del SO. fino simula 4 tareas en 1 hilo, así que"
echo "   su eficiencia no mide paralelismo real."
echo " - grueso cuenta 2 hilos trabajadores (el principal también trabaja)."
echo " - Una sola repetición no permite hablar de variabilidad."
echo
echo "Resultados guardados en: $RESULTADOS/"