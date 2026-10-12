#!/usr/bin/env python3
"""Consolida benchmarks completos en un HTML autónomo, sin dependencias externas."""
import argparse
from html import escape
import json
import math
from pathlib import Path
import random
import statistics
import sys

from analyze import load_samples, summarize, write_csv

MODELS = ('sequential', 'fine-grained', 'coarse-grained', 'smt', 'cmp')
NAMES = dict(zip(MODELS, ('Secuencial', 'Fine-Grained', 'Coarse-Grained', 'SMT aproximado', 'CMP')))
COLORS = dict(zip(MODELS, ('#64748b', '#a78bfa', '#22b8a0', '#e9a23b', '#568bfa')))
INPUTS = ('bodies', 'steps', 'dt', 'seed')


def quantile(values, probability):
    ordered = sorted(values)
    point = (len(ordered) - 1) * probability
    lower = int(point)
    return ordered[lower] + (ordered[math.ceil(point)] - ordered[lower]) * (point - lower)


def speedup_interval(reference, values, draws=2000):
    """Bootstrap independiente del cociente de medias; percentiles 2.5 y 97.5.

    No empareja repeticiones que fueron medidas en procesos/lotes distintos.
    Semilla estable: reproducible para los mismos datos y número de remuestreos.
    """
    rng = random.Random(42)
    ratios = []
    for _ in range(draws):
        numerator = sum(rng.choices(reference, k=len(reference))) / len(reference)
        denominator = sum(rng.choices(values, k=len(values))) / len(values)
        ratios.append(numerator / denominator)
    return quantile(ratios, .025), quantile(ratios, .975)


def environment_key(folder):
    """Separa condiciones conocidas; las ausentes quedan explícitamente unknown.

    No considera iguales equipos distintos por tener el mismo número de núcleos.
    Metadatos históricos no certifican compilación ni política de captura.
    """
    text = (folder / 'environment.txt').read_text(encoding='utf-8') if (folder / 'environment.txt').exists() else ''
    system = next((line for line in text.splitlines() if line.startswith(('Linux ', 'Darwin '))), 'unknown')
    cpu = next((line.split(':', 1)[1].strip() for line in text.splitlines()
                if line.startswith(('Model name:', 'Nombre del modelo:'))), 'unknown')
    topology = (folder / 'topology.txt').read_text(encoding='utf-8').strip() if (folder / 'topology.txt').exists() else 'unknown'
    # Captura cambios de compilación cuando el recolector los registra.
    build = '\n'.join(line for line in text.splitlines()
                      if line.startswith(('compiler=', 'flags=', 'binary_sha256='))) or 'unknown'
    return system, cpu, topology, build


def collect(root):
    groups, excluded = {}, []
    for path in sorted(root.rglob('samples.csv')):
        # summary.csv solo se publica después de completar el bucle del lote.
        if not (path.parent / 'summary.csv').exists():
            excluded.append(f'{path.parent.name}: lote sin análisis final; excluido.')
            continue
        try:
            config, values = load_samples(path)
            if config['model'] not in MODELS:
                raise ValueError('Modelo desconocido')
            if int(config['workers']) < 1:
                raise ValueError('Cantidad de trabajadores no positiva')
            if config['model'] == 'sequential' and int(config['workers']) != 1:
                raise ValueError('Referencia secuencial con más de un trabajador')
            # Normalización numérica evita separar dt=0.002 de dt=0.002000.
            entry = (int(config['bodies']), int(config['steps']), float(config['dt']), int(config['seed']))
            if entry[0] < 1 or entry[1] < 0 or not math.isfinite(entry[2]) or entry[2] <= 0:
                raise ValueError('Entrada física no válida')
            key = entry + environment_key(path.parent)
            variant = (config['model'], int(config['workers']), int(config['quantum']))
            group = groups.setdefault(key, {})
            if variant in group:
                excluded.append(f'{group[variant]["source"]}: reemplazado por el lote más reciente de esa configuración.')
            group[variant] = dict(config=config, values=values, source=str(path.parent), stamp=path.parent.name)
        except (ValueError, OSError, OverflowError) as error:
            excluded.append(f'{path.parent.name}: {error}')
    return groups, excluded


