#!/usr/bin/env python3
"""Recalcula métricas con una referencia secuencial nueva, sin entrenar otra vez."""
import argparse
import csv
import json
import math
import os
from pathlib import Path
import shutil
import statistics
import time

import numpy as np
from benchmark import FIELDS, SCOPE, file_hash, plot_campaign, read_parameters, write_summary
from plot_metrics import WORKLOAD, load


def read_rows(path):
    with path.open(newline="") as stream:
        reader = csv.DictReader(stream)
        if tuple(reader.fieldnames or ()) != FIELDS:
            raise ValueError(f"Esquema incompatible: {path}")
        return list(reader)


def write_rows(path, rows):
    temporary = path.with_suffix(path.suffix + ".tmp")
    try:
        with temporary.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=FIELDS)
            writer.writeheader()
            writer.writerows(rows)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def rebase(root, baseline, plots=True):
    root, baseline = Path(root).resolve(), Path(baseline).resolve()
    reference = read_rows(baseline)
    load([str(baseline)])
    if len(reference) < 2:
        raise ValueError("Se requieren al menos dos mediciones secuenciales")
    first = reference[0]
    workload = tuple(first[k] for k in WORKLOAD)
    for row in reference:
        if (row["modelo"] != "sequential" or row["hilos"] != "1"
                or tuple(row[k] for k in WORKLOAD) != workload or row["alcance_tiempo"] != SCOPE
                or any(row[k] != first[k] for k in ("parametros_hash", "referencia_smt"))):
            raise ValueError("La nueva referencia mezcla configuraciones o resultados")
    params_source = baseline.parent / first["parametros_archivo"]
    if file_hash(params_source) != first["parametros_hash"]:
        raise ValueError("Los parámetros de la referencia no coinciden con su hash")
    reference_params = read_parameters(params_source)
    times = [float(r["tiempo_total_ms"]) for r in reference]
    if not all(math.isfinite(t) and t > 0 for t in times):
        raise ValueError("Tiempos secuenciales inválidos")
    mean = statistics.mean(times)
    paths = sorted(p for p in root.glob("*.csv") if not p.stem.endswith("-summary"))
    load([str(p) for p in paths])
    stamp = time.strftime("%Y%m%d-%H%M%S") + "-" + str(time.time_ns())
    snapshot = root / "references" / ("sequential-rebased-" + stamp + ".csv")
    updated, parameter_cache = {}, {}
    # Validar toda la campaña antes de cambiar cualquier CSV.
    for path in paths:
        rows = read_rows(path)
        if not rows:
            raise ValueError(f"CSV sin mediciones: {path}")
        if all(row["modelo"] == "sequential" for row in rows):
            continue
        for row in rows:
            if (row["modelo"] == "sequential" or tuple(row[k] for k in WORKLOAD) != workload
                    or row["alcance_tiempo"] != SCOPE):
                raise ValueError(f"Carga o alcance incompatible: {path}")
            params = (root / row["parametros_archivo"]).resolve()
            key = (params, row["parametros_hash"])
            if key not in parameter_cache:
                if file_hash(params) != row["parametros_hash"]:
                    raise ValueError(f"Parámetros alterados: {params}")
                values = read_parameters(params)
                if not np.allclose(values, reference_params, rtol=1e-9, atol=1e-11):
                    raise ValueError(f"Pesos incompatibles con la nueva referencia: {path}")
                parameter_cache[key] = float(np.max(np.abs(values - reference_params)))
            total, workers = float(row["tiempo_total_ms"]), int(row["hilos"])
            row.update(tiempo_secuencial_ms=mean, speedup=mean / total,
                eficiencia_paralela=mean / total / workers, referencia_tipo="compartida",
                referencia_csv=os.path.relpath(snapshot, root), referencia_smt=first["referencia_smt"],
                equivalencia_numerica="allclose", max_error_absoluto=parameter_cache[key])
        updated[path] = rows
    backup = root / "backups" / ("rebase-" + stamp)
    backup.mkdir(parents=True)
    for path in paths:
        for source in (path, path.with_name(path.stem + "-summary.csv")):
            if source.exists():
                shutil.copy2(source, backup / source.name)
    snapshot.parent.mkdir(exist_ok=True)
    shutil.copy2(params_source, snapshot.with_suffix(".params"))
    write_rows(snapshot, [{**row, "parametros_archivo": snapshot.with_suffix(".params").name} for row in reference])
    manifest = dict(status="incomplete", baseline=str(baseline), reference=str(snapshot),
                    sequential_mean_ms=mean, reference_repetitions=len(times), updated_csv=[p.name for p in updated])
    manifest_path = backup / "rebase.json"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    for path, rows in updated.items():
        write_rows(path, rows)
        write_summary(path.with_name(path.stem + "-summary.csv"), rows, times)
    # Recalcular también el resumen de la nueva referencia, conservando sus tiempos.
    for path in paths:
        if path not in updated:
            rows = read_rows(path)
            write_summary(path.with_name(path.stem + "-summary.csv"), rows, None)
    load([str(p) for p in paths])
    manifest["status"] = "complete"
    manifest_path.write_text(json.dumps(manifest, indent=2) + "\n")
    print(f"Nueva media secuencial: {mean:.6f} ms · n={len(times)}", flush=True)
    print(f"Actualizados {len(updated)} CSV; respaldo: {backup}", flush=True)
    if plots:
        plot_campaign(paths, root)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--results-dir", type=Path, default=Path("resultdl"))
    parser.add_argument("--baseline-csv", type=Path)
    parser.add_argument("--no-plots", action="store_true")
    args = parser.parse_args()
    try:
        rebase(args.results_dir, args.baseline_csv or args.results_dir / "sequential.csv", not args.no_plots)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"No se pudo actualizar la referencia: {error}\n")


if __name__ == "__main__":
    main()
