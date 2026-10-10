#!/usr/bin/env python3
"""Analiza un lote de una configuración, sin dependencias externas.

IC bilateral del 95 % de la media: mean +/- t(0.975, n-1)*s/sqrt(n).
El cálculo de t integra numéricamente su densidad con el cambio de variable
x=sqrt(df)*tan(theta), y resuelve su cuantil por bisección.
"""
import argparse
import csv
from functools import lru_cache
from html import escape
import math
from pathlib import Path
import statistics
import sys

CONFIG_FIELDS = ("model", "workers", "bodies", "steps", "dt", "seed")


def integrate(function, left, right, tolerance=1e-11):
    """Simpson adaptativo con estimación de error por subdivisión."""
    middle = (left + right) / 2
    fa, fm, fb = function(left), function(middle), function(right)
    whole = (right - left) * (fa + 4 * fm + fb) / 6

    def refine(a, b, fa, fm, fb, estimate, error, depth):
        m = (a + b) / 2
        fl, fr = function((a + m) / 2), function((m + b) / 2)
        first = (m - a) * (fa + 4 * fl + fm) / 6
        second = (b - m) * (fm + 4 * fr + fb) / 6
        delta = first + second - estimate
        if abs(delta) <= 15 * error:
            return first + second + delta / 15
        if depth == 0:
            raise ValueError("La integración de Student t no convergió")
        return (refine(a, m, fa, fl, fm, first, error / 2, depth - 1)
                + refine(m, b, fm, fr, fb, second, error / 2, depth - 1))

    return refine(left, right, fa, fm, fb, whole, tolerance, 24)


@lru_cache(maxsize=None)
def t_critical(df):
    if df < 1:
        raise ValueError("Student t requiere al menos un grado de libertad")
    # La densidad transformada es C*cos(theta)**(df-1), para theta >= 0.
    constant = math.exp(math.lgamma((df + 1) / 2) - math.lgamma(df / 2)) / math.sqrt(math.pi)

    def density(theta):
        return constant * math.cos(theta) ** (df - 1)

    low, high = 0.0, math.pi / 2
    for _ in range(45):
        middle = (low + high) / 2
        if integrate(density, 0.0, middle) < 0.475:
            low = middle
        else:
            high = middle
    return math.sqrt(df) * math.tan((low + high) / 2)


def summarize(values):
    n = len(values)
    if n < 2 or any(not math.isfinite(value) or value <= 0 for value in values):
        raise ValueError("Se requieren al menos dos tiempos positivos y finitos")
    mean = statistics.mean(values)
    deviation = statistics.stdev(values)  # Muestral, denominador n-1.
    half_width = t_critical(n - 1) * deviation / math.sqrt(n)
    return dict(repetitions=n, mean_seconds=mean, sample_stddev_seconds=deviation,
                ci95_low_seconds=mean - half_width, ci95_high_seconds=mean + half_width,
                ci95_relative_half_width_percent=100 * half_width / mean)


def load_samples(path):
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        expected = ["repetition", *CONFIG_FIELDS, "total_seconds"]
        if reader.fieldnames != expected:
            raise ValueError("Cabecera de samples.csv incompatible")
        config, values = None, []
        for row in reader:
            if None in row or any(value is None for value in row.values()):
                raise ValueError("Fila incompleta o con columnas adicionales")
            current = {field: row[field] for field in CONFIG_FIELDS}
            if config is not None and config != current:
                raise ValueError("El lote mezcla configuraciones diferentes")
            config = current
            if int(row["repetition"]) != len(values) + 1:
                raise ValueError("Las repeticiones deben ser consecutivas desde 1")
            values.append(float(row["total_seconds"]))
    summarize(values)  # Valida tamaños y valores antes de generar archivos.
    return config, values


def write_csv(path, rows):
    with path.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