def label(record):
    config = record['config']
    extra = f'q={config["quantum"]}' if config['model'] == 'fine-grained' else f'{config["workers"]} hilo(s)'
    return f'{NAMES[config["model"]]} · {extra}'


def svg_start(title, ylabel):
    return [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 960 420" role="img" aria-label="{escape(title, quote=True)}">',
            f'<title>{escape(title)}</title>',
            '<rect width="960" height="420" rx="12" fill="#fff"/>',
            '<g font-family="system-ui,sans-serif" font-size="12" fill="#475569">',
            f'<text x="22" y="22">{escape(ylabel)}</text>']


def grid(svg, maximum):
    maximum = maximum if maximum > 0 else 1
    for tick in range(6):
        y = 330 - tick * 56
        svg.append(f'<path d="M80 {y}H925" stroke="#e8edf4"/>')
        svg.append(f'<text x="70" y="{y+4}" text-anchor="end">{maximum*tick/5:.4g}</text>')
    return maximum


def bars(records, field, low, high, title, unit):
    svg = svg_start(title, unit)
    valid = [r for r in records if r[field] is not None]
    if not valid:
        svg.append('<text x="80" y="150">Falta una referencia secuencial compatible.</text>')
    else:
        maximum = grid(svg, max(r[high] for r in valid) * 1.15)
        width = 840 / len(valid)
        if field != 'mean_seconds':
            y = 330 - 280 / maximum
            if y >= 50:
                svg.append(f'<path d="M80 {y}H925" stroke="#94a3b8" stroke-dasharray="5 5"><title>Referencia: 1</title></path>')
        for index, r in enumerate(valid):
            x = 80 + width * (index + .5)
            height = 280 * r[field] / maximum
            upper, lower = (330 - 280 * max(0, r[k]) / maximum for k in (high, low))
            tip = escape(f'{r["label"]}: {r[field]:.6g}; IC 95 % [{r[low]:.6g}, {r[high]:.6g}]')
            svg.extend([f'<g><title>{tip}</title><rect x="{x-width*.28}" y="{330-height}" width="{width*.56}" height="{height}" rx="5" fill="{COLORS[r["model"]]}"/>',
                        f'<path d="M{x} {upper}V{lower}M{x-7} {upper}H{x+7}M{x-7} {lower}H{x+7}" stroke="#172033"/>',
                        f'<text x="{x}" y="{min(upper,330-height)-10}" text-anchor="middle">{r[field]:.4g}</text>',
                        f'<text x="{x}" y="355" text-anchor="middle">{escape(NAMES[r["model"]])}</text>',
                        f'<text x="{x}" y="375" text-anchor="middle">p={r["workers"]} · q={r["quantum"]}</text></g>'])
    return ''.join(svg) + '</g></svg>'


def line_chart(series, title, ylabel, xlabel):
    svg = svg_start(title, ylabel)
    points = [point for _, _, data in series for point in data]
    if not points:
        return ''.join(svg) + '<text x="80" y="150">Sin datos suficientes.</text></g></svg>'
    xmin, xmax = min(x for x, y in points), max(x for x, y in points)
    maximum = grid(svg, max(y for x, y in points) * 1.12)
    span = xmax - xmin or 1
    project = lambda x, y: (80 + 840 * (x - xmin) / span, 330 - 280 * y / maximum)
    for name, color, data in series:
        coords = [project(x, y) for x, y in data]
        svg.append(f'<polyline points="{" ".join(f"{x},{y}" for x,y in coords)}" fill="none" stroke="{color}" stroke-width="2"/>')
        # Los puntos conservan valores exactos como tooltip SVG.
        for (x, y), (px, py) in zip(data, coords):
            svg.append(f'<circle cx="{px}" cy="{py}" r="3" fill="{color}"><title>{escape(name)}: {x:g}, {y:.7g}</title></circle>')
    for i in range(6):
        svg.append(f'<text x="{80+i*168}" y="355" text-anchor="middle">{xmin+span*i/5:.4g}</text>')
    svg.append(f'<text x="500" y="403" text-anchor="middle">{escape(xlabel)}</text>')
    for i, (name, color, _) in enumerate(series):
        svg.append(f'<text x="{90+i*260}" y="40" fill="{color}">{escape(name)}</text>')
    return ''.join(svg) + '</g></svg>'


