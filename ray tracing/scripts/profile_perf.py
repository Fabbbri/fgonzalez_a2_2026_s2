#!/usr/bin/env python3
# Captura e interpretación de perf stat en una campaña separada del benchmark.
# Cada fila conserva modelo, repetición, CPU, evento, PMU, unidad y cobertura.
# Los estados sin conteo permanecen explícitos y nunca se sustituyen por cero.
# El IPC se deriva exclusivamente de contadores de la misma CPU/PMU y repetición.
"""Perfilado separado del benchmark: conserva contadores crudos y su contexto."""
import argparse
import csv
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import re
import shlex
import statistics
import subprocess
import sys
from collections import Counter

# Eventos portables de partida; el soporte efectivo depende de kernel y PMU.
DEFAULT_EVENTS = "task-clock,context-switches,cpu-migrations,cycles,instructions,cache-misses"
# Umbral de aviso visual; nunca descarta por sí solo una medición numérica.
MIN_COVERAGE_PERCENT = 50.0


# Devuelve porcentaje de tiempo medido en [0,100], o None si falta o es inválido.
def coverage(row):
    try:
        value = float(row.get("porcentaje_activo", ""))
        return value if math.isfinite(value) and 0 <= value <= 100 else None
    except (ValueError, TypeError):
        return None


# Determina indisponibilidad numérica por estado; la baja cobertura no elimina datos.
def measurement_issue(row, scaled=True):
    if row["estado"] != "ok":
        return row["estado"]
    return ""


# Clasifica cobertura baja o desconocida como advertencia conservando el conteo.
def measurement_warning(row):
    """Separar disponibilidad del dato de las limitaciones de su medición."""
    value = coverage(row)
    if value is None:
        return "cobertura no disponible"
    if value < MIN_COVERAGE_PERCENT:
        return "porcentaje medido bajo"
    return ""


# Construye argumentos de perf sin shell. En modo observed agrupa eventos
# compatibles por PMU y desactiva extrapolación mediante --no-scale.
def capture_flags(events, scope, cpu_ids, count_mode, pmus=None):
    if scope == "cpu":
        events = events.replace("task-clock", "cpu-clock")
    if count_mode == "observed":
        # Agrupar ciclos/instrucciones/caché por PMU evita cocientes medidos
        # en periodos distintos. No agrupar cpu_atom con cpu_core.
        # Descompone la selección solo para eventos simples no agrupados por el usuario.
        tokens = events.split(",")
        hardware = [e for e in tokens if e in ("cycles", "instructions", "cache-misses")]
        if hardware and not any("{" in e or "}" in e for e in tokens):
            if pmus is None:
                pmus = [name for name in ("cpu_core", "cpu_atom")
                        if (Path("/sys/bus/event_source/devices") / name / "cpus").exists()]
            # Una agrupación por PMU: evita sumar dominios cpu_core y cpu_atom como uno solo.
            groups = ["{" + ",".join(f"{pmu}/{event}/" for event in hardware) + "}" for pmu in pmus]
            if not groups:
                groups = ["{" + ",".join(hardware) + "}"]
            events = ",".join([e for e in tokens if e not in hardware] + groups)
    # Salida sin separadores de miles y con punto y coma para lectura estructurada.
    flags = ["stat", "--no-big-num", "-x", ";", "-e", events]
    if count_mode == "observed":
        flags.append("--no-scale")
    if scope == "cpu":
        flags += ["-a", "-A", "-C", ",".join(map(str, cpu_ids))]
    return flags


# Interpreta el formato delimitado de perf; preserva CPU, evento y cobertura.
def parse_stat(text, scope):
    """Formato -x ';' sin -I ni -r. Mantener CPU, PMU y porcentaje de ejecución."""
    rows = []
    # Descarta cabeceras y procesa una fila de contador a la vez.
    for line in text.splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        cells = [cell.strip() for cell in line.split(";")]
        # Valor de alcance process; cpu extrae el identificador CPU cuando perf lo incluye.
        cpu = "programa"
        if scope == "cpu" and cells[0].startswith("CPU"):
            cpu = cells.pop(0)
        if len(cells) < 3 or not cells[2]:
            continue
        # Las tres primeras columnas útiles son conteo, unidad y nombre de evento.
        value, unit, event = cells[:3]
        try:
            number = float(value)
            if not math.isfinite(number) or number < 0:
                raise ValueError()
            status = "ok"
        except ValueError:
            # Un estado como <not counted> no representa el valor numérico cero.
            number, status = "", value or "sin_dato"
        rows.append(dict(cpu=cpu, evento=event, unidad=unit, valor=number,
                         estado=status, tiempo_contador=cells[3] if len(cells) > 3 else "",
                         porcentaje_activo=cells[4] if len(cells) > 4 else ""))
    return rows