def histogram(values, config, summary, path):
    """Histograma SVG autónomo; cada barra muestra frecuencia absoluta."""
    bins = max(1, math.ceil(math.log2(len(values)) + 1))  # Regla de Sturges.
    low, high = min(values), max(values)
    if low == high:
        padding = max(abs(low) * 0.01, 1e-12)
        low, high = low - padding, high + padding
    width = (high - low) / bins
    counts = [0] * bins
    for value in values:
        counts[min(bins - 1, int((value - low) / width))] += 1
    peak = max(counts)
    title = f"Tiempo total — {config['model']} ({config['workers']} trabajador/es)"
    svg = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 960 600" role="img">',
           f'<title>{escape(title)}</title>',
           '<rect width="960" height="600" fill="white"/>',
           '<g font-family="sans-serif" fill="#172033">',
           f'<text x="80" y="36" font-size="22">{escape(title)}</text>',
           f'<text x="80" y="65" font-size="15">n={len(values)}; media={summary["mean_seconds"]:.6g} s; '
           f'IC 95 % de la media=[{summary["ci95_low_seconds"]:.6g}, {summary["ci95_high_seconds"]:.6g}] s</text>']
    for index, count in enumerate(counts):
        x, height = 80 + index * 800 / bins, count / peak * 380
        svg.append(f'<rect x="{x:.3f}" y="{470-height:.3f}" width="{800/bins-1:.3f}" '
                   f'height="{height:.3f}" fill="#2878b5"><title>{count} muestras; '
                   f'[{low+index*width:.6g}, {low+(index+1)*width:.6g}] s</title></rect>')
    for tick in range(6):
        x, y = 80 + tick * 160, 470 - tick * 76
        svg.append(f'<text x="{x}" y="496" text-anchor="middle" font-size="13">{low+(high-low)*tick/5:.5g}</text>')
        svg.append(f'<text x="68" y="{y+4}" text-anchor="end" font-size="13">{peak*tick/5:.3g}</text>')
    svg.extend(['<path d="M80 90 V470 H880" fill="none" stroke="#172033"/>',
                '<text x="480" y="540" text-anchor="middle" font-size="17">Tiempo total (s)</text>',
                '<text transform="translate(24 280) rotate(-90)" text-anchor="middle" font-size="17">Frecuencia</text>',
                '</g></svg>'])
    path.write_text("\n".join(svg), encoding="utf-8")


def analyze(path):
    config, values = load_samples(path)
    result = summarize(values)
    half = len(values) // 2
    first, second = statistics.mean(values[:half]), statistics.mean(values[half:])
    result["half_means_change_percent"] = 100 * (second - first) / first
    write_csv(path.parent / "summary.csv", [{**config, **result}])
    # Todos los prefijos permiten estudiar estabilidad sin eliminar outliers.
    write_csv(path.parent / "convergence.csv", [summarize(values[:n]) for n in range(2, len(values) + 1)])
    histogram(values, config, result, path.parent / "histogram.svg")
    print(f"Media: {result['mean_seconds']:.9g} s")
    print(f"Desviación estándar muestral: {result['sample_stddev_seconds']:.9g} s")
    print(f"IC 95 % de la media (Student t): [{result['ci95_low_seconds']:.9g}, {result['ci95_high_seconds']:.9g}] s")
    print(f"Semiancho relativo del IC: {result['ci95_relative_half_width_percent']:.4g} %")
    print(f"Cambio entre medias de las dos mitades: {result['half_means_change_percent']:.4g} %")
    if len(values) < 200:
        print(f"Lote de {len(values)} muestras: inferior a las 200 repeticiones de referencia.")
    print(f"Archivos: {path.parent}/{{samples.csv,summary.csv,convergence.csv,histogram.svg}}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("samples", type=Path, help="CSV generado por benchmark.sh")
    args = parser.parse_args()
    try:
        analyze(args.samples)
    except (ValueError, OSError, OverflowError) as error:
        print(f"Error de análisis: {error}", file=sys.stderr)
        sys.exit(1)
