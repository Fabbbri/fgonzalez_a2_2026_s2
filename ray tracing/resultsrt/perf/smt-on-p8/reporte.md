# Perfilado con perf

Alcance: **process**.

Estos contadores cubren el proceso completo, incluida inicialización y finalización. No tienen el mismo intervalo que tiempo_total_ms del renderizador; no mezclar estos tiempos con los benchmarks sin perf.

En modo cpu se incluye todo el trabajo del sistema en las CPUs seleccionadas, no únicamente el ray tracer. Los ciclos de distintos hilos lógicos pueden solaparse sobre un mismo núcleo físico; no son tiempo de pared.

Conteos escalados por perf: se conservan todos los valores numéricos, incluso con poco tiempo medido. Un conteo extrapolado desde una residencia mínima puede resultar enorme; no representa necesariamente trabajo observado. El IPC usa la menor cobertura de ciclos e instrucciones, sin garantizar que sus periodos coincidan.

Aviso visual por debajo de 50 % medido; no es un criterio de descarte. Las medias e IC describen los valores guardados, sin corregir posibles sesgos del contador.


La referencia sequential se reutiliza desde ../sequential; no se ejecuta otra vez para este modelo. Repeticiones originales: 20. Su estado SMT puede diferir del modelo; consultar referencia_topologia en metadata.json.

| Modelo | CPU | Evento/PMU | Unidad | n usado / capturas | Media | Desviación muestral | Exclusiones y avisos |
| --- | --- | --- | --- | ---: | ---: | ---: | --- |
| sequential | programa | IPC derivado: cpu_atom/cycles/ | instrucciones/ciclo | 17 / 20 | 1.4416 | 0.8480 | IPC no disponible: 3; porcentaje medido 0–0 %; porcentaje medido bajo: 17 (conservadas) |
| sequential | programa | IPC derivado: cpu_core/cycles/ | instrucciones/ciclo | 20 / 20 | 4.4927 | 0.0339 | ninguna; porcentaje medido 99–100 % |
| sequential | programa | context-switches |  | 20 / 20 | 40.0000 | 26.0061 | ninguna; porcentaje medido 100–100 % |
| sequential | programa | cpu-migrations |  | 20 / 20 | 2.0000 | 2.4709 | ninguna; porcentaje medido 100–100 % |
| sequential | programa | cpu_atom/cache-misses/ |  | 17 / 20 | 29870531.1765 | 14132366.1922 | <not counted>: 3; porcentaje medido 0–0 %; porcentaje medido bajo: 17 (conservadas) |
| sequential | programa | cpu_atom/cycles/ |  | 17 / 20 | 3665827019.2941 | 91079608.4768 | <not counted>: 3; porcentaje medido 0–0 %; porcentaje medido bajo: 17 (conservadas) |
| sequential | programa | cpu_atom/instructions/ |  | 17 / 20 | 5249390359.2353 | 2988104395.3026 | <not counted>: 3; porcentaje medido 0–0 %; porcentaje medido bajo: 17 (conservadas) |
| sequential | programa | cpu_core/cache-misses/ |  | 20 / 20 | 132716.9500 | 65948.8145 | ninguna; porcentaje medido 99–100 % |
| sequential | programa | cpu_core/cycles/ |  | 20 / 20 | 4867126680.0500 | 32588494.2827 | ninguna; porcentaje medido 99–100 % |
| sequential | programa | cpu_core/instructions/ |  | 20 / 20 | 21865348163.0500 | 36824322.5639 | ninguna; porcentaje medido 99–100 % |
| sequential | programa | task-clock |  | 20 / 20 | 1130425827.1500 | 21950699.9673 | ninguna; porcentaje medido 100–100 % |
| smt | programa | IPC derivado: cpu_atom/cycles/ | instrucciones/ciclo | 20 / 20 | 3.2809 | 0.0066 | ninguna; porcentaje medido 79–99 % |
| smt | programa | IPC derivado: cpu_core/cycles/ | instrucciones/ciclo | 20 / 20 | 4.4381 | 0.0636 | ninguna; porcentaje medido 79–99 % |
| smt | programa | context-switches |  | 20 / 20 | 82.5000 | 8.8228 | ninguna; porcentaje medido 100–100 % |
| smt | programa | cpu-migrations |  | 20 / 20 | 10.0500 | 2.1637 | ninguna; porcentaje medido 100–100 % |
| smt | programa | cpu_atom/cache-misses/ |  | 20 / 20 | 68540.9000 | 72794.3218 | ninguna; porcentaje medido 79–99 % |
| smt | programa | cpu_atom/cycles/ |  | 20 / 20 | 2583066508.4500 | 266552010.7211 | ninguna; porcentaje medido 79–99 % |
| smt | programa | cpu_atom/instructions/ |  | 20 / 20 | 8474693914.8000 | 872515609.8848 | ninguna; porcentaje medido 79–99 % |
| smt | programa | cpu_core/cache-misses/ |  | 20 / 20 | 158595.4500 | 72166.9981 | ninguna; porcentaje medido 79–99 % |
| smt | programa | cpu_core/cycles/ |  | 20 / 20 | 3326410620.0500 | 357126539.1143 | ninguna; porcentaje medido 79–99 % |
| smt | programa | cpu_core/instructions/ |  | 20 / 20 | 14754127110.4500 | 1501275366.2787 | ninguna; porcentaje medido 79–99 % |
| smt | programa | task-clock |  | 20 / 20 | 1870684762.5500 | 26683569.4550 | ninguna; porcentaje medido 100–100 % |

Filas excluidas del resumen: 12. Revisar exclusiones y contadores.csv.

## Interpretación bajo SMT

Comparar igual carga y P con SMT activado/desactivado en BIOS, conservando campañas separadas. Contrastar también CMP y sequential. Relacionar instrucciones, ciclos, caché y cambios de contexto con el tiempo y speedup del benchmark separado. IPC = instrucciones/ciclos solo para la misma CPU/PMU y alcance; no mezclar cpu_core con cpu_atom en equipos híbridos. Un IPC menor con SMT no implica por sí solo peor rendimiento si aumenta el trabajo total por segundo.

El porcentaje_activo indica la fracción de tiempo que se ejecutó el contador. Perf puede multiplexar eventos; los conteos suelen estar escalados. Si el porcentaje es bajo, repetir con menos eventos. Los stalls y métricas top-down dependen de la microarquitectura: consultar perf list y añadir eventos compatibles, sin sustituir fallos de caché por una medición directa de stalls. El IPC derivado usa los conteos guardados: si ciclos e instrucciones no se midieron en los mismos periodos, su cociente puede ser menos fiable.
