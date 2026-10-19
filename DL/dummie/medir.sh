#!/bin/bash

# 1. Comprobar si se pasó al menos un parámetro
if [ -z "$1" ]; then
    echo "Error: Debes proporcionar el nombre de un ejecutable."
    echo "Uso: $0 <nombre_del_ejecutable>"
    exit 1
fi

# 2. Asignar el primer parámetro a una variable con nombre claro
EJECUTABLE="$1"

# 3. Validar si el archivo existe y es ejecutable
if [ ! -x "$EJECUTABLE" ]; then
    echo "Error: '$EJECUTABLE' no existe o no tiene permisos de ejecución."
    exit 1
fi

# 4. Ejecutar el programa pasándole el resto de los argumentos que reciba el script
echo "Iniciando ejecución de $EJECUTABLE..."

times=()

for i in {1..10}
do
    time=$(./"$EJECUTABLE" | grep "Tiempo total" | awk '{print $3}')
    times+=("$time")
done

echo "Tiempos:"
printf '%s us\n' "${times[@]}"

sum=0
min=${times[0]}
max=${times[0]}

for time in "${times[@]}"
do
    ((sum += time))

    (( time < min )) && min=$time
    (( time > max )) && max=$time
done

average=$((sum / ${#times[@]}))

echo
echo "=== Resumen ==="
echo "Promedio: ${average} us"
echo "Mínimo:   ${min} us"
echo "Máximo:   ${max} us"