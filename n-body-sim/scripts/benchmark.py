#!/usr/bin/env python3
"""Campaña N-body con referencia compartida y CSV/gráficas equivalentes a ray tracing."""
import argparse
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
import subprocess
import time

from plot_metrics import Measurements, WORKLOAD, generate, load, metric_estimate

FIELDS = ("modelo", "hilos", "repeticion", "cuerpos", "pasos", "dt", "K",
          "intervalo_ms", "espera_ms", "semilla", "tiempo_total_ms",
          "tiempo_secuencial_ms", "speedup", "eficiencia_paralela",
          "nucleos_fisicos_disponibles", "cpus_logicas_disponibles", "smt_activo_linux",
          "afinidad_cpus", "cpus_permitidas", "sibling_groups", "referencia_tipo",
          "referencia_csv", "trayectoria_hash", "referencia_smt", "alcance_tiempo")
SCOPE = "creacion_calculo_sincronizacion_captura_cierre"
ALIASES = {"sequential": "sequential", "fine": "fine-grained", "coarse": "coarse-grained",
           "fine-grained": "fine-grained", "coarse-grained": "coarse-grained", "smt": "smt", "cmp": "cmp"}


def apply_config_defaults(args):
    fields = ("bodies_per_galaxy", "steps", "dt", "seed", "quantum",
              "block_interval_ms", "block_duration_ms")
    missing = [field for field in fields if getattr(args, field) is None]
    if missing:
        defaults = json.loads(subprocess.check_output(
            [str(Path(args.binary).resolve()), "--defaults"], text=True))
        for field in missing:
            setattr(args, field, float(defaults[field]) if field == "dt" else int(defaults[field]))


def topology(binary):
    output = subprocess.check_output([binary, "--topology"], text=True).strip()
    return output, dict(part.split("=", 1) for part in output.split(","))


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
                "binary_sha256": file_hash(Path(binary)),
                "status": "incomplete", "environment": {key: os.environ.get(key, "unknown")
                    for key in ("BENCHMARK_COMPILER", "BENCHMARK_FLAGS")}}
    (logs / "environment.json").write_text(json.dumps(metadata, default=str, indent=2) + "\n")
    (root / "benchmark.txt").write_text(json.dumps(metadata, default=str, indent=2) + "\n")
    status = logs / "status.txt"
    status.write_text("incomplete\n")
    trajectory = logs / "trajectory.nbody"
    common = ["--bodies-per-galaxy", str(args.bodies_per_galaxy), "--steps", str(args.steps),
              "--dt", str(args.dt), "--seed", str(args.seed), "--no-output", "--trajectory", str(trajectory)]

    def check_topology():
        if topology(binary)[0] != snapshot:
            raise ValueError("Cambió la topología/afinidad o el estado SMT; campaña interrumpida")

    def run(model, workers, label):
        check_topology()
        command = [binary, *common, "--model", model]
        if workers is not None:
            command += ["--threads", str(workers)]
        if model == "fine-grained":
            command += ["--quantum", str(args.quantum)]
        if model == "coarse-grained":
            command += ["--block-interval-ms", str(args.block_interval_ms), "--block-duration-ms", str(args.block_duration_ms)]
        with (logs / "commands.jsonl").open("a") as stream:
            stream.write(json.dumps(command) + "\n")
        result = subprocess.run(command, capture_output=True, text=True)
        with (logs / (label + ".log")).open("a") as stream:
            stream.write(result.stderr)
        if result.returncode:
            raise ValueError(f"Falló {label}: {result.stderr.strip()}")
        check_topology()
        records = list(csv.DictReader(result.stdout.splitlines()))
        if len(records) != 1:
            raise ValueError("La simulación no produjo exactamente una medición")
        record = records[0]
        total = float(record["total_seconds"]) * 1000
        if not math.isfinite(total) or total <= 0 or record["model"] != model:
            raise ValueError("Medición inválida")
        if model in ("smt", "cmp"):
            match = re.search(r"CPU fijadas (?:y verificadas|por núcleo físico): ([0-9;]+); ", result.stderr)
            if not match:
                raise ValueError("La simulación no informó la afinidad de sus trabajadores")
            record["affinity"] = match.group(1)
        return record, total, file_hash(trajectory)

    expected_workload = (str(2 * args.bodies_per_galaxy), str(args.steps), str(args.dt), str(args.seed))
    baseline_rows = None
    baseline_path = None
    generated = []

    def measure(model, workers_list, label, reference=None):
        rows = []
        path = root / (label + ".csv")
        with path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            stream.flush()
            for workers in workers_list:
                warmup, _, expected_hash = run(model, workers, label)
                if reference and expected_hash != reference[0]["trayectoria_hash"]:
                    raise ValueError("La trayectoria difiere de la referencia compartida")
                for repetition in range(1, args.repetitions + 1):
                    record, total, digest = run(model, workers, label)
                    if digest != expected_hash:
                        raise ValueError("La trayectoria cambió entre ejecuciones; no se exporta la muestra")
                    actual_workload = (record["bodies"], record["steps"], str(float(record["dt"])), record["seed"])
                    if actual_workload != expected_workload or record["workers"] != warmup["workers"]:
                        raise ValueError("La medición cambió de carga o cantidad de trabajadores")
                    p = int(record["workers"])
                    base = statistics.mean(float(r["tiempo_total_ms"]) for r in reference) if reference else total
                    cpus = record.get("affinity", "no_aplica")
                    if cpus != warmup.get("affinity", "no_aplica"):
                        raise ValueError("La afinidad cambió entre ejecuciones")
                    row = dict(modelo={"fine-grained": "fine", "coarse-grained": "coarse"}.get(model, model),
                        hilos=p, repeticion=repetition, cuerpos=record["bodies"], pasos=record["steps"], dt=str(float(record["dt"])),
                        K=record["quantum"], intervalo_ms=args.block_interval_ms if model == "coarse-grained" else 0,
                        espera_ms=args.block_duration_ms if model == "coarse-grained" else 0, semilla=record["seed"],
                        tiempo_total_ms=total, tiempo_secuencial_ms=base, speedup=base/total, eficiencia_paralela=base/total/p,
                        nucleos_fisicos_disponibles=physical, cpus_logicas_disponibles=logical, smt_activo_linux=topo.get("smt_active", "unknown"),
                        afinidad_cpus=cpus, cpus_permitidas=topo.get("allowed_cpus", "unknown"), sibling_groups=topo.get("sibling_groups", "unknown"),
                        referencia_tipo="compartida" if reference else "propia",
                        referencia_csv=os.path.relpath(baseline_path, root) if reference else "",
                        trayectoria_hash=digest, referencia_smt=reference[0]["referencia_smt"] if reference else topo.get("smt_active", "unknown"),
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
            if any(row[k] != baseline_rows[0][k] for k in ("trayectoria_hash", "referencia_smt")):
                raise ValueError("La referencia mezcla resultados o estados SMT")
    else:
        baseline_rows = measure("sequential", [None], getattr(args, "baseline_label", "sequential"))
    references = root / "references"
    references.mkdir(exist_ok=True)
    source_baseline = baseline_path if args.baseline_csv else root / (getattr(args, "baseline_label", "sequential") + ".csv")
    baseline_path = references / ("sequential-" + stamp + ".csv")
    shutil.copyfile(source_baseline, baseline_path)
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
    subprocess.run([*shlex.split(make_command), "--no-print-directory", "smt-control", "SMT=" + state],
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
            raise RuntimeError(f"No se pudo restaurar SMT={original}. Ejecute: make smt-control SMT={original}") from error
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
    parser.add_argument("--results-dir", type=Path, default=Path("resultnbody"))
    parser.add_argument("--models", nargs="+", choices=ALIASES)
    parser.add_argument("--model", choices=ALIASES)
    parser.add_argument("--thread-counts", default="auto")
    parser.add_argument("--cmp-thread-counts", default="1,2,4,8")
    parser.add_argument("--smt-thread-counts", default="auto")
    parser.add_argument("--smt-states", choices=("keep", "on", "off", "on,off", "off,on"), default="keep")
    parser.add_argument("--make-command", default="make")
    parser.add_argument("--threads", type=int)
    parser.add_argument("--baseline-csv", type=Path)
    # Los valores omitidos se consultan al ejecutable compilado desde nbody.hpp.
    parser.add_argument("--bodies-per-galaxy", type=int)
    parser.add_argument("--steps", type=int)
    parser.add_argument("--dt", type=float)
    parser.add_argument("--seed", type=int)
    parser.add_argument("--quantum", type=int)
    parser.add_argument("--block-interval-ms", type=int)
    parser.add_argument("--block-duration-ms", type=int)
    args = parser.parse_args()
    try:
        apply_config_defaults(args)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, f"No se pudieron consultar los valores de Config: {error}\n")
    if args.repetitions < 2 or (args.threads is not None and args.threads < 1):
        parser.error("Se requieren al menos 2 repeticiones y una cantidad positiva de trabajadores")
    if (args.bodies_per_galaxy < 1 or args.steps < 0 or not 0 <= args.seed <= 2**32-1
            or not math.isfinite(args.dt) or args.dt <= 0 or not math.isfinite(args.dt * args.steps)
            or args.quantum < 1 or not 0 <= args.block_interval_ms <= 2**31-1
            or not 0 <= args.block_duration_ms <= 2**31-1):
        parser.error("Parámetros de simulación o planificación fuera de rango")
    if args.model and args.models:
        parser.error("Seleccione --model o --models, no ambos")
    if args.repetitions < 200:
        print("Prueba corta: menos de 200 repeticiones; no constituye una campaña final.", flush=True)
    try:
        signal.signal(signal.SIGTERM, interrupt_benchmark)
        run_campaigns(args)
    except KeyboardInterrupt:
        parser.exit(130, "Benchmark interrumpido.\n")
    except (ValueError, KeyError, OSError, RuntimeError, subprocess.CalledProcessError, ImportError) as error:
        parser.exit(1, f"Error de benchmark: {error}\n")


if __name__ == "__main__":
    main()
