#!/usr/bin/env python3
"""Benchmark SGD/MNIST: mediciones crudas, referencia compartida y SMT on/off."""
import argparse
import numpy as np
import copy
import csv
import hashlib
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import shlex
import signal
import statistics
import struct
import subprocess
import time

from plot_metrics import Measurements, WORKLOAD, generate, load, metric_estimate

FIELDS = ("modelo", "hilos", "repeticion", "epocas", "muestras", "learning_rate",
          "semilla", "dataset_hash", "K", "intervalo_ms", "espera_ms", "tiempo_total_ms",
          "tiempo_secuencial_ms", "speedup", "eficiencia_paralela", "nucleos_fisicos_disponibles",
          "cpus_logicas_disponibles", "smt_activo_linux", "afinidad_cpus", "cpus_permitidas",
          "sibling_groups", "referencia_tipo", "referencia_csv", "parametros_hash", "parametros_archivo",
          "referencia_smt", "alcance_tiempo", "equivalencia_numerica", "max_error_absoluto")
SCOPE = "creacion_entrenamiento_sincronizacion_cierre_sin_io"
ALIASES = {"sequential": "sequential", "main": "sequential", "fine": "fine-grained", "fino": "fine-grained",
           "fine-grained": "fine-grained", "coarse": "coarse-grained", "grueso": "coarse-grained",
           "coarse-grained": "coarse-grained", "smt": "smt", "cmp": "cmp"}
EXECUTABLES = {"sequential": "main", "fine-grained": "fino", "coarse-grained": "grueso", "smt": "smt.bin", "cmp": "cmp"}


def topology(binary=None):
    groups = {}
    for cpu in sorted(os.sched_getaffinity(0)):
        directory = Path(f"/sys/devices/system/cpu/cpu{cpu}/topology")
        key = (int((directory / "physical_package_id").read_text()), int((directory / "core_id").read_text()))
        groups.setdefault(key, []).append(cpu)
    cores = [cpus for _, cpus in sorted(groups.items())]
    def state(name):
        try:
            return Path("/sys/devices/system/cpu/smt", name).read_text().strip()
        except OSError:
            return "unknown"
    topo = dict(physical_cores=str(len(cores)), logical_cpus=str(sum(map(len, cores))),
        allowed_cpus=";".join(map(str, sorted(os.sched_getaffinity(0)))),
        core_cpus=";".join(str(c[0]) for c in cores),
        sibling_groups="|".join(";".join(map(str, c)) for c in cores),
        smt_control=state("control"), smt_active=state("active"))
    return ",".join(f"{k}={v}" for k, v in topo.items()), topo


def smt_cpus(topo, workers):
    groups = [list(map(int, group.split(";"))) for group in topo["sibling_groups"].split("|")]
    order = [group[level] for level in range(max(map(len, groups))) for group in groups if level < len(group)]
    return [order[i % len(order)] for i in range(workers)]


def dataset_hash(project, samples):
    image = project / "../data/mnist/train-images-idx3-ubyte"
    label = project / "../data/mnist/train-labels-idx1-ubyte"
    with image.open("rb") as stream:
        magic, count, rows, cols = struct.unpack(">4I", stream.read(16))
    with label.open("rb") as stream:
        label_magic, label_count = struct.unpack(">2I", stream.read(8))
    if (magic != 2051 or label_magic != 2049 or (rows, cols) != (28, 28)
            or count != label_count or samples > count
            or image.stat().st_size != 16 + count * rows * cols
            or label.stat().st_size != 8 + count):
        raise ValueError("MNIST: cabeceras, tamaños o cantidad de muestras incompatibles")
    return hashlib.sha256((file_hash(image) + file_hash(label)).encode()).hexdigest()


def read_parameters(path):
    values = np.fromfile(path, dtype=np.float64)
    if len(values) != 109386 or not np.isfinite(values).all():
        raise ValueError("Parámetros finales inválidos")
    return values


