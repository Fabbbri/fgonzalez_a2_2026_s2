#!/usr/bin/env python3
# Estadística de un lote de tiempos de una configuración única.
# Usa Student t para IC95 de la media, integración adaptativa y bisección.
# El histograma SVG y las series por prefijo se generan después de medir.
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

# Dimensiones que identifican un lote; no se combinan P, carga o quantum distintos.
CONFIG_FIELDS = ("model", "workers", "bodies", "steps", "dt", "seed", "quantum")


# Integra numéricamente en [left,right] mediante Simpson con refinamiento adaptativo.
def integrate(function, left, right, tolerance=1e-11):
    """Simpson adaptativo con estimación de error por subdivisión."""
    middle = (left + right) / 2
    fa, fm, fb = function(left), function(middle), function(right)
    # Simpson inicial: (b-a)*(f(a)+4*f(m)+f(b))/6.
    whole = (right - left) * (fa + 4 * fm + fb) / 6

    # Subdivide el intervalo, estima el error por delta/15 y reparte tolerancia entre mitades.
    def refine(a, b, fa, fm, fb, estimate, error, depth):
        m = (a + b) / 2
        fl, fr = function((a + m) / 2), function((m + b) / 2)
        first = (m - a) * (fa + 4 * fl + fm) / 6
        second = (b - m) * (fm + 4 * fr + fb) / 6
        # Diferencia de la suma refinada respecto a la aproximación anterior.
        delta = first + second - estimate
        # El factor 15 corresponde al orden de error de Simpson al bisecar.
        if abs(delta) <= 15 * error:
            return first + second + delta / 15
        # Límite de recursión: falla explícitamente si no alcanza tolerancia.
        if depth == 0:
            raise ValueError("La integración de Student t no convergió")
        return (refine(a, m, fa, fl, fm, first, error / 2, depth - 1)
                + refine(m, b, fm, fr, fb, second, error / 2, depth - 1))

    return refine(left, right, fa, fm, fb, whole, tolerance, 24)


@lru_cache(maxsize=None)
# Calcula el cuantil 0.975 de Student t con df grados de libertad; cachea resultados por df.
def t_critical(df):
    if df < 1:
        raise ValueError("Student t requiere al menos un grado de libertad")
    # La densidad transformada es C*cos(theta)**(df-1), para theta >= 0.
    # Cociente de funciones gamma evaluado con logaritmos para estabilizar magnitudes.
    constant = math.exp(math.lgamma((df + 1) / 2) - math.lgamma(df / 2)) / math.sqrt(math.pi)

    # Densidad tras x=sqrt(df)*tan(theta): constante*cos(theta)^(df-1).
    def density(theta):
        return constant * math.cos(theta) ** (df - 1)

    # Intervalo angular de búsqueda; simetría de la densidad deja área objetivo 0.475.
    low, high = 0.0, math.pi / 2
    # Bisección del cuantil transformado, sin depender de una biblioteca estadística externa.
    for _ in range(45):
        middle = (low + high) / 2
        if integrate(density, 0.0, middle) < 0.475:
            low = middle
        else:
            high = middle
    # Transformación inversa de theta al cuantil t.
    return math.sqrt(df) * math.tan((low + high) / 2)


# Valida tiempos positivos y obtiene media, desviación muestral e IC95 bilateral de Student.
def summarize(values):
    n = len(values)
    if n < 2 or any(not math.isfinite(value) or value <= 0 for value in values):
        raise ValueError("Se requieren al menos dos tiempos positivos y finitos")
    mean = statistics.mean(values)
    deviation = statistics.stdev(values)  # Muestral, denominador n-1.
    # Error de la media multiplicado por el cuantil para n-1 grados de libertad.
    half_width = t_critical(n - 1) * deviation / math.sqrt(n)
    return dict(repetitions=n, mean_seconds=mean, sample_stddev_seconds=deviation,
                ci95_low_seconds=mean - half_width, ci95_high_seconds=mean + half_width,
                ci95_relative_half_width_percent=100 * half_width / mean)


# Exige configuración única y repeticiones consecutivas; acepta el esquema secuencial anterior.
def load_samples(path):
    with path.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source)
        expected = ["repetition", *CONFIG_FIELDS, "total_seconds"]
        # Cabecera anterior sin quantum solo se admite para sequential, donde K no aplica.
        legacy = reader.fieldnames == [field for field in expected if field != "quantum"]
        if reader.fieldnames != expected and not legacy:
            raise ValueError("Cabecera de samples.csv incompatible")
        config, values = None, []
        for row in reader:
            if legacy:
                if row.get("model") != "sequential":
                    raise ValueError("Falta quantum para identificar la configuración del modelo")
                row["quantum"] = "0"  # Lotes secuenciales previos a Fine-Grained.
            if None in row or any(value is None for value in row.values()):
                raise ValueError("Fila incompleta o con columnas adicionales")
            current = {field: row[field] for field in CONFIG_FIELDS}
            if config is not None and config != current:
                raise ValueError("El lote mezcla configuraciones diferentes")
            config = current
            # Verifica secuencia 1..n para detectar observaciones ausentes o duplicadas.
            if int(row["repetition"]) != len(values) + 1:
                raise ValueError("Las repeticiones deben ser consecutivas desde 1")
            values.append(float(row["total_seconds"]))
    summarize(values)  # Valida tamaños y valores antes de generar archivos.
    return config, values


# Escribe cabecera y registros mediante DictWriter en UTF-8.
def write_csv(path, rows):
    with path.open("w", newline="", encoding="utf-8") as destination:
        writer = csv.DictWriter(destination, fieldnames=list(rows[0]))
        writer.writeheader()
        writer.writerows(rows)


# Distribución SVG de todas las observaciones, con intervalos según la regla de Sturges.
def histogram(values, config, summary, path):
    """Histograma SVG autónomo; cada barra muestra frecuencia absoluta."""
    # Sturges fija ceil(log2(n)+1) intervalos de frecuencia.
    bins = max(1, math.ceil(math.log2(len(values)) + 1))  # Regla de Sturges.
    low, high = min(values), max(values)
    # Una serie constante recibe un pequeño rango artificial solo para el eje de dibujo.
    if low == high:
        padding = max(abs(low) * 0.01, 1e-12)
        low, high = low - padding, high + padding
    width = (high - low) / bins
    counts = [0] * bins
    # Cuenta cada observación, colocando el máximo en el último intervalo.
    for value in values:
        counts[min(bins - 1, int((value - low) / width))] += 1
    peak = max(counts)
    title = f"Tiempo total — {config['model']} ({config['workers']} trabajador/es)"
    if config.get("quantum", "0") != "0":
        title += f" / quantum {config['quantum']}"
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


# Genera resumen, estimaciones por prefijo, histograma y diferencia de medias de mitades.
def analyze(path):
    config, values = load_samples(path)
    result = summarize(values)
    # Divide la serie por orden temporal para comparar medias inicial y final.
    half = len(values) // 2
    first, second = statistics.mean(values[:half]), statistics.mean(values[half:])
    result["half_means_change_percent"] = 100 * (second - first) / first
    write_csv(path.parent / "summary.csv", [{**config, **result}])
    # Todos los prefijos permiten estudiar estabilidad sin eliminar outliers.
    # Resume todos los prefijos de n=2 a n_total, sin retirar valores atípicos.
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
