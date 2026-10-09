#!/bin/bash

PROGRAMAS=(
    "main"
    "fino"
    "grueso"
    "smt"
    "cmp"
)

REPETICIONES=10
HILOS=2

# ============================================
# Medir un programa
# ============================================

medir() {
    local programa="$1"
    local suma=0

    for ((i=1; i<=REPETICIONES; i++))
    do
        tiempo=$(./"$programa" | grep "Tiempo total" | awk '{print $3}')
        suma=$((suma + tiempo))
    done

    echo $((suma / REPETICIONES))
}

# ============================================
# Medición secuencial
# ============================================

echo "============================================"
echo "        MÉTRICAS DE LOS MODELOS"
echo "============================================"

echo
echo "Midiendo versión secuencial..."

T_SEQ=$(medir "main")

echo "Tiempo secuencial: $T_SEQ us"

# ============================================
# Métricas
# ============================================

echo
printf "%-15s %-15s %-15s %-15s\n" \
    "Modelo" "Tiempo(us)" "Speedup" "Eficiencia"

echo "------------------------------------------------------------"

for programa in "${PROGRAMAS[@]}"
do

    if [ "$programa" = "main" ]; then

        tiempo=$T_SEQ
        speedup="1.000"
        eficiencia="1.000"

    else

        tiempo=$(medir "$programa")

        speedup=$(LC_NUMERIC=C awk -v seq="$T_SEQ" -v par="$tiempo" \
            'BEGIN {printf "%.3f", seq/par}')

        eficiencia=$(LC_NUMERIC=C awk -v s="$speedup" -v p="$HILOS" \
            'BEGIN {printf "%.3f", s/p}')
    fi

    printf "%-15s %-15s %-15s %-15s\n" \
        "$programa" "$tiempo" "$speedup" "$eficiencia"
done