#!/usr/bin/env python3
"""Campañas perf comunes: CSV largos, evidencia cruda, reporte y SVG sin dependencias."""
import argparse
import csv
from datetime import datetime, timezone
from html import escape
import json
import math
import os
from pathlib import Path
import platform
import shlex
import shutil
import statistics
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "perf"
MODELS = ("sequential", "fine", "coarse", "smt", "cmp")
FIELDS = ("campana", "problema", "modelo", "repeticion", "trabajadores", "alcance",
          "cpu", "evento", "unidad", "valor", "estado", "tiempo_contador_ns",
          "porcentaje_activo", "pared_segundos", "smt_activo", "comando", "evidencia")
DEFAULT_EVENTS = ("task-clock,context-switches,cpu-migrations,page-faults,cycles,"
                  "instructions,cache-references,cache-misses,"
                  "stalled-cycles-frontend,stalled-cycles-backend")


def positive(value):
    result = int(value)
    if result < 1:
        raise ValueError("Se requiere un entero positivo")
    return result


def topology():
    cpus = sorted(os.sched_getaffinity(0))
    groups = {}
    for cpu in cpus:
        base = Path(f"/sys/devices/system/cpu/cpu{cpu}/topology")
        key = tuple((base / name).read_text().strip()
                    for name in ("physical_package_id", "core_id"))
        groups.setdefault(key, []).append(cpu)
    active = Path("/sys/devices/system/cpu/smt/active")
    return {"cpus": cpus, "cores": list(groups.values()),
            "smt_active": active.read_text().strip() if active.exists() else "unknown"}


def parse_stat(raw):
    """perf stat -x ';', sin -r/-I; preservar PMUs híbridas y contadores ausentes."""
    rows = []
    for line in raw.splitlines():
        cells = [cell.strip() for cell in line.split(";")]
        if line.lstrip().startswith("#"):
            continue
        cpu = "proceso"
        if cells and cells[0].startswith("CPU"):
            cpu = cells.pop(0)
        if len(cells) < 3 or not cells[2]:
            continue
        value, unit, event = cells[:3]
        try:
            value = float(value)
            if not math.isfinite(value) or value < 0:
                raise ValueError()
            status = "ok"
        except ValueError:
            status, value = value or "sin_dato", ""
        rows.append(dict(cpu=cpu, evento=event, unidad=unit, valor=value, estado=status,
                         tiempo_contador_ns=cells[3] if len(cells) > 3 else "",
                         porcentaje_activo=cells[4] if len(cells) > 4 else ""))
    return rows


def flags(scope, topo):
    return ["-a", "-A", "-C", ",".join(map(str, topo["cpus"]))] if scope == "cpu" else []


def preflight(perf, scope, topo, folder):
    if not shutil.which(perf):
        raise ValueError(f"No se encuentra {perf}")
    requested = list(dict.fromkeys(os.environ.get("EVENTS", DEFAULT_EVENTS).split(",")))
    if not all(event.strip() for event in requested):
        raise ValueError("EVENTS debe contener eventos separados por comas")
    accepted, unavailable = [], []
    for index, event in enumerate(requested):
        event = event.strip()
        if scope == "cpu" and event == "task-clock":
            event = "cpu-clock"
        path = folder / f"preflight-{index}.txt"
        result = subprocess.run([perf, "stat", "--no-big-num", "-x", ";", *flags(scope, topo),
                                 "-e", event, "-o", str(path), "--", "true"],
                                capture_output=True, text=True, env={**os.environ, "LC_ALL": "C"})
        (folder / f"preflight-{index}.log").write_text(result.stderr)
        raw = path.read_text() if path.exists() else ""
        # Permisos son un fallo de campaña, no un contador cero ni no soportado.
        diagnostic = raw + result.stderr
        if any(word in diagnostic.lower() for word in
               ("permission", "access to performance", "operation not permitted", "no permission")):
            raise ValueError(f"perf no tiene permiso para {event}. Diagnóstico: {folder}\n{diagnostic}")
        rows = parse_stat(raw)
        if result.returncode or not rows or all(r["estado"] == "<not supported>" for r in rows):
            unavailable.append(event)
        else:
            accepted.append(event)
    if not accepted:
        raise ValueError(f"Ningún evento disponible; revise {folder} y perf list")
    return accepted, unavailable


