#!/usr/bin/env python3
# Comparación transversal de capturas compatibles de perf.
# La firma combina carga, alcance, modo de conteo, CPU y eventos.
# Los dominios de PMU se muestran separados y la referencia común se cuenta una vez.
"""Compara capturas de perf sin ejecutar renders ni modificar sus CSV."""
import argparse
import csv
import json
import math
from pathlib import Path
import re
import statistics
from profile_perf import assessed_rows, coverage, measurement_warning

CATEGORIES = {
    'ipc': ('Instrucciones por ciclo (IPC)', 'Instrucciones/ciclo'),
    'cycles': ('Ciclos de CPU', 'Ciclos'),
    'instructions': ('Instrucciones completadas', 'Instrucciones'),
    'cache-misses': ('Fallos de caché', 'Fallos'),
    'context-switches': ('Cambios de contexto', 'Cambios'),
    'cpu-migrations': ('Migraciones entre CPUs', 'Migraciones'),
    'task-clock': ('Tiempo CPU acumulado', 'Segundos de CPU'),
    'cpu-clock': ('Tiempo de las CPUs observadas', 'Segundos de CPU'),
}
ORDER = ['sequential', 'fine', 'coarse', 'cmp', 'smt']


# Extrae categoría y dominio de hardware a partir del nombre completo del evento.
def event_kind(event):
    name = event.split('/')[1] if '/' in event else event.split(':')[0]
    category = 'ipc' if event.startswith('IPC derivado:') else name
    domain = 'E-cores (cpu_atom)' if 'cpu_atom/' in event else 'P-cores (cpu_core)' if 'cpu_core/' in event else 'Contador general'
    return category, domain


# Valida una firma común y construye entradas separadas por configuración/CPU/PMU.
def load(captures):
    entries, signature = [], None
    for capture in sorted(set(Path(p).resolve() for p in captures)):
        metadata = json.loads((capture / 'metadata.json').read_text())
        # Las opciones del modelo no forman parte de la carga común registrada.
        # Firma experimental: el orden de parámetros de carga se conserva como fue registrado.
        current = (tuple(metadata['carga_argumentos']), metadata['alcance'],
                   metadata.get('modo_conteo', 'scaled'), tuple(metadata.get('modelos_cpu', [])),
                   metadata['eventos'])
        if signature is not None and current != signature:
            raise ValueError('Las capturas mezclan carga, CPU, eventos, alcance o modo de conteo. Compáralas por separado.')
        signature = current
        with (capture / 'contadores.csv').open(newline='') as stream:
            rows = list(csv.DictReader(stream))
        for row in rows:
            if row['estado'] == 'ok':
                row['valor'] = float(row['valor'])
                if not math.isfinite(row['valor']) or row['valor'] < 0:
                    raise ValueError(f'Contador inválido en {capture}')
        # Agrupación exacta de modelo, CPU, evento y unidad antes de producir una entrada.
        batches = {}
        for row in assessed_rows(rows):
            # Los contadores importados de la referencia ya están en su captura
            # original: no duplicarlos en cada configuración paralela.
            if metadata.get('referencia_compartida') and row['modelo'] == 'sequential':
                continue
            batches.setdefault((row['modelo'], row['cpu'], row['evento'], row['unidad']), []).append(row)
        for (model, cpu, event, unit), batch in batches.items():
            category, domain = event_kind(event)
            if category not in CATEGORIES:
                continue
            command = metadata.get('comandos', {}).get(model, [])
            # P explícito del comando; fine/sequential tienen un hilo y otros pueden ser automáticos.
            workers = int(command[command.index('--threads')+1]) if '--threads' in command else 1 if model in ('fine', 'sequential') else None
            # Recupera el estado SMT informado por Linux, distinto del nombre del modelo.
            state = re.search(r'smt_activo_linux=(\S+)', metadata['topologia'])
            state = {'1': 'on', '0': 'off'}.get(state[1] if state else '', '?')
            entries.append(dict(model=model, workers=workers, state=state, cpu=cpu,
                                category=category, domain=domain, event=event, unit=unit,
                                rows=batch, capture=capture.name))
    if not entries:
        raise ValueError('No hay contadores para comparar.')
    entries.sort(key=lambda e: (ORDER.index(e['model']), e['state'], e['workers'] or 0, e['capture'], e['cpu']))
    return entries, signature


