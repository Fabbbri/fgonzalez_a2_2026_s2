#!/usr/bin/env python3
"""Analiza los CSV crudos; nunca usa medias como muestras de una distribución."""
import argparse
import csv
import glob
import math
from pathlib import Path
import statistics

# La carga se mantiene fija. K y los bloqueos identifican políticas distintas.
WORKLOAD = ("ancho", "alto", "tile_ancho", "tile_alto", "muestras", "rebotes", "semilla")
POLICY = ("modelo", "K", "intervalo_ms", "espera_ms")


def estimate(values):
    mean = statistics.mean(values)
    sd = statistics.stdev(values)
    return mean, sd, 1.96 * sd / math.sqrt(len(values))


def load(patterns):
    paths = sorted({Path(p) for pattern in patterns for p in glob.glob(pattern)})
    if not paths:
        raise ValueError("No hay CSV que coincidan con la ruta indicada")
    groups, workload = {}, None
    for path in paths:
        with path.open(newline="") as stream:
            reader = csv.DictReader(stream)
            # Un resumen no contiene iteraciones y no sirve para un boxplot.
            if "repeticion" not in (reader.fieldnames or []):
                continue
            for row in reader:
                current = tuple(int(row[k]) for k in WORKLOAD)
                if workload is not None and current != workload:
                    raise ValueError("Los CSV mezclan cargas diferentes; grafícalas por separado")
                workload = current
                policy = tuple(row[k] for k in POLICY)
                if row["modelo"] in ("smt", "cmp"):
                    # No mezclar el estado SMT ni topologías de campañas diferentes.
                    policy += ("C=" + row.get("nucleos_fisicos_disponibles", "?") +
                               " L=" + row.get("cpus_logicas_disponibles", "?") +
                               " SMT=" + row.get("smt_activo_linux", "?"),)
                workers = int(row["hilos"])
                if workers < 1:
                    raise ValueError("Cantidad de hilos inválida")
                total, base = float(row["tiempo_total_ms"]), float(row["tiempo_secuencial_ms"])
                if not all(math.isfinite(x) and x > 0 for x in (total, base)):
                    raise ValueError("Tiempos inválidos")
                speedup = base / total
                if not math.isclose(speedup, float(row["speedup"]), rel_tol=1e-8):
                    raise ValueError("Speedup incompatible con los tiempos del CSV")
                if not math.isclose(speedup / workers, float(row["eficiencia_paralela"]), rel_tol=1e-8):
                    raise ValueError("Eficiencia incompatible con los tiempos del CSV")
                groups.setdefault((policy, workers), []).append((total, base, speedup, speedup / workers))
    if not groups or any(len(rows) < 2 for rows in groups.values()):
        raise ValueError("Se necesitan al menos dos iteraciones crudas por configuración")
    return dict(sorted(groups.items())), workload


def label(key):
    policy, workers = key
    model, quantum, interval, wait = policy[:4]
    detail = f"K={quantum}" if model == "fine" else f"bloqueo={interval}/{wait} ms" if model == "coarse" else "base"
    if model in ("smt", "cmp"):
        detail = policy[4]
    return f"{model} · P={workers}\n{detail}"