def arguments(name, fallback):
    return shlex.split(os.environ.get(name, fallback))


def command(problem, model, workers, folder):
    env = {**os.environ, "LC_ALL": "C"}
    mapped = {"fine": "fine-grained", "coarse": "coarse-grained"}.get(model, model)
    if problem == "nbody":
        cwd = ROOT / "n-body-sim"
        cmd = [str(cwd / "build/nbody"), *arguments("NBODY_ARGS", "--bodies-per-galaxy 500 --steps 1000 --dt 0.002 --seed 42"),
               "--model", mapped, "--no-output", "--trajectory", str(folder / "trajectory.nbody")]
        if model == "fine":
            cmd += ["--quantum", os.environ.get("NBODY_QUANTUM", "1")]
    elif problem == "ray":
        cwd = ROOT / "ray tracing"
        cmd = [str(cwd / "build/headless/raytracer"), *arguments("RAY_ARGS", "--width 640 --height 480 --samples 8 --bounces 12 --tile-size 32"),
               "--model", model, "--headless"]
        if model == "fine":
            cmd += ["--quantum", os.environ.get("RAY_QUANTUM", "64")]
    elif problem == "sentinel":
        cwd = ROOT / "sentinel1-SLC"
        inputs = [Path(os.environ.get(key, f"sentinel1-SLC/data/coregistered/P09-R2-2023-2024/{letter}.tif"))
                  for key, letter in (("SLC_A", "A"), ("SLC_B", "B"))]
        inputs = [path if path.is_absolute() else ROOT / path for path in inputs]
        for path in inputs:
            if not path.is_file():
                raise ValueError(f"Falta el TIFF Sentinel: {path}. Configure SLC_A y SLC_B desde la raíz.")
        cmd = [str(cwd / "build/interferogram"), "--a", str(inputs[0]), "--b", str(inputs[1]),
               *arguments("SENTINEL_ARGS", "--width 24444 --height 1499 --tile-width 32 --tile-height 32 --allow-nodata"), "--mode", mapped]
        if model == "fine":
            cmd += ["--quantum", os.environ.get("SENTINEL_QUANTUM", "2")]
    else:
        cwd = ROOT / "DL/src"
        for filename in ("train-images-idx3-ubyte", "train-labels-idx1-ubyte"):
            if not (cwd.parent / "data/mnist" / filename).is_file():
                raise ValueError(f"Falta MNIST: {filename}")
        binary = dict(sequential="main", fine="fino", coarse="grueso", smt="smt.bin", cmp="cmp")[model]
        cmd = [str(cwd / binary)]
        epochs = str(positive(os.environ.get("DL_EPOCHS", "1000")))
        if model in ("sequential", "fine"):
            cmd += [epochs]
        elif model == "coarse":
            cmd += [epochs, str(workers), os.environ.get("BLOCK_INTERVAL_MS", "10"), os.environ.get("BLOCK_DURATION_MS", "1")]
        elif model == "cmp":
            cmd += [str(workers), epochs]
        else:
            topo = topology()
            order = [group[level] for level in range(max(map(len, topo["cores"])))
                     for group in topo["cores"] if level < len(group)]
            cmd += [str(workers), ",".join(str(order[i % len(order)]) for i in range(workers)), epochs]
        env.update(DL_QUIET="1", DL_SEED=os.environ.get("DL_SEED", "1"),
                   DL_SAMPLES=os.environ.get("DL_SAMPLES", "100"), DL_QUANTUM_US=os.environ.get("DL_QUANTUM_US", "100"))
    if problem != "dl":
        if model in ("coarse", "smt", "cmp"):
            cmd += ["--threads", str(workers)]
        if model == "coarse" and problem != "sentinel":
            cmd += ["--block-interval-ms", os.environ.get("BLOCK_INTERVAL_MS", "10"),
                    "--block-duration-ms", os.environ.get("BLOCK_DURATION_MS", "1")]
    if not Path(cmd[0]).is_file():
        raise ValueError(f"Falta compilar {cmd[0]}; use make build-{problem}")
    return cmd, cwd, env