# Separa la política de ejecución de los parámetros comunes de carga.
# Retira --model/--threads para construir la referencia secuencial comparable.
def configuration(arguments):
    model, common = "sequential", []
    i = 0
    while i < len(arguments):
        flag = arguments[i]
        if flag in ("--benchmark", "--output", "--metrics-output", "--repetitions",
                    "--thread-counts", "--baseline-csv", "--cpu-info", "--help", "-h"):
            raise ValueError(f"{flag} no corresponde al perfilado. Usa PROFILE_REPETITIONS y un solo P con --threads")
        if flag in ("--model", "--threads"):
            if i + 1 == len(arguments):
                raise ValueError(f"Falta valor para {flag}")
            if flag == "--model":
                model = arguments[i + 1]
            i += 2
        else:
            common.append(flag)
            i += 1
    if model not in ("sequential", "fine", "coarse", "smt", "cmp"):
        raise ValueError("Modelo desconocido")
    return model, common


# Genera registros instrucciones/ciclos con identidad de CPU y PMU conservada.
# La cobertura derivada toma el mínimo de los dos contadores disponibles.
def ipc_rows(rows):
    """Cocientes por CPU y PMU; no sumar cpu_core con cpu_atom."""
    # Índice por (CPU, evento completo), que incluye el dominio PMU del evento.
    lookup = {(row["cpu"], row["evento"]): row for row in rows}
    derived = []
    for (cpu, event), cycles in lookup.items():
        if not re.search(r"(^|/)cycles(?=[:/]|$)", event):
            continue
        # Sustituye cycles por instructions conservando prefijo PMU y modificadores.
        instructions_event = re.sub(r"(^|/)cycles(?=[:/]|$)", r"\1instructions", event)
        instructions = lookup.get((cpu, instructions_event))
        if instructions:
            valid = cycles["estado"] == instructions["estado"] == "ok" and cycles["valor"] > 0
            active = [coverage(cycles), coverage(instructions)]
            derived.append(dict(cpu=cpu, evento="IPC derivado: " + event,
                                unidad="instrucciones/ciclo", valor=instructions["valor"] / cycles["valor"] if valid else "",
                                estado="ok" if valid else "IPC no disponible", tiempo_contador="",
                                porcentaje_activo=str(min(active)) if all(v is not None for v in active) else ""))
    return derived


# Recalcula IPC por modelo y repetición para no combinar ejecuciones distintas.
def assessed_rows(rows):
    # Recalcular también IPC antiguos, cuyo CSV no conservaba la cobertura.
    # Agrupar por repetición evita emparejar contadores de distintas ejecuciones.
    raw = [dict(r) for r in rows if not r["evento"].startswith("IPC derivado:")]
    # Subconjuntos independientes por modelo y repetición para reconstruir IPC.
    batches = {}
    for row in raw:
        batches.setdefault((row["modelo"], row["repeticion"]), []).append(row)
    for (model, repetition), batch in batches.items():
        raw.extend(dict(modelo=model, repeticion=repetition, **r) for r in ipc_rows(batch))
    return raw