def file_hash(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def write_summary(path, rows, baseline):
    summaries = []
    for workers in sorted({int(row["hilos"]) for row in rows}):
        selected = [row for row in rows if int(row["hilos"]) == workers]
        data = Measurements((float(r["tiempo_total_ms"]), float(r["tiempo_secuencial_ms"]),
                             float(r["speedup"]), float(r["eficiencia_paralela"])) for r in selected)
        data.workers = workers
        data.baseline = baseline
        total, base, speedup, efficiency = [metric_estimate(data, i) for i in range(4)]
        summaries.append(dict(modelo=selected[0]["modelo"], hilos=workers, repeticiones=len(data),
            tiempo_total_media_ms=total[0], tiempo_total_desviacion_ms=total[1],
            tiempo_total_ic95_aprox_inferior=total[0]-total[2], tiempo_total_ic95_aprox_superior=total[0]+total[2],
            tiempo_secuencial_media_ms=base[0], speedup_media=speedup[0], speedup_desviacion=speedup[1],
            speedup_ic95_aprox_inferior=speedup[0]-speedup[2], speedup_ic95_aprox_superior=speedup[0]+speedup[2],
            eficiencia_media=efficiency[0], eficiencia_ic95_aprox_inferior=efficiency[0]-efficiency[2],
            eficiencia_ic95_aprox_superior=efficiency[0]+efficiency[2]))
    with path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=list(summaries[0]))
        writer.writeheader()
        writer.writerows(summaries)


def counts(value, physical, logical, smt=False):
    if value == "auto":
        return sorted({physical, logical if logical > physical else 2 * physical}) if smt else sorted(
            {p for p in (1, 2, 4, 8, physical) if p <= physical})
    try:
        parsed = sorted({int(p) for p in value.split(",")})
    except ValueError as error:
        raise ValueError("Las cantidades de hilos deben ser enteros separados por comas") from error
    if not parsed or min(parsed) < 1:
        raise ValueError("Las cantidades de hilos deben ser positivas")
    return parsed