def capture(problem, perf, scope, topo, folder, events, unavailable):
    models = os.environ.get("MODELS", " ".join(MODELS)).split()
    if not models or any(model not in MODELS for model in models):
        raise ValueError(f"MODELS admite: {' '.join(MODELS)}")
    normal = os.environ.get("THREADS", "auto")
    normal = min(4, len(topo["cores"])) if normal == "auto" else positive(normal)
    smt = os.environ.get("SMT_THREADS", "auto")
    smt = len(topo["cpus"]) if smt == "auto" else positive(smt)
    if "cmp" in models and normal > len(topo["cores"]):
        raise ValueError("THREADS excede los núcleos físicos permitidos para CMP")
    jobs = [(model, 1 if model in ("sequential", "fine") else smt if model == "smt" else normal)
            for model in models]
    # Resolver toda la configuración antes de reemplazar un CSV anterior.
    commands = [(model, workers, command(problem, model, workers, folder)) for model, workers in jobs]
    repetitions = positive(os.environ.get("REPETITIONS", "3"))
    target = OUTPUT / f"{problem}.csv"
    with target.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        stream.flush()
        for model, workers, (cmd, cwd, env) in commands:
            for repetition in range(1, repetitions + 1):
                if topology() != topo:
                    raise ValueError("Cambió la topología, afinidad o estado SMT")
                stem = folder / f"{model}-{repetition:03d}"
                raw = stem.with_suffix(".stat")
                full = [perf, "stat", "--no-big-num", "-x", ";", *flags(scope, topo),
                        "-e", ",".join(events), "-o", str(raw), "--", *cmd]
                stem.with_suffix(".json").write_text(json.dumps({"command": full, "cwd": str(cwd),
                    "env": {key: value for key, value in env.items() if key.startswith("DL_")}}, indent=2))
                print(f"{problem}: {model}, P={workers}, {repetition}/{repetitions}", flush=True)
                start = time.perf_counter()
                with stem.with_suffix(".stdout").open("w") as out, stem.with_suffix(".stderr").open("w") as err:
                    result = subprocess.run(full, cwd=cwd, env=env, stdout=out, stderr=err)
                wall = time.perf_counter() - start
                if result.returncode:
                    raise ValueError(f"Falló {problem}/{model}: consulte {stem}.stderr y {raw}")
                if topology() != topo:
                    raise ValueError("Cambió la topología durante la medición")
                rows = parse_stat(raw.read_text())
                if not rows:
                    raise ValueError(f"perf no produjo contadores: {raw}")
                rows += [dict(cpu="no_disponible", evento=event, unidad="", valor="", estado="no_soportado")
                         for event in unavailable]
                base = dict(campana=folder.name, problema=problem, modelo=model, repeticion=repetition,
                            trabajadores=workers, alcance=scope, pared_segundos=wall,
                            smt_activo=topo["smt_active"], comando=shlex.join(cmd), evidencia=str(raw.relative_to(ROOT)))
                for row in rows:
                    writer.writerow({**base, **row})
                stream.flush()


def svg_chart(path, title, values):
    """Una barra por modelo/CPU; escalas separadas para cada evento/PMU."""
    width, height = 960, 80 + 30 * len(values)
    maximum = max((v for _, v in values), default=0) or 1
    pieces = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" role="img">',
              '<rect width="100%" height="100%" fill="white"/>',
              f'<text x="20" y="28" font-family="sans-serif" font-size="16">{escape(title)}</text>']
    for index, (label, value) in enumerate(values):
        y = 55 + index * 30
        pieces += [f'<text x="12" y="{y+15}" font-family="sans-serif" font-size="12">{escape(label)}</text>',
                   f'<rect x="240" y="{y}" width="{560*value/maximum:.2f}" height="20" fill="#287ba5"/>',
                   f'<text x="810" y="{y+15}" font-family="sans-serif" font-size="12">{value:.6g}</text>']
    path.write_text("\n".join([*pieces, "</svg>"]))