# Agrupa valores por modelo, CPU, evento y unidad; escribe resumen, barras y boxplots.
# Los estados no numéricos se contabilizan aparte; cobertura baja añade rayado.
def report(rows, output, scope, metadata=None):
    rows = assessed_rows(rows)
    observed = (metadata or {}).get("modo_conteo", "scaled") == "observed"
    # Adaptador local del criterio de exclusión para el modo de conteo de la captura.
    def issue(row):
        return measurement_issue(row, scaled=not observed)
    # groups contiene números válidos; records conserva también estados y advertencias.
    groups = {}
    records = {}
    for row in rows:
        # Clave de agregación: no combina CPU, evento o unidad diferentes.
        key = (row["modelo"], row["cpu"], row["evento"], row["unidad"])
        records.setdefault(key, []).append(row)
        groups.setdefault(key, [])
        if not issue(row):
            groups[key].append(row["valor"])
    lines = ["# Perfilado con perf", "", f"Alcance: **{scope}**.", "",
             "Estos contadores cubren el proceso completo, incluida inicialización y finalización. "
             "No tienen el mismo intervalo que tiempo_total_ms del renderizador; no mezclar "
             "estos tiempos con los benchmarks sin perf.", "",
             "En modo cpu se incluye todo el trabajo del sistema en las CPUs seleccionadas, "
             "no únicamente el ray tracer. Los ciclos de distintos hilos lógicos pueden "
             "solaparse sobre un mismo núcleo físico; no son tiempo de pared.", "",
             ("Conteos observados con --no-scale: no se extrapola el trabajo de cpu_atom/cpu_core "
              "al tiempo total. Un porcentaje bajo puede indicar poca residencia en esa PMU; "
              "se conserva ese conteo y su tiempo de contador. Si hubo multiplexado, los conteos "
              "son parciales y no se deben interpretar como totales completos. Los grupos por PMU "
              "buscan medir los eventos juntos; revisar siempre los archivos crudos."
              if observed else "Conteos escalados por perf: se conservan todos los valores numéricos, "
             "incluso con poco tiempo medido. Un conteo extrapolado desde una residencia mínima "
             "puede resultar enorme; no representa necesariamente trabajo observado. "
             "El IPC usa la menor cobertura de ciclos e instrucciones, sin garantizar que sus periodos coincidan."), "",
             f"Aviso visual por debajo de {MIN_COVERAGE_PERCENT:g} % medido; no es un criterio de descarte. "
             "Las medias e IC describen los valores guardados, sin corregir posibles sesgos del contador.", "",
             "| Modelo | CPU | Evento/PMU | Unidad | n usado / capturas | Media | Desviación muestral | Exclusiones y avisos |",
             "| --- | --- | --- | --- | ---: | ---: | ---: | --- |"]
    if metadata and metadata.get("referencia_compartida"):
        lines[-2:-2] = ["", "La referencia sequential se reutiliza desde " + metadata["referencia_compartida"] +
                  "; no se ejecuta otra vez para este modelo. Repeticiones originales: " +
                  str(metadata["referencia_repeticiones"]) + ". Su estado SMT puede diferir del modelo; "
                  "consultar referencia_topologia en metadata.json.", ""]
    for (model, cpu, event, unit), values in sorted(groups.items()):
        key = (model, cpu, event, unit)
        # Cuenta cada razón de exclusión sin perder la cantidad total de capturas.
        issues = Counter(issue(r) for r in records[key] if issue(r))
        exclusions = "; ".join(f"{reason}: {n}" for reason,n in issues.items()) or "ninguna"
        active = [coverage(r) for r in records[key] if not issue(r) and coverage(r) is not None]
        if active: exclusions += f"; porcentaje medido {min(active):g}–{max(active):g} %"
        warnings = Counter(measurement_warning(r) for r in records[key] if not issue(r) and measurement_warning(r))
        for warning, n in warnings.items():
            exclusions += f"; {warning}: {n} (conservadas)"
        if not values:
            lines.append(f"| {model} | {cpu} | {event} | {unit} | 0 / {len(records[key])} | — | — | {exclusions} |")
            continue
        # Desviación muestral si hay n>1; una observación no estima variabilidad.
        deviation = statistics.stdev(values) if len(values) > 1 else 0
        lines.append(f"| {model} | {cpu} | {event} | {unit} | {len(values)} / {len(records[key])} | {statistics.mean(values):.4f} | {deviation:.4f} | {exclusions} |")
    missing = [row for row in rows if issue(row)]
    lines += ["", f"Filas excluidas del resumen: {len(missing)}. Revisar exclusiones y contadores.csv.", "",
              "## Interpretación bajo SMT", "",
              "Comparar igual carga y P con SMT activado/desactivado en BIOS, conservando "
              "campañas separadas. Contrastar también CMP y sequential. Relacionar instrucciones, "
              "ciclos, caché y cambios de contexto con el tiempo y speedup del benchmark separado. "
              "IPC = instrucciones/ciclos solo para la misma CPU/PMU y alcance; no mezclar "
              "cpu_core con cpu_atom en equipos híbridos. Un IPC menor con SMT no implica "
              "por sí solo peor rendimiento si aumenta el trabajo total por segundo.", "",
              "El porcentaje_activo indica la fracción de tiempo que se ejecutó el contador. "
              "Perf puede multiplexar eventos; los conteos suelen estar escalados. Si el porcentaje "
              "es bajo, repetir con menos eventos. Los stalls y métricas top-down dependen de la "
              "microarquitectura: consultar perf list y añadir eventos compatibles, sin sustituir "
              "fallos de caché por una medición directa de stalls. El IPC derivado usa los "
              "conteos guardados: si ciclos e instrucciones no se midieron en los mismos "
              "periodos, su cociente puede ser menos fiable."]
    (output / "reporte.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
    # Mantener evento/PMU separado; cambiar solo la presentación.
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    from plot_metrics import COLORS, NAMES, decorate, save
    explanations = {
        "cycles": ("Ciclos de CPU", "Se acumulan entre trabajadores; no equivalen a tiempo de pared."),
        "instructions": ("Instrucciones completadas", "Más instrucciones pueden indicar sobrecarga; comparar con tiempo y speedup."),
        "cache-misses": ("Fallos de caché", "No indican directamente cuánto tiempo estuvo esperando la CPU."),
        "context-switches": ("Cambios de contexto", "Cambios observados por el SO; los turnos de fine no son necesariamente cambios del SO."),
        "cpu-migrations": ("Migraciones entre CPUs", "Incluye el principal, que puede migrar aunque los trabajadores estén fijados."),
        "task-clock": ("Tiempo CPU acumulado", "Puede superar el tiempo real cuando trabajan varios hilos a la vez."),
        "cpu-clock": ("Clock de las CPUs observadas", "Alcance de sistema; no es utilización exclusiva de nuestros trabajadores."),
    }
    # Una figura por evento y unidad; sus paneles mantienen identidad de CPU/PMU.
    events = sorted({(key[2], key[3]) for key in groups})
    for index, (event, unit) in enumerate(events):
        # Comando medido sin ventana; baseline sustituye la política por sequential.
        selected = [(key, values) for key, values in sorted(groups.items()) if key[2:] == (event, unit)]
        selected.sort(key=lambda item: (list(NAMES).index(item[0][0]), item[0][1]))
        canonical = event.split("/")[1] if "/" in event else event.split(":")[0]
        if event.startswith("IPC derivado"):
            title, meaning = "Instrucciones por ciclo (IPC)", "Un IPC mayor no garantiza menor tiempo de renderizado."
        else:
            title, meaning = explanations.get(canonical, (event, "Interpretar según la definición del evento en esta CPU."))
        labels = []
        warned = []
        for key, values in selected:
            command = (metadata or {}).get("comandos", {}).get(key[0], [])
            workers = "automático"
            if "--threads" in command:
                workers = command[command.index("--threads")+1]
            detail = "1 hilo real" if key[0] in ("fine", "sequential") else f"P={workers}"
            cpu = "programa completo" if key[1] == "programa" else key[1].replace("CPU", "CPU lógica ")
            numeric = [r for r in records[key] if not issue(r)]
            active = [coverage(r) for r in numeric if coverage(r) is not None]
            warning_count = sum(bool(measurement_warning(r)) for r in numeric)
            warned.append(bool(warning_count))
            context = f"medido {min(active):g}–{max(active):g} %" if active else "porcentaje no disponible"
            warning = f" · AVISO en {warning_count}" if warning_count else ""
            labels.append(f"{NAMES.get(key[0], key[0])} · {detail} · {cpu} · {len(values)}/{len(records[key])} usadas\n{context}{warning}")
        colors = [COLORS.get(key[0], "#64748b") for key, _ in selected]
        # Medias por configuración; los grupos vacíos solo reciben un marcador de ausencia.
        averages = [statistics.mean(values) if values else 0 for _, values in selected]
        # IC95 normal aproximado de la media; se muestra junto al número de observaciones.
        margins = [1.96*statistics.stdev(values)/math.sqrt(len(values)) if len(values)>1 else 0 for _, values in selected]
        scale, axis = 1, unit or "Conteo"
        if unit in ("msec", "ms"):
            scale, axis = .001, "Tiempo CPU acumulado (segundos)"
        elif not unit and max(averages) >= 1e6:
            scale, axis = 1e-6, "Conteo (millones)"
        elif not unit and max(averages) >= 1e3:
            scale, axis = .001, "Conteo (miles)"
        averages = [value * scale for value in averages]
        margins = [value * scale for value in margins]
        fig, ax = plt.subplots(figsize=(12, max(5, len(selected)*.65+2)))
        # Posiciones de grupos con medición: evita dibujar ausencia como una barra cero.
        valid_positions = [i for i, (_, values) in enumerate(selected) if values]
        bars = ax.barh(valid_positions, [averages[i] for i in valid_positions],
                       xerr=[margins[i] for i in valid_positions],
                       color=[colors[i] for i in valid_positions], height=.55, capsize=4)
        for bar, i in zip(bars, valid_positions):
            # El rayado indica cobertura limitada; el valor numérico permanece en la figura.
            if warned[i]:
                bar.set_hatch("///")
                bar.set_edgecolor("#b45309")
        ax.bar_label(bars, labels=[f"{averages[i]:.3g}" for i in valid_positions], padding=8)
        for i, (_, values) in enumerate(selected):
            if not values:
                ax.text(.02, i, "Sin medición disponible (no equivale a cero)",
                        transform=ax.get_yaxis_transform(), va="center", fontsize=9, color="#b45309")
        ax.set_yticks(range(len(selected)), labels, fontsize=9)
        ax.set_ylim(-.6, len(selected)-.4)
        ax.invert_yaxis()
        ax.set_xlim(0, max(v+m for v, m in zip(averages, margins))*1.25 or 1)
        decorate(ax, title, axis)
        note = f"Evento: {event} · Alcance: {scope} · IC95 aproximado (n > 1). " + ("Observado, sin extrapolar (--no-scale)." if observed else "Escalado por perf; puede incluir extrapolación.")
        if any(warned):
            note += "\nAVISO / rayado: poco tiempo medido (<50 %) o porcentaje desconocido; valores conservados."
        if scope == "cpu":
            note += " Incluye otras tareas del sistema."
        save(fig, output / f"evento-{index:02d}.png", note + "\n" + meaning)
        plt.close(fig)
        fig, ax = plt.subplots(figsize=(12, max(5, len(selected)*.65+2)))
        if valid_positions:
            boxes = ax.boxplot([[v*scale for v in selected[i][1]] for i in valid_positions],
                               positions=[i+1 for i in valid_positions], vert=False, patch_artist=True)
            for box, i in zip(boxes["boxes"], valid_positions):
                box.set_facecolor(colors[i])
                box.set_alpha(.65)
                if warned[i]:
                    box.set_hatch("///")
                    box.set_edgecolor("#b45309")
        for i, (_, values) in enumerate(selected):
            if not values:
                ax.text(.02, i+1, "Sin medición disponible (no equivale a cero)",
                        transform=ax.get_yaxis_transform(), va="center", fontsize=9, color="#b45309")
        ax.set_ylim(.5, len(selected)+.5)
        ax.set_yticks(range(1,len(selected)+1), labels, fontsize=9)
        ax.invert_yaxis()
        decorate(ax, title + " · variabilidad", axis)
        save(fig, output / f"variabilidad-evento-{index:02d}.png", note + "\nCaja: mitad central · línea: mediana · puntos: valores atípicos (se conservan).")
        plt.close(fig)


# Ejecuta perf y conserva salidas según política de logs; los errores siempre dejan diagnóstico.
def capture_process(command, stem, env, keep_logs=False):
    """Omitir logs de ejecuciones correctas; conservar diagnóstico si falla."""
    if keep_logs:
        with stem.with_suffix(".stdout.txt").open("w") as stdout, stem.with_suffix(".stderr.txt").open("w") as stderr:
            return subprocess.run(command, stdout=stdout, stderr=stderr, env=env)
    result = subprocess.run(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, text=True, env=env)
    if result.returncode or result.stderr:
        # No perder errores ni avisos de perf aunque los logs estén desactivados.
        stem.with_suffix(".stderr.txt").write_text(result.stderr or "Proceso finalizado con error sin diagnóstico.\n", encoding="utf-8")
    return result


# Valida campaña, sondea acceso a eventos, obtiene metadatos, calienta y captura
# cada repetición. El reporte y las figuras se generan después de medir.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--perf", default="perf", help="También acepta 'sudo perf'")
    parser.add_argument("--events", default=DEFAULT_EVENTS)
    parser.add_argument("--scope", choices=("process", "cpu"), default="process")
    parser.add_argument("--count-mode", choices=("observed", "scaled"), default="observed")
    parser.add_argument("--logs", choices=("0", "1"), default="0",
                        help="1 conserva stdout/stderr por ejecución; 0 solo diagnósticos (predeterminado)")
    parser.add_argument("--repetitions", type=int, default=5)
    parser.add_argument("--output-dir", type=Path, default=Path("results/perf"))
    parser.add_argument("--fixed-output", action="store_true",
                        help="Reutilizar output-dir y reemplazar sus capturas generadas (run-all)")
    parser.add_argument("--baseline-capture", type=Path,
                        help="Reutilizar una captura sequential de perf, sin volver a ejecutarlo")
    parser.add_argument("binary")
    parser.add_argument("args", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    try:
        if options.repetitions < 1 or not options.events.strip():
            raise ValueError("Se necesitan repeticiones positivas y eventos")
        import matplotlib  # noqa: F401
        model, common = configuration(options.args)
        baseline_rows, baseline_metadata = [], None
        if options.baseline_capture:
            if model == "sequential" or options.baseline_capture.resolve() == options.output_dir.resolve():
                raise ValueError("La captura de referencia debe ser distinta del modelo y su salida")
            baseline_metadata = json.loads((options.baseline_capture / "metadata.json").read_text())
            if (baseline_metadata.get("carga_argumentos") != common or
                baseline_metadata["alcance"] != options.scope or
                baseline_metadata.get("modo_conteo", "scaled") != options.count_mode or
                baseline_metadata["eventos"] != (options.events.replace("task-clock", "cpu-clock") if options.scope == "cpu" else options.events)):
                raise ValueError("Referencia perf con carga, alcance, modo de conteo o eventos diferentes")
            with (options.baseline_capture / "contadores.csv").open(newline="") as stream:
                baseline_rows = [r for r in csv.DictReader(stream) if r["modelo"] == "sequential"]
            for row in baseline_rows:
                row["repeticion"] = int(row["repeticion"])
                if row["estado"] == "ok": row["valor"] = float(row["valor"])
            if not baseline_rows:
                raise ValueError("Captura perf sin referencia sequential")
        # Fija configuración regional C para obtener números interpretables de forma uniforme.
        env = dict(os.environ, LC_ALL="C")
        # Separa comandos como sudo perf en argumentos, sin evaluar código de shell.
        perf = shlex.split(options.perf)
        if not perf:
            raise ValueError("Comando perf vacío")
        cpu_info = subprocess.run([options.binary, "--cpu-info"], capture_output=True, text=True, env=env, check=True).stdout
        # Extrae destinos lógicos de la topología del ejecutable, incluidos hermanos SMT.
        cpu_ids = sorted({int(cpu) for match in re.findall(r"nucleo=\d+ cpus=([\d;]+)", cpu_info)
                          for cpu in match.split(";")})
        if options.scope == "cpu" and not cpu_ids:
            raise ValueError("No se pudo determinar las CPUs lógicas a perfilar")
        events = options.events.replace("task-clock", "cpu-clock") if options.scope == "cpu" else options.events
        flags = capture_flags(options.events, options.scope, cpu_ids, options.count_mode)
        # No renderizar antes de saber si Linux permite usar los contadores.
        # Sondeo con true: verifica permisos y eventos antes del costo de los renders.
        probe = subprocess.run([*perf, *flags, "--", "true"], capture_output=True, text=True, env=env)
        if probe.returncode:
            raise ValueError("perf no pudo abrir los eventos.\n" + probe.stderr +
                             "No se cambian permisos del sistema. Consulta README: perfilado con perf.")
        output = options.output_dir if options.fixed_output else options.output_dir / (model + "-" + datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S%fZ"))
        output.mkdir(parents=True, exist_ok=options.fixed_output)
        if options.fixed_output:
            # Retirar solo salidas propias, también si ahora hay menos repeticiones
            # o eventos. No borrar otros archivos del usuario en la carpeta.
            for path in output.iterdir():
                capture = re.fullmatch(r"(?:sequential|fine|coarse|smt|cmp)-\d+\.(?:perf\.csv|stdout\.txt|stderr\.txt)", path.name)
                plot = re.fullmatch(r"(?:variabilidad-)?evento-\d+\.png", path.name)
                if path.is_file() and (capture or plot or path.name in ("metadata.json", "contadores.csv", "reporte.md")):
                    path.unlink()
        selected = [options.binary, *options.args, "--headless"]
        baseline = [options.binary, *common, "--model", "sequential", "--headless"]
        commands = {model: selected} if options.baseline_capture else {"sequential": baseline, model: selected}
        # Registra procedencia: fecha UTC, comandos, sistema, CPUs y modo de contador.
        metadata = dict(fecha_utc=datetime.now(timezone.utc).isoformat(), alcance=options.scope,
                        repeticiones=options.repetitions, eventos=events, comandos=commands,
                        topologia=cpu_info, cpus_observadas=cpu_ids,
                        sistema=dict(zip(("sysname", "nodename", "release", "version", "machine"), os.uname())),
                        perf_version=subprocess.run([*perf, "--version"], capture_output=True, text=True, env=env).stdout.strip(),
                        carga_argumentos=common, modo_conteo=options.count_mode, flags_perf=flags,
                        logs_detallados=options.logs == "1")
        if options.baseline_capture:
            metadata["referencia_compartida"] = os.path.relpath(options.baseline_capture, output)
            metadata["referencia_topologia"] = baseline_metadata["topologia"]
            metadata["referencia_repeticiones"] = baseline_metadata["repeticiones"]
        cpu_text = Path("/proc/cpuinfo").read_text()
        metadata["modelos_cpu"] = sorted(set(re.findall(r"^model name\s*:\s*(.+)$", cpu_text, re.MULTILINE)))
        metadata["perf_event_paranoid"] = Path("/proc/sys/kernel/perf_event_paranoid").read_text().strip()
        (output / "metadata.json").write_text(json.dumps(metadata, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
        # Inicia con referencia importada cuando existe; nunca inventa nuevas observaciones.
        rows = list(baseline_rows)
        # Calentamiento sin perf, excluido de las filas exportadas.
        for command in commands.values():
            subprocess.run(command, stdout=subprocess.DEVNULL, env=env, check=True)  # Calentamiento sin perf.
        # Cada repetición conserva su identidad para emparejar contadores y calcular IPC.
        for repetition in range(1, options.repetitions + 1):
            order = list(commands.items())
            # Alterna orden modelo/referencia para reducir efectos sistemáticos de ejecución.
            if repetition % 2 == 0:
                order.reverse()
            for name, command in order:
                stem = output / f"{name}-{repetition:03d}"
                raw = stem.with_suffix(".perf.csv")
                raw.touch()  # El usuario conserva propiedad incluso con PERF='sudo perf'.
                result = capture_process([*perf, *flags, "-o", str(raw), "--", *command],
                                         stem, env, options.logs == "1")
                if result.returncode:
                    raise ValueError(f"Falló perf/{name}; captura parcial en {output}. Revisa stderr; no se presenta como campaña completa")
                measured = parse_stat(raw.read_text(), options.scope)
                if not measured:
                    raise ValueError(f"No hay contadores interpretables; conserva el archivo crudo: {raw}")
                if not any(row["estado"] == "ok" for row in measured):
                    raise ValueError(f"Ningún contador tiene una medición válida. Revisa {raw}")
                # Añade IPC derivado tras interpretar y validar los eventos originales.
                measured += ipc_rows(measured)
                rows += [dict(modelo=name, repeticion=repetition, **row) for row in measured]
                print(f"perfilado={name} repeticion={repetition}/{options.repetitions}", flush=True)
        # Exporta todas las filas antes de generar resumen y presentación.
        with (output / "contadores.csv").open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=list(rows[0]))
            writer.writeheader()
            writer.writerows(rows)
        report(rows, output, options.scope, metadata)
        print(f"Capturas, CSV, reporte y gráficas: {output}")
    except (ImportError, OSError, ValueError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