# Dibuja barras horizontales con IC95, cobertura, número de mediciones y avisos.
def draw(ax, entries, title):
    from plot_metrics import COLORS, NAMES, decorate
    labels, maxima = [], [0]
    for i, entry in enumerate(entries):
        rows = [r for r in entry['rows'] if r['estado'] == 'ok']
        values = [r['valor'] for r in rows]
        # perf expresa task-clock sin unidad en nanosegundos en estas capturas.
        # Convierte clocks de milisegundos o nanosegundos a segundos; los demás conteos quedan iguales.
        factor = (1e-3 if entry['unit'] in ('ms', 'msec') else 1e-9) if entry['category'] in ('task-clock', 'cpu-clock') else 1
        values = [v * factor for v in values]
        warning = any(measurement_warning(r) for r in rows)
        active = [coverage(r) for r in rows if coverage(r) is not None]
        label = NAMES[entry['model']] + f" · P={entry['workers'] or 'auto'} · SMT {entry['state']}"
        if sum((e['model'], e['workers'], e['state'], e['cpu']) ==
               (entry['model'], entry['workers'], entry['state'], entry['cpu']) for e in entries) > 1:
            label += '\n' + entry['capture']
        if entry['cpu'] != 'programa': label += ' · ' + entry['cpu']
        label += f"\n{len(values)}/{len(entry['rows'])} mediciones"
        if active: label += f" · medido {min(active):g}–{max(active):g} %"
        if warning: label += ' · AVISO'
        labels.append(label)
        if not values:
            ax.text(.02, i, 'Sin conteo disponible', transform=ax.get_yaxis_transform(), va='center', color='#b45309')
            continue
        mean = statistics.mean(values)
        # Semiancho normal del IC95 con n>1; n=1 no estima intervalo.
        margin = 1.96 * statistics.stdev(values) / math.sqrt(len(values)) if len(values) > 1 else 0
        # El conteo no puede ser negativo aunque el IC normal sí cruce cero.
        # Limita visualmente el extremo inferior a cero, sin cambiar las observaciones.
        bars = ax.barh(i, mean, xerr=[[min(mean, margin)], [margin]], color=COLORS[entry['model']], capsize=3, height=.6)
        if warning:
            bars[0].set_hatch('//')
            bars[0].set_edgecolor('#b45309')
        ax.text(mean + margin, i, f'  {mean:,.3g}', va='center', fontsize=9)
        maxima.append(mean + margin)
    ax.set_yticks(range(len(entries)), labels, fontsize=9)
    ax.set_ylim(len(entries)-.5, -.5)
    ax.set_xlim(0, max(maxima)*1.2 or 1)
    decorate(ax, title, CATEGORIES[entries[0]['category']][1])


# Genera una figura por categoría y un índice Markdown de capturas incluidas.
def generate(entries, signature, output):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from plot_metrics import save
    output.mkdir(parents=True, exist_ok=True)
    mode = 'observado (--no-scale)' if signature[2] == 'observed' else 'escalado por perf (puede extrapolar)'
    note = f'Conteo {mode} · alcance {signature[1]} · barras negras: IC95 aproximado de la media.'
    note += '\nRayado naranja: porcentaje medido bajo o desconocido; valores conservados. P = trabajadores.'
    note += '\nP-cores y E-cores se comparan por separado. IPC mayor o menos eventos no garantiza menor tiempo.'
    files = []
    # Cada categoría tiene paneles independientes por dominio P-core/E-core/general.
    for category, (title, _) in CATEGORIES.items():
        selected = [e for e in entries if e['category'] == category]
        domains = sorted({e['domain'] for e in selected})
        if not domains: continue
        height = max(5, max(sum(e['domain'] == d for e in selected) for d in domains)*.75+2)
        fig, axes = plt.subplots(1, len(domains), figsize=(14*len(domains), height), squeeze=False)
        for ax, domain in zip(axes[0], domains):
            draw(ax, [e for e in selected if e['domain'] == domain], title + ' · ' + domain)
        filename = f'comparacion-{category}.png'
        save(fig, output / filename, note)
        plt.close(fig)
        files.append((title, filename))
    # Índice de figuras y procedencia de las capturas, sin alterar CSV fuente.
    lines = ['# Comparación general de perf', '', 'Carga: `' + ' '.join(signature[0]) + '`.', '', note, '',
             'Cada fila conserva la configuración y sus repeticiones; no se mezclan cantidades de trabajadores ni estados SMT.',
             'Las referencias compartidas se muestran una sola vez desde su captura original.',
             'Campañas independientes pueden tener sesgos por temperatura, frecuencia u orden. Los IC no corrigen esos sesgos.', '',
             '| Categoría | Gráfica |', '| --- | --- |']
    lines += [f'| {title} | [Abrir]({filename}) |' for title, filename in files]
    lines += ['', '## Capturas incluidas', ''] + [f'- `{name}`' for name in sorted({e['capture'] for e in entries})]
    (output / 'README.md').write_text('\n'.join(lines)+'\n', encoding='utf-8')


# Selecciona carpetas explícitas o descubiertas, valida compatibilidad y genera comparación.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('root', type=Path, help='Carpeta perf que contiene las capturas')
    parser.add_argument('--captures', nargs='+', type=Path, help='Capturas explícitas de una campaña')
    args = parser.parse_args()
    try:
        captures = args.captures or [p.parent for p in args.root.glob('*/metadata.json') if (p.parent/'contadores.csv').is_file()]
        entries, signature = load(captures)
        generate(entries, signature, args.root/'comparison')
        print(f'Comparación de perf: {args.root / "comparison"}')
    except (OSError, ValueError, KeyError, ImportError) as error:
        parser.exit(1, f'Error: {error}\n')


if __name__ == '__main__':
    main()