def generate(groups, workload, output):
    try:
        import matplotlib
        matplotlib.use("Agg")  # No abre ventana y funciona con --headless.
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise ValueError("Falta Matplotlib. En Ubuntu: sudo apt install python3-matplotlib") from error
    output.mkdir(parents=True, exist_ok=True)
    keys = list(groups)
    captions = [f"{label(k)}\nn={len(groups[k])}" for k in keys]
    metrics = [(0, "Tiempo del modelo (ms)", "tiempo"),
               (2, "Speedup respecto a sequential", "speedup"),
               (3, "Eficiencia paralela", "eficiencia")]
    for index, title, name in metrics:
        fig, ax = plt.subplots(figsize=(max(8, len(keys) * 1.8), 5))
        ax.boxplot([[r[index] for r in groups[k]] for k in keys])
        ax.set_xticks(range(1, len(keys) + 1), captions)
        ax.set_ylabel(title)
        ax.set_title("Distribución de todas las iteraciones")
        ax.grid(axis="y", alpha=0.3)
        if index != 0:
            ax.axhline(1, color="gray", linestyle="--", linewidth=1)
        fig.tight_layout()
        fig.savefig(output / f"boxplot-{name}.png", dpi=160)
        plt.close(fig)
        # Un histograma por configuración evita mezclar distribuciones distintas.
        fig, axes = plt.subplots(len(keys), 1, figsize=(8, 2.7 * len(keys)), squeeze=False)
        for key, axes_row in zip(keys, axes):
            ax = axes_row[0]
            ax.hist([r[index] for r in groups[key]], bins="auto", edgecolor="white")
            ax.set_title(label(key).replace("\n", " · ") + f" · n={len(groups[key])}")
            ax.set_xlabel(title)
            ax.set_ylabel("Frecuencia")
        fig.tight_layout()
        fig.savefig(output / f"histograma-{name}.png", dpi=160)
        plt.close(fig)
    # La referencia es sin esquema de hilos, medida en cada par, no coarse P=1.
    fig, ax = plt.subplots(figsize=(max(8, len(keys) * 1.8), 5))
    ax.boxplot([[r[1] for r in groups[k]] for k in keys])
    ax.set_xticks(range(1, len(keys) + 1), captions)
    ax.set_ylabel("Tiempo sequential (ms)")
    ax.set_title("Referencia secuencial de cada comparación pareada")
    fig.tight_layout()
    fig.savefig(output / "boxplot-base-secuencial.png", dpi=160)
    plt.close(fig)

    fig, axes = plt.subplots(1, 3, figsize=(15, 5))
    policies = sorted({key[0] for key in keys})
    for policy in policies:
        selected = [k for k in keys if k[0] == policy]
        ps = [k[1] for k in selected]
        caption = label(selected[0]).replace(f" · P={ps[0]}\n", " · ")
        for ax, (index, title, _) in zip(axes, metrics):
            stats = [estimate([r[index] for r in groups[k]]) for k in selected]
            ax.errorbar(ps, [s[0] for s in stats], yerr=[s[2] for s in stats],
                        marker="o", capsize=4, label=caption)
            ax.set_xlabel("Trabajadores reales P")
            ax.set_ylabel(title)
            ax.set_xticks(sorted({k[1] for k in keys}))
            ax.grid(alpha=0.3)
    axes[1].plot(sorted({k[1] for k in keys}), sorted({k[1] for k in keys}),
                 "--", color="gray", label="Speedup ideal S=P")
    for ax in axes:
        ax.legend(fontsize=8)
    fig.suptitle("Escalabilidad: media e IC95 aproximado (una configuración es un punto)")
    fig.tight_layout()
    fig.savefig(output / "escalabilidad.png", dpi=160)
    plt.close(fig)

    report = ["# Análisis de repeticiones", "",
              "Carga fija: " + ", ".join(f"{k}={v}" for k, v in zip(WORKLOAD, workload)) + ".", "",
              "Se incluyen todas las iteraciones, sin eliminar valores atípicos. "
              "La referencia es sequential, medida en cada par de ejecuciones.", "",
              "| Configuración | n | Referencia 200 | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |",
              "| --- | ---: | --- | ---: | ---: | ---: | ---: |"]
    for key, rows in groups.items():
        tm, sd, margin = estimate([r[0] for r in rows])
        sm, ssd, smargin = estimate([r[2] for r in rows])
        status = "alcanzada" if len(rows) >= 200 else "prueba corta, pendiente"
        report.append(f"| {label(key).replace(chr(10), ' · ')} | {len(rows)} | {status} | {tm:.4f} ± {margin:.4f} | {sd:.4f} | {sm:.4f} ± {smargin:.4f} | {ssd:.4f} |")
    report += ["", "## Justificación y alcance", "",
               "Se eligen inicialmente 200 repeticiones por configuración para cumplir la "
               "referencia del enunciado, caracterizar la variabilidad y estimar la precisión de la media. "
               "El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); "
               "al aumentar n disminuye el error estándar si las muestras son independientes. "
               "200 no garantiza precisión suficiente: en el informe se debe evaluar el margen "
               "relativo (margen/media) de tiempo y speedup, y aumentar las repeticiones si es excesivo. "
               "Una prueba con menos de 200 sirve para validar el funcionamiento, no se presenta como campaña final.", "",
               "Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones "
               "de carga del equipo en el informe. Mantener escena, semilla, resolución, tiles, muestras, "
               "rebotes y política constantes durante cada barrido. El calentamiento no se cuenta. "
               "Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC "
               "aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico."]
    (output / "reporte.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    print(f"Gráficas y reporte: {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="+", help="CSV crudos o patrones; los resúmenes se omiten")
    parser.add_argument("--output-dir", type=Path, default=Path("results/plots"))
    args = parser.parse_args()
    try:
        groups, workload = load(args.csv)
        generate(groups, workload, args.output_dir)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