def report():
    lines = ["# Perfilado de los modelos con perf", "",
             "Contadores del proceso completo (incluye carga, inicialización, cálculo, salida y cierre). "
             "No tienen el mismo alcance que los tiempos internos de los benchmarks. Pared incluye el lanzamiento de perf. "
             "Cada CSV corresponde a la última campaña de ese problema; las campañas crudas permanecen en raw/.", "",
             "En alcance process se heredan los hilos del programa. En alcance cpu se mide toda la actividad "
             "del sistema en cada CPU lógica permitida, incluido trabajo ajeno al programa. cpu-clock no demuestra "
             "ocupación exclusiva del programa; para atribución de uso de CPU use alcance process.", "",
             "perf escala los contadores multiplexados. Se conserva porcentaje_activo y tiempo_contador_ns; "
             "la cobertura baja limita la comparación. Las PMUs cpu_core/cpu_atom se muestran separadas. "
             "Los eventos no soportados o no contados no se sustituyen por cero. No se calcula IPC entre PMUs "
             "ni eventos medidos en intervalos potencialmente distintos.", "",
             "## Relación con los modelos", "",
             "- Secuencial: referencia de instrucciones, ciclos y tiempo de CPU para la misma carga.",
             "- Fine-grained: planificación cooperativa en un hilo real. El quantum añade trabajo del planificador; "
             "compare instrucciones y ciclos sin interpretar sus tareas como CPUs utilizadas.",
             "- Coarse-grained: trabajadores reales y sincronización. En N-body, ray tracing y DL hay esperas "
             "artificiales configurables; en Sentinel el reparto es por tiles sin esas pausas. Context switches "
             "y tiempo de CPU ayudan a discutir esperas; DL reparte las actualizaciones y conserva forward/backprop secuenciales.",
             "- SMT: afinidad a CPUs lógicas, primero una por núcleo y luego siblings. Registrar P y smt_activo: "
             "P menor o igual a los núcleos físicos no prueba uso de siblings. Compartir recursos puede cambiar ciclos, "
             "stalls y fallos de caché. Esta campaña conserva el estado SMT del sistema.",
             "- CMP: un trabajador por núcleo físico seleccionado. Compare reducción del tiempo de pared con "
             "tiempo de CPU agregado y costo de sincronización; los ciclos acumulados no son tiempo de pared.", "",
             "Stalls frontend/backend son eventos dependientes del procesador; su ausencia no significa cero stalls. "
             "Estas relaciones orientan la discusión; los contadores por sí solos no prueban causalidad."]
    plots = OUTPUT / "plots"
    plots.mkdir(exist_ok=True)
    for source in sorted(OUTPUT.glob("*.csv")):
        with source.open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        if not rows:
            continue
        groups = {}
        for row in rows:
            groups.setdefault((row["modelo"], row["cpu"], row["evento"], row["unidad"]), []).append(row)
        lines += ["", f"## {source.stem}", "",
                  f"Fuente: [{source.name}]({source.name}). Campaña: `{rows[0]['campana']}`. Alcance: `{rows[0]['alcance']}`.", "",
                  "| Modelo | CPU | Evento / PMU | Unidad | n válido / capturas | Media | Desviación | Cobertura mínima (%) |",
                  "| --- | --- | --- | --- | ---: | ---: | ---: | ---: |"]
        charts = {}
        for (model, cpu, event, unit), batch in groups.items():
            valid = [float(r["valor"]) for r in batch if r["estado"] == "ok"]
            coverage = [float(r["porcentaje_activo"]) for r in batch if r["estado"] == "ok" and r["porcentaje_activo"]]
            mean = f"{statistics.mean(valid):.6g}" if valid else "no disponible"
            sd = f"{statistics.stdev(valid):.6g}" if len(valid) > 1 else "—"
            lines.append(f"| {model} | {cpu} | {event} | {unit} | {len(valid)}/{len(batch)} | {mean} | {sd} | {min(coverage) if coverage else '—'} |")
            if valid:
                charts.setdefault((event, unit), []).append((f"{model} · {cpu}", statistics.mean(valid)))
        lines += ["", "Promedios de pared y uso agregado del proceso:", "",
                  "| Modelo | P | Pared media (s) | CPUs lógicas equivalentes ocupadas |", "| --- | ---: | ---: | ---: |"]
        for model in dict.fromkeys(r["modelo"] for r in rows):
            batch = [r for r in rows if r["modelo"] == model]
            runs = {r["repeticion"]: float(r["pared_segundos"]) for r in batch}
            utilization = []
            for r in batch:
                if r["alcance"] == "process" and r["evento"] == "task-clock" and r["estado"] == "ok":
                    scale = {"msec": 1e-3, "ms": 1e-3, "nsec": 1e-9, "ns": 1e-9, "sec": 1}.get(r["unidad"])
                    if scale:
                        utilization.append(float(r["valor"]) * scale / runs[r["repeticion"]])
            use = f"{statistics.mean(utilization):.3f}" if utilization else "no disponible"
            lines.append(f"| {model} | {batch[0]['trabajadores']} | {statistics.mean(runs.values()):.6g} | {use} |")
        wall_values = [(model, statistics.mean({r["repeticion"]: float(r["pared_segundos"]) for r in rows if r["modelo"] == model}.values()))
                       for model in dict.fromkeys(r["modelo"] for r in rows)]
        baseline = dict(wall_values).get("sequential")
        if baseline:
            lines += ["", "Comparación observada de pared (instrumentada por perf): " + "; ".join(
                f"{model}: {baseline / value:.3f}× respecto al secuencial"
                for model, value in wall_values if model != "sequential") + ". "
                "Un cociente menor que 1 indica mayor tiempo. La tabla de eventos permite contrastar "
                "esa variación con trabajo ejecutado, sincronización y memoria; no separa sus causas."]
        charts[("pared", "s")] = wall_values
        for index, ((event, unit), values) in enumerate(charts.items()):
            name = f"{source.stem}-{rows[0]['campana']}-{index}.svg"
            svg_chart(plots / name, f"{source.stem}: {event} ({unit}), media", values)
            lines += ["", f"![{event}](plots/{name})"]
        unavailable = sorted({r["evento"] + ": " + r["estado"] for r in rows if r["estado"] != "ok"})
        if unavailable:
            lines += ["", "Eventos no disponibles: " + "; ".join(unavailable) + "."]
        status = OUTPUT / "raw" / rows[0]["campana"] / "status.txt"
        lines += ["", "Estado de campaña: **" + (status.read_text().strip() if status.exists() else "desconocido") + "**."]
    (OUTPUT / "reporte.md").write_text("\n".join(lines) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--problem", choices=("nbody", "ray", "dl", "sentinel"))
    parser.add_argument("--check", action="store_true")
    parser.add_argument("--report", action="store_true")
    args = parser.parse_args()
    OUTPUT.mkdir(exist_ok=True)
    if args.report:
        report()
        return
    if not args.problem and not args.check:
        parser.error("Use --problem, --check o --report")
    if platform.system() != "Linux":
        raise ValueError("perf y la afinidad requieren Linux")
    scope = os.environ.get("SCOPE", "process")
    if scope not in ("process", "cpu"):
        raise ValueError("SCOPE debe ser process o cpu")
    topo = topology()
    perf = os.environ.get("PERF", "perf")
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
    folder = OUTPUT / "raw" / f"{args.problem or 'check'}-{stamp}"
    folder.mkdir(parents=True)
    (folder / "status.txt").write_text("incomplete\n")
    (folder / "environment.json").write_text(json.dumps({"topology": topo, "platform": platform.platform(),
        "scope": scope, "perf": perf, "configuration": {k: os.environ[k] for k in
        ("EVENTS", "REPETITIONS", "MODELS", "THREADS", "SMT_THREADS", "NBODY_ARGS", "RAY_ARGS", "SENTINEL_ARGS",
         "DL_EPOCHS", "DL_SAMPLES", "DL_SEED", "DL_QUANTUM_US") if k in os.environ}}, indent=2))
    try:
        events, unavailable = preflight(perf, scope, topo, folder)
        if unavailable:
            print("Eventos no soportados: " + ", ".join(unavailable), flush=True)
        if args.problem:
            capture(args.problem, perf, scope, topo, folder, events, unavailable)
        (folder / "status.txt").write_text("complete\n")
    finally:
        if args.problem:
            report()


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as error:
        print(f"Error: {error}", file=sys.stderr)
        sys.exit(1)