def distribution(values):
    bins = math.ceil(math.log2(len(values)) + 1)
    lo, hi = min(values), max(values)
    if lo == hi:
        lo, hi = lo * .99, hi * 1.01
    width = (hi - lo) / bins
    counts = [0] * bins
    for v in values:
        counts[min(bins - 1, int((v - lo) / width))] += 1
    svg = svg_start('Distribución de los tiempos', 'Frecuencia absoluta')
    maximum = grid(svg, max(counts))
    for i, count in enumerate(counts):
        x, h = 80 + 840 * i / bins, 280 * count / maximum
        svg.append(f'<rect x="{x}" y="{330-h}" width="{840/bins-2}" height="{h}" fill="#568bfa"><title>{count} muestras; [{lo+i*width:.6g}, {lo+(i+1)*width:.6g}] s</title></rect>')
    for i in range(6):
        svg.append(f'<text x="{80+i*168}" y="355" text-anchor="middle">{lo+(hi-lo)*i/5:.5g}</text>')
    return ''.join(svg) + '<text x="500" y="403" text-anchor="middle">Tiempo total (s)</text></g></svg>'


def make_record(lot, baseline, draws):
    config, values = lot['config'], lot['values']
    result = {**config, **summarize(values), 'source': lot['source'], 'label': label(lot)}
    result['workers'], result['quantum'] = int(config['workers']), int(config['quantum'])
    result.update(speedup=None, speedup_ci95_low=None, speedup_ci95_high=None,
                  efficiency=None, efficiency_ci95_low=None, efficiency_ci95_high=None)
    if baseline:
        speedup = statistics.mean(baseline['values']) / result['mean_seconds']
        low, high = (1., 1.) if lot is baseline else speedup_interval(baseline['values'], values, draws)
        result.update(speedup=speedup, speedup_ci95_low=low, speedup_ci95_high=high,
                      efficiency=speedup/result['workers'], efficiency_ci95_low=low/result['workers'],
                      efficiency_ci95_high=high/result['workers'])
    half = len(values) // 2
    result['half_means_change_percent'] = 100 * (statistics.mean(values[half:]) / statistics.mean(values[:half]) - 1)
    result['cv_percent'] = 100 * result['sample_stddev_seconds'] / result['mean_seconds']
    result['histogram'] = distribution(values)
    prefixes = [(n, summarize(values[:n])) for n in range(2, len(values)+1)]
    result['convergence'] = line_chart([
        ('Media acumulada', '#568bfa', [(n, s['mean_seconds']) for n, s in prefixes]),
        ('Límite inferior IC 95 %', '#22b8a0', [(n, max(0, s['ci95_low_seconds'])) for n, s in prefixes]),
        ('Límite superior IC 95 %', '#e9a23b', [(n, s['ci95_high_seconds']) for n, s in prefixes]),
    ], 'Convergencia de la media', 'Segundos (límite inferior recortado a 0 en la gráfica)', 'Repeticiones acumuladas')
    return result