def campaign(args):
    import matplotlib  # Fallar antes de medir si no se podrán producir gráficas.
    matplotlib.use("Agg")
    binary = str(Path(args.binary).resolve())
    project = Path(binary).parent
    input_hash = dataset_hash(project, args.samples)
    models = list(dict.fromkeys(ALIASES[m] for m in (args.models or [args.model or "sequential"])))
    snapshot, topo = topology(binary)
    physical, logical = int(topo["physical_cores"]), int(topo["logical_cpus"])
    ordinary = counts(args.thread_counts, physical, logical)
    cmp = counts(args.cmp_thread_counts, physical, logical)
    smt = counts(args.smt_thread_counts, physical, logical, True)
    if args.threads is not None:
        ordinary = cmp = smt = [args.threads]
    if "cmp" in models and max(cmp) > physical:
        raise ValueError("CMP requiere una cantidad de trabajadores <= núcleos físicos")
    root = args.results_dir.resolve()
    root.mkdir(parents=True, exist_ok=True)
    stamp = time.strftime("%Y%m%d-%H%M%S") + "-" + str(time.time_ns())
    logs = root / "logs" / stamp
    logs.mkdir(parents=True)
    (logs / "topology.txt").write_text(snapshot + "\n")
    metadata = {**vars(args), "binary": binary, "platform": platform.platform(), "topology": topo,
                "timing_scope": SCOPE, "warmups_per_configuration": 1, "hash": "sha256",
                "binary_sha256": {name: file_hash(project / exe) for name, exe in EXECUTABLES.items()},
                "dataset_sha256": input_hash, "numerical_rtol": 1e-9, "numerical_atol": 1e-11,
                "status": "incomplete", "environment": {key: os.environ.get(key, "unknown")
                    for key in ("BENCHMARK_COMPILER", "BENCHMARK_FLAGS")}}
    (logs / "environment.json").write_text(json.dumps(metadata, default=str, indent=2) + "\n")
    (root / "benchmark.txt").write_text(json.dumps(metadata, default=str, indent=2) + "\n")
    status = logs / "status.txt"
    status.write_text("incomplete\n")
    reference_params = None
    env = {**os.environ, "DL_QUIET": "1", "DL_SEED": str(args.seed), "DL_SAMPLES": str(args.samples),
           "DL_QUANTUM_US": str(args.quantum_us)}

    def check_topology():
        if topology(binary)[0] != snapshot:
            raise ValueError("Cambió la topología/afinidad o el estado SMT; campaña interrumpida")

    def run(model, workers, label):
        check_topology()
        cpus = []
        command = [str(project / EXECUTABLES[model])]
        if model in ("sequential", "fine-grained"):
            command += [str(args.epochs)]
        elif model == "coarse-grained":
            command += [str(args.epochs), str(workers), str(args.block_interval_ms), str(args.block_duration_ms)]
        elif model == "smt":
            cpus = smt_cpus(topo, workers)
            command += [str(workers), ",".join(map(str, cpus)), str(args.epochs)]
        else:
            command += [str(workers), str(args.epochs)]
            cpus = list(map(int, topo["core_cpus"].split(";")))[:workers]
        params_path = logs / f"{label}-p{workers or 1}.params"
        with (logs / "commands.jsonl").open("a") as stream:
            stream.write(json.dumps(command) + "\n")
        result = subprocess.run(command, cwd=project, env={**env, "DL_PARAMS": str(params_path)},
                                capture_output=True, text=True)
        with (logs / (label + ".log")).open("a") as stream:
            stream.write(result.stdout + result.stderr)
        if result.returncode:
            raise ValueError(f"Falló {label}: {result.stderr.strip()}")
        check_topology()
        timing = re.search(r"Tiempo total: ([0-9]+) us", result.stdout)
        checksum = re.search(r"Checksum parametros: ([0-9a-f]+)", result.stdout)
        if not timing or not checksum:
            raise ValueError("La simulación no produjo tiempo y checksum")
        total = int(timing[1]) / 1000
        if total <= 0:
            raise ValueError("Medición inválida")
        affinity = "no_aplica"
        if model in ("smt", "cmp"):
            match = re.search(r"CPU fijadas y verificadas: ([0-9;]+); Hilos: ([0-9]+)", result.stdout)
            if not match or int(match[2]) != workers or match[1] != ";".join(map(str, cpus)):
                raise ValueError("Afinidad efectiva distinta de la solicitada")
            affinity = match[1]
        if model == "coarse-grained" and f"Hilos: {workers}\n" not in result.stdout:
            raise ValueError("Cantidad de trabajadores incorrecta")
        params = read_parameters(params_path)
        error = 0.0
        if reference_params is not None:
            error = float(np.max(np.abs(params - reference_params)))
            if not np.allclose(params, reference_params, rtol=1e-9, atol=1e-11):
                raise ValueError(f"Pesos finales difieren de secuencial: error máximo {error}")
        record = dict(workers=str(workers or 1), affinity=affinity,
                      parameters_file=str(params_path.relative_to(root)), max_error=error)
        return record, total, file_hash(params_path)

    expected_workload = (str(args.epochs), str(args.samples), "0.1", str(args.seed), input_hash)
    baseline_rows = None
    baseline_path = None
    generated = []

    def measure(model, workers_list, label, reference=None):
        rows = []
        path = root / (label + ".csv")
        resume = getattr(args, "resume", False) and path.exists()
        if resume:
            if reference is None:
                raise ValueError("Para continuar use BASELINE_CSV con la referencia existente")
            with path.open(newline="") as stream:
                reader = csv.DictReader(stream)
                if tuple(reader.fieldnames or ()) != FIELDS:
                    raise ValueError("CSV incompatible para continuar")
                rows = list(reader)
            reference_times = [float(r["tiempo_total_ms"]) for r in reference]
            cached_references = {}
            policy = (args.quantum_us if model == "fine-grained" else 0,
                      args.block_interval_ms if model == "coarse-grained" else 0,
                      args.block_duration_ms if model == "coarse-grained" else 0)
            expected_topology = dict(nucleos_fisicos_disponibles=physical,
                cpus_logicas_disponibles=logical, smt_activo_linux=topo.get("smt_active", "unknown"),
                cpus_permitidas=topo["allowed_cpus"], sibling_groups=topo["sibling_groups"])
            for row in rows:
                if (row["modelo"] != {"fine-grained": "fine", "coarse-grained": "coarse"}.get(model, model)
                        or tuple(row[k] for k in WORKLOAD) != expected_workload
                        or tuple(int(row[k]) for k in ("K", "intervalo_ms", "espera_ms")) != policy
                        or row["alcance_tiempo"] != SCOPE
                        or row["referencia_tipo"] != "compartida"
                        or row["referencia_smt"] != reference[0]["referencia_smt"]
                        or any(row[field] != str(value) for field, value in expected_topology.items())):
                    raise ValueError("CSV previo incompatible con la carga, política o topología actual")
                old_reference = (root / row["referencia_csv"]).resolve()
                if old_reference not in cached_references:
                    with old_reference.open(newline="") as stream:
                        cached_references[old_reference] = list(csv.DictReader(stream))
                old_rows = cached_references[old_reference]
                if (not old_rows or old_rows[0]["parametros_hash"] != reference[0]["parametros_hash"]
                        or [float(r["tiempo_total_ms"]) for r in old_rows] != reference_times):
                    raise ValueError("Para continuar se requiere la misma referencia secuencial")
                total = float(row["tiempo_total_ms"])
                if not math.isfinite(total) or total <= 0:
                    raise ValueError("Tiempos previos inválidos")
                speedup = statistics.mean(reference_times) / total
                if (int(row["hilos"]) < 1
                        or not math.isclose(float(row["tiempo_secuencial_ms"]), statistics.mean(reference_times), rel_tol=1e-8)
                        or not math.isclose(float(row["speedup"]), speedup, rel_tol=1e-8)
                        or not math.isclose(float(row["eficiencia_paralela"]), speedup / int(row["hilos"]), rel_tol=1e-8)):
                    raise ValueError("Métricas previas inválidas")
            for workers in {int(row["hilos"]) for row in rows}:
                selected = [row for row in rows if int(row["hilos"]) == workers]
                if (sorted(int(row["repeticion"]) for row in selected) != list(range(1, len(selected) + 1))
                        or len({(row["parametros_hash"], row["afinidad_cpus"]) for row in selected}) != 1):
                    raise ValueError("Repeticiones previas duplicadas, incompletas o inconsistentes")
        for existing in (path, root / (label + "-summary.csv")):
            if existing.exists():
                backup = root / "backups" / stamp
                backup.mkdir(parents=True, exist_ok=True)
                shutil.copy2(existing, backup / existing.name)
        (root / (label + "-summary.csv")).unlink(missing_ok=True)
        with path.open("a" if resume else "w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            if not resume:
                writer.writeheader()
            stream.flush()
            for workers in workers_list:
                previous = [row for row in rows if int(row["hilos"]) == (workers or 1)]
                if len(previous) >= args.repetitions:
                    print(f"{label}: P={workers or 1}, se conservan {len(previous)} repeticiones existentes", flush=True)
                    continue
                warmup, _, expected_hash = run(model, workers, label)
                if previous and (previous[0]["parametros_hash"] != expected_hash
                                 or previous[0]["afinidad_cpus"] != warmup.get("affinity", "no_aplica")):
                    raise ValueError("La configuración actual difiere de las mediciones previas")
                for repetition in range(len(previous) + 1, args.repetitions + 1):
                    record, total, digest = run(model, workers, label)
                    if digest != expected_hash:
                        raise ValueError("Los parámetros cambiaron entre ejecuciones; no se exporta la muestra")
                    if record["workers"] != warmup["workers"]:
                        raise ValueError("La medición cambió de carga o cantidad de trabajadores")
                    p = int(record["workers"])
                    base = statistics.mean(float(r["tiempo_total_ms"]) for r in reference) if reference else total
                    cpus = record.get("affinity", "no_aplica")
                    if cpus != warmup.get("affinity", "no_aplica"):
                        raise ValueError("La afinidad cambió entre ejecuciones")
                    row = dict(modelo={"fine-grained": "fine", "coarse-grained": "coarse"}.get(model, model),
                        hilos=p, repeticion=repetition, epocas=args.epochs, muestras=args.samples, learning_rate="0.1",
                        dataset_hash=input_hash, K=args.quantum_us if model == "fine-grained" else 0, intervalo_ms=args.block_interval_ms if model == "coarse-grained" else 0,
                        espera_ms=args.block_duration_ms if model == "coarse-grained" else 0, semilla=args.seed,
                        tiempo_total_ms=total, tiempo_secuencial_ms=base, speedup=base/total, eficiencia_paralela=base/total/p,
                        nucleos_fisicos_disponibles=physical, cpus_logicas_disponibles=logical, smt_activo_linux=topo.get("smt_active", "unknown"),
                        afinidad_cpus=cpus, cpus_permitidas=topo.get("allowed_cpus", "unknown"), sibling_groups=topo.get("sibling_groups", "unknown"),
                        referencia_tipo="compartida" if reference else "propia",
                        referencia_csv=os.path.relpath(baseline_path, root) if reference else "",
                        parametros_hash=digest, parametros_archivo=record["parameters_file"],
                        equivalencia_numerica="allclose" if reference is not None else "propia",
                        max_error_absoluto=record["max_error"], referencia_smt=reference[0]["referencia_smt"] if reference else topo.get("smt_active", "unknown"),
                        alcance_tiempo=SCOPE)
                    writer.writerow(row)
                    stream.flush()
                    rows.append(row)
                    if repetition == 1 or repetition % 10 == 0 or repetition == args.repetitions:
                        print(f"{label}: P={p}, repetición {repetition}/{args.repetitions}", flush=True)
        write_summary(root / (label + "-summary.csv"), rows,
                      [float(r["tiempo_total_ms"]) for r in reference] if reference else None)
        generated.append(path)
        return rows

    if args.baseline_csv:
        baseline_path = args.baseline_csv.resolve()
        with baseline_path.open(newline="") as stream:
            baseline_rows = list(csv.DictReader(stream))
        if len(baseline_rows) < 2:
            raise ValueError("La referencia requiere al menos dos mediciones")
        for row in baseline_rows:
            if row["modelo"] != "sequential" or row["hilos"] != "1" or tuple(row[k] for k in WORKLOAD) != expected_workload:
                raise ValueError("Referencia incompatible con la carga actual")
            if row["alcance_tiempo"] != SCOPE or not math.isfinite(float(row["tiempo_total_ms"])) or float(row["tiempo_total_ms"]) <= 0:
                raise ValueError("Referencia con alcance o tiempos inválidos")
            if any(row[k] != baseline_rows[0][k] for k in ("parametros_hash", "referencia_smt")):
                raise ValueError("La referencia mezcla resultados o estados SMT")
    else:
        baseline_rows = measure("sequential", [None], getattr(args, "baseline_label", "sequential"))
    source_params_root = args.baseline_csv.resolve().parent if args.baseline_csv else root
    source_params = source_params_root / baseline_rows[0]["parametros_archivo"]
    if file_hash(source_params) != baseline_rows[0]["parametros_hash"]:
        raise ValueError("Parámetros de referencia alterados o incompatibles con su CSV")
    reference_params = read_parameters(source_params)
    references = root / "references"
    references.mkdir(exist_ok=True)
    source_baseline = baseline_path if args.baseline_csv else root / (getattr(args, "baseline_label", "sequential") + ".csv")
    baseline_path = references / ("sequential-" + stamp + ".csv")
    shutil.copyfile(source_baseline, baseline_path)
    shutil.copyfile(source_params_root / baseline_rows[0]["parametros_archivo"], baseline_path.with_suffix(".params"))
    # A reference can subsequently be reused directly from references/.
    with baseline_path.open("w", newline="") as stream:
        writer = csv.DictWriter(stream, fieldnames=FIELDS)
        writer.writeheader()
        for row in baseline_rows:
            writer.writerow({**row, "parametros_archivo": baseline_path.with_suffix(".params").name})
    for model in models:
        if model == "sequential":
            continue
        label = {"fine-grained": "fine", "coarse-grained": "coarse"}.get(model, model)
        if model == "smt":
            label += "-" + {"1": "on", "0": "off"}.get(topo.get("smt_active"), "unknown")
        workers = smt if model == "smt" else cmp if model == "cmp" else ordinary if model == "coarse-grained" else [None]
        measure(model, workers, label, baseline_rows)
    if not getattr(args, "defer_plots", False):
        plot_campaign(generated, root)
    metadata["status"] = "measured" if getattr(args, "defer_plots", False) else "complete"
    status.write_text(metadata["status"] + "\n")
    metadata["generated_csv"] = [str(p.relative_to(root)) for p in generated]
    metadata["baseline_csv"] = str(baseline_path.relative_to(root))
    (logs / "environment.json").write_text(json.dumps(metadata, default=str, indent=2) + "\n")
    (root / "benchmark.txt").write_text(json.dumps(metadata, default=str, indent=2) + "\n")
    print(f"Mediciones guardadas: {root}" if getattr(args, "defer_plots", False)
          else f"Resultados y gráficas: {root}")
    return {"generated": generated, "logs": logs, "metadata": metadata}


def plot_campaign(generated, root):
    for path in generated:
        groups, workload = load([str(path)])
        generate(groups, workload, root / "plots" / path.stem)
    if generated:
        groups, workload = load([str(path) for path in generated])
        generate(groups, workload, root / "plots" / "comparison")


def read_smt_state():
    state = Path("/sys/devices/system/cpu/smt/control").read_text().strip()
    if state not in ("on", "off"):
        raise ValueError(f"No se puede alternar SMT: {state}. Use SMT_STATES=keep.")
    return state


def set_smt_state(state, make_command):
    subprocess.run([*shlex.split(make_command), "--no-print-directory", "smt", "SMT=" + state],
                   cwd=Path(__file__).resolve().parents[1], check=True)


def interrupt_benchmark(signum, frame):
    raise KeyboardInterrupt


def authorize_smt_control():
    subprocess.run(["sudo", "-v"], check=True)


def run_campaigns(args):
    models = list(dict.fromkeys(ALIASES[m] for m in (args.models or [args.model or "sequential"])))
    states = getattr(args, "smt_states", "keep")
    if "smt" not in models or states == "keep":
        return campaign(args)
    # Comprobar dependencias y credenciales antes de una campaña larga.
    import matplotlib  # noqa: F401
    original = read_smt_state()
    authorize_smt_control()
    make_command = args.make_command
    results = []
    root = args.results_dir.resolve()
    try:
        # Descubrir el barrido con SMT activo y mantenerlo idéntico en ambos estados.
        set_smt_state("on", make_command)
        _, topo = topology(str(Path(args.binary).resolve()))
        fixed_counts = counts(args.smt_thread_counts, int(topo["physical_cores"]), int(topo["logical_cpus"]), True)
        if args.threads is not None:
            fixed_counts = [args.threads]
        for index, state in enumerate(states.split(",")):
            set_smt_state(state, make_command)
            phase = copy.copy(args)
            phase.models = models if index == 0 else ["smt"]
            phase.model = None
            phase.smt_thread_counts = ",".join(map(str, fixed_counts))
            phase.defer_plots = True
            # Una referencia por estado; conservar la primera y los CSV previos.
            if index > 0 or state == "off" or models == ["smt"]:
                phase.baseline_label = "sequential-" + state
            print(f"Campaña SMT={state}; trabajadores={phase.smt_thread_counts}", flush=True)
            results.append(campaign(phase))
    finally:
        print(f"Restaurando SMT={original}...", flush=True)
        try:
            set_smt_state(original, make_command)
        except (OSError, subprocess.CalledProcessError) as error:
            raise RuntimeError(f"No se pudo restaurar SMT={original}. Ejecute: make smt SMT={original}") from error
    # Restaurar antes de graficar, como en la campaña de ray tracing.
    generated = [path for result in results for path in result["generated"]]
    plot_campaign(generated, root)
    for result in results:
        result["metadata"]["status"] = "complete"
        (result["logs"] / "status.txt").write_text("complete\n")
        (result["logs"] / "environment.json").write_text(json.dumps(result["metadata"], default=str, indent=2) + "\n")
    manifest = {"status": "complete", "smt_states": states, "original_smt": original,
                "restored_smt": original, "smt_thread_counts": fixed_counts,
                "generated_csv": [str(path.relative_to(root)) for path in generated],
                "phases": [str(result["logs"].relative_to(root)) for result in results]}
    (root / "benchmark.txt").write_text(json.dumps(manifest, indent=2) + "\n")
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("binary")
    parser.add_argument("--repetitions", type=int, default=200)
    parser.add_argument("--results-dir", type=Path, default=Path("resultdl"))
    parser.add_argument("--models", nargs="+", choices=ALIASES)
    parser.add_argument("--model", choices=ALIASES)
    parser.add_argument("--thread-counts", default="auto")
    parser.add_argument("--cmp-thread-counts", default="auto")
    parser.add_argument("--smt-thread-counts", default="auto")
    parser.add_argument("--smt-states", choices=("keep", "on", "off", "on,off", "off,on"), default="keep")
    parser.add_argument("--make-command", default="make")
    parser.add_argument("--threads", type=int)
    parser.add_argument("--baseline-csv", type=Path)
    parser.add_argument("--resume", action="store_true", help="Conservar datos y completar hasta --repetitions por configuración")
    parser.add_argument("--epochs", type=int, default=1000)
    parser.add_argument("--samples", type=int, default=100)
    parser.add_argument("--seed", type=int, default=1)
    parser.add_argument("--quantum-us", type=int, default=100)
    parser.add_argument("--block-interval-ms", type=int, default=10)
    parser.add_argument("--block-duration-ms", type=int, default=1)
    args = parser.parse_args()
    if args.repetitions < 2 or (args.threads is not None and args.threads < 1):
        parser.error("Se requieren al menos 2 repeticiones y trabajadores positivos")
    if (args.epochs < 1 or not 1 <= args.samples <= 60000 or not 0 <= args.seed <= 2**31-1
            or args.quantum_us < 1 or not 0 <= args.block_interval_ms <= 2**31-1
            or not 0 <= args.block_duration_ms <= 2**31-1):
        parser.error("Parámetros de entrenamiento o planificación fuera de rango")
    if args.model and args.models:
        parser.error("Seleccione --model o --models, no ambos")
    if args.repetitions < 200:
        print("Prueba corta: menos de 200 repeticiones; no constituye una campaña final.", flush=True)
    try:
        signal.signal(signal.SIGTERM, interrupt_benchmark)
        run_campaigns(args)
    except KeyboardInterrupt:
        parser.exit(130, "Benchmark interrumpido.\n")
    except (ValueError, KeyError, OSError, RuntimeError, subprocess.CalledProcessError, ImportError, struct.error) as error:
        if args.results_dir.exists():
            (args.results_dir / "last-error.txt").write_text(str(error) + "\n")
        parser.exit(1, f"Error de benchmark: {error}\n")


if __name__ == "__main__":
    main()