def build_report(root, destination, draws=2000):
    groups, excluded = collect(root)
    if not groups:
        raise ValueError('No hay lotes completos con al menos dos muestras válidas')
    cohorts, export_rows, samples = [], [], []
    def priority(item):
        key, variants = item
        lots = list(variants.values())
        complete = set(MODELS).issubset({v['config']['model'] for v in lots})
        return complete, min(len(v['values']) for v in lots) >= 200, key[0]**2 * (key[1]+1), max(v['stamp'] for v in lots)

    for number, (key, variants) in enumerate(sorted(groups.items(), key=priority, reverse=True), 1):
        lots = sorted(variants.values(), key=lambda r: (MODELS.index(r['config']['model']), int(r['config']['workers']), int(r['config']['quantum'])))
        baseline = next((lot for lot in lots if lot['config']['model'] == 'sequential'), None)
        records = [make_record(lot, baseline, draws) for lot in lots]
        warnings = []
        missing = [NAMES[m] for m in MODELS if m not in {r['model'] for r in records}]
        if missing: warnings.append('Faltan modelos: ' + ', '.join(missing) + '.')
        if not baseline: warnings.append('Sin referencia secuencial compatible: speedup y eficiencia no disponibles.')
        if any(r['repetitions'] < 200 for r in records): warnings.append('Hay configuraciones con menos de 200 ejecuciones.')
        if key[6] == 'unknown': warnings.append('Sin topología registrada: compatibilidad experimental no verificada.')
        if key[7] == 'unknown': warnings.append('Sin identidad de compilación registrada: verificar manualmente compilador, banderas y versión.')
        if 'smt_control=notsupported' in key[6]: warnings.append('SMT de hardware no disponible: estos lotes solo evalúan sobresuscripción por software.')
        warnings.append('Verifique misma política de captura, hardware físico dedicado y ausencia de carga externa; los CSV históricos no certifican estas condiciones.')
        charts = {
            'time': bars(records, 'mean_seconds', 'ci95_low_seconds', 'ci95_high_seconds', 'Tiempo de ejecución', 'Tiempo medio (s) · IC 95 %'),
            'speedup': bars(records, 'speedup', 'speedup_ci95_low', 'speedup_ci95_high', 'Speedup frente al secuencial', 'Speedup (×) · IC 95 %'),
            'efficiency': bars(records, 'efficiency', 'efficiency_ci95_low', 'efficiency_ci95_high', 'Eficiencia paralela', 'Eficiencia (fracción) · IC 95 %'),
        }
        scaling = {}
        for model in MODELS[2:]:
            candidates = [r for r in records if r['model'] == model]
            if len({r['workers'] for r in candidates}) < 2:
                continue
            scaling[model] = {}
            for metric, field, unit in [('time', 'mean_seconds', 'Segundos'), ('speedup', 'speedup', 'Speedup (×)'), ('efficiency', 'efficiency', 'Eficiencia (fracción)')]:
                points = [(r['workers'], r[field]) for r in candidates if r[field] is not None]
                scaling[model][metric] = line_chart([(NAMES[model], COLORS[model], points)], 'Escalabilidad', unit, 'Trabajadores reales (p)')
        cohort_id = f'grupo-{number}'
        for lot, record in zip(lots, records):
            export_rows.append({'group': cohort_id, **{k: v for k, v in record.items() if k not in ('histogram', 'convergence', 'label')}})
            for index, value in enumerate(lot['values'], 1):
                samples.append({'group': cohort_id, 'source': lot['source'], **lot['config'], 'repetition': index, 'total_seconds': value})
        cohorts.append(dict(id=cohort_id, title=f'{key[0]} cuerpos · {key[1]} pasos · dt={key[2]:g} · semilla {key[3]} · {cohort_id}',
                            cpu=key[5], system=key[4], topology=key[6], build=key[7], records=records, charts=charts,
                            scaling=scaling, warnings=warnings, baseline=baseline['source'] if baseline else None))
    destination.parent.mkdir(parents=True, exist_ok=True)
    write_csv(destination.with_suffix('.csv'), export_rows)
    write_csv(destination.with_name(destination.stem + '-samples.csv'), samples)
    template = Path(__file__).with_name('report.html').read_text(encoding='utf-8')
    payload = json.dumps(dict(cohorts=cohorts, excluded=excluded, draws=draws), ensure_ascii=False, allow_nan=False).replace('<', '\\u003c')
    destination.write_text(template.replace('__REPORT_DATA__', payload), encoding='utf-8')
    print(f'Reporte: {destination}\nGrupos comparables: {len(cohorts)}; configuraciones: {len(export_rows)}; lotes excluidos/reemplazados: {len(excluded)}')
    return cohorts


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', nargs='?', type=Path, default=Path('output'))
    parser.add_argument('--output', type=Path, default=Path('output/benchmark-report.html'))
    parser.add_argument('--bootstrap', type=int, default=2000, help='Remuestreos para IC del speedup (mínimo 200)')
    args = parser.parse_args()
    try:
        if args.bootstrap < 200: raise ValueError('Se requieren al menos 200 remuestreos bootstrap')
        build_report(args.root, args.output, args.bootstrap)
    except (ValueError, OSError, OverflowError) as error:
        print(f'Error de reporte: {error}', file=sys.stderr)
        sys.exit(1)
