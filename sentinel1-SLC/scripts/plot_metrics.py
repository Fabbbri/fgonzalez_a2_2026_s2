#!/usr/bin/env python3
"""Analiza los CSV crudos; nunca usa medias como muestras de una distribución."""
import argparse
import csv
import glob
import math
from pathlib import Path
import statistics

# La carga se mantiene fija. K y los bloqueos identifican políticas distintas.
WORKLOAD = ("ancho", "alto", "origen_x", "origen_y", "slc_a_hash", "slc_b_hash", "allow_nodata")
POLICY = ("modelo", "K", "tile_ancho", "tile_alto")

# Filas (total, secuencial, speedup, eficiencia) de una configuración.
# baseline guarda los tiempos de la referencia secuencial compartida, si la hay.
class Measurements(list):
    baseline = None


def metric_estimate(rows, index, scale=1):
    """Media, desviación y semiancho del IC95 de una métrica (0=tiempo, 1=secuencial, 2=speedup, 3=eficiencia).

    Con referencia compartida, speedup y eficiencia se estiman como cociente de medias
    y su incertidumbre se propaga con el método delta.
    """
    if rows.baseline is None:
        return estimate([r[index] * scale for r in rows])
    if index == 1:
        return estimate([t * scale for t in rows.baseline])
    if index == 0:
        return estimate([r[0] * scale for r in rows])
    bm, bsd, _ = estimate(rows.baseline)
    tm, tsd, _ = estimate([r[0] for r in rows])
    ratio = bm / tm
    factor = scale / (rows.workers if index == 3 else 1)
    vb, vt = (bsd / bm)**2, (tsd / tm)**2
    return (ratio * factor, ratio * math.sqrt(vb + vt) * factor,
            1.96 * ratio * math.sqrt(vb / len(rows.baseline) + vt / len(rows)) * factor)


def estimate(values):
    """Media, desviación estándar e IC95 aproximado con distribución normal."""
    mean = statistics.mean(values)
    sd = statistics.stdev(values)
    return mean, sd, 1.96 * sd / math.sqrt(len(values))


def load(patterns):
    """Lee los CSV crudos, valida su consistencia y los agrupa por (política, hilos)."""
    paths = sorted({Path(p) for pattern in patterns for p in glob.glob(pattern)})
    if not paths:
        raise ValueError("No hay CSV que coincidan con la ruta indicada")
    groups, workload = {}, None
    baseline_cache = {}
    for path in paths:
        with path.open(newline="") as stream:
            reader = csv.DictReader(stream)
            # Un resumen no contiene iteraciones y no sirve para un boxplot.
            if "repeticion" not in (reader.fieldnames or []):
                continue
            for row in reader:
                if row.get("alcance_tiempo") != "preparacion_creacion_calculo_sincronizacion_cierre_sin_io_hash":
                    raise ValueError("CSV con alcance temporal incompatible con el benchmark del modelo")
                if not 0 < float(row["tiempo_kernel_ms"]) <= float(row["tiempo_total_ms"]):
                    raise ValueError("El tiempo del kernel debe ser positivo y no superar el del modelo")
                current = tuple(row[k] for k in WORKLOAD)
                if workload is not None and current != workload:
                    raise ValueError("Los CSV mezclan cargas diferentes; grafícalas por separado")
                workload = current
                policy = tuple(row[k] for k in POLICY)
                # También separar referencias sequential medidas en estados distintos.
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
                data = groups.setdefault((policy, workers), Measurements())
                data.workers = workers
                shared = row.get("referencia_tipo") == "compartida"
                if data and shared != (data.baseline is not None):
                    raise ValueError("No mezclar referencias compartidas y pareadas")
                if shared:
                    reference_path = (path.parent / row["referencia_csv"]).resolve()
                    if reference_path not in baseline_cache:
                        with reference_path.open(newline="") as reference:
                            baseline_cache[reference_path] = list(csv.DictReader(reference))
                    baseline_rows = baseline_cache[reference_path]
                    if len(baseline_rows) < 2 or any(
                        r["modelo"] != "sequential" or tuple(r[k] for k in WORKLOAD) != current
                        or r["resultado_hash"] != row["resultado_hash"]
                        or r["referencia_smt"] != row["referencia_smt"] for r in baseline_rows):
                        raise ValueError("Referencia compartida incompatible")
                    baseline = [float(r["tiempo_total_ms"]) for r in baseline_rows]
                    if not all(math.isfinite(t) and t > 0 for t in baseline):
                        raise ValueError("Tiempos de referencia inválidos")
                    if not math.isclose(statistics.mean(baseline), base, rel_tol=1e-8):
                        raise ValueError("Tiempo secuencial no coincide con la media compartida")
                    if data.baseline is not None and data.baseline != baseline:
                        raise ValueError("No mezclar referencias compartidas diferentes")
                    data.baseline = baseline
                data.append((total, base, speedup, speedup / workers))
    if not groups or any(len(rows) < 2 for rows in groups.values()):
        raise ValueError("Se necesitan al menos dos iteraciones crudas por configuración")
    return dict(sorted(groups.items())), workload


def label(key):
    """Etiqueta corta de una configuración para los ejes."""
    policy, workers = key
    model, quantum, interval, wait = policy[:4]
    detail = f"quantum={quantum} píxeles" if model == "fine" else f"tiles={interval}×{wait}" if model == "coarse" else "base"
    if len(policy) > 4:
        detail = policy[4] if model in ("smt", "cmp") else detail + " · " + policy[4]
    return f"{model} · P={workers}\n{detail}"


COLORS = {"sequential": "#64748b", "fine": "#8b5cf6", "coarse": "#e69519",
          "cmp": "#0891b2", "smt": "#d94b73"}
NAMES = {"sequential": "Secuencial", "fine": "Fine", "coarse": "Coarse", "cmp": "CMP", "smt": "SMT"}


def readable(key):
    """Descripción legible de una configuración para títulos y leyendas."""
    policy, workers = key
    model = policy[0]
    name = NAMES.get(model, model)
    detail = "1 hilo real" if model == "fine" else f"{workers} trabajador" + ("es" if workers != 1 else "")
    if model == "fine":
        detail += f" · quantum={policy[1]} píxeles"
    elif model == "coarse":
        detail += f" · tiles {policy[2]}×{policy[3]}"
    elif len(policy) > 4:
        detail += "\n" + policy[4].replace("C=", "núcleos: ").replace(" L=", " · CPUs lógicas: ").replace(" SMT=1", " · SMT activo").replace(" SMT=0", " · SMT inactivo")
    return f"{name} · {detail}"


def sample_caption(key, data, reference=False):
    n = len(data.baseline) if reference and data.baseline is not None else len(data)
    text = f"{readable(key)} · n={n}"
    if not reference and data.baseline is not None:
        text += f" · n referencia={len(data.baseline)}"
    return text


def decorate(ax, title, xlabel):
    """Estilo común de los ejes."""
    ax.set_title(title, loc="left", fontsize=15, fontweight="bold", pad=20)
    ax.set_xlabel(xlabel, fontsize=11)
    ax.set_axisbelow(True)
    ax.grid(axis="x", alpha=.18)
    for side in ("top", "right"):
        ax.spines[side].set_visible(False)


def save(fig, path, note):
    """Agrega la nota al pie y guarda la figura en PNG."""
    fig.text(.02, .02, note, fontsize=9, color="#475569")
    fig.tight_layout(rect=(0, .09, 1, 1))
    fig.savefig(path, dpi=170, facecolor="white")


def fit_amdahl(points):
    """Ajuste descriptivo de T(P)/Tseq = f + (1-f)/P, sin intercepto libre.

    Cada configuración P pesa lo mismo. f también absorbe sobrecarga: no es
    una medición directa del porcentaje de instrucciones secuenciales.
    """
    if len({p for p, _ in points}) < 2:
        return None
    if any(p < 1 or not math.isfinite(s) or s <= 0 for p, s in points):
        raise ValueError("P y speedup deben ser positivos para Amdahl")
    denominator = sum((1-1/p)**2 for p, _ in points)
    if not denominator:
        return None
    raw = sum((1-1/p)*(1/s-1/p) for p, s in points) / denominator
    fraction = min(1.0, max(0.0, raw))
    rmse = math.sqrt(sum((1/s-(fraction+(1-fraction)/p))**2 for p, s in points)/len(points))
    return fraction, raw, rmse


def generate_amdahl(groups, workload, output):
    """Ajusta Amdahl a cada modelo y escribe amdahl.png y amdahl.md."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    output.mkdir(parents=True, exist_ok=True)
    policies = sorted({k[0] for k in groups if k[0][0] != "sequential"})
    report = ["# Ley de Amdahl", "",
              "Carga: " + ", ".join(f"{k}={v}" for k,v in zip(WORKLOAD,workload)) + ".", "",
              "S(P) = 1 / [f + (1-f)/P]. P es el número de trabajadores; f es una fracción secuencial efectiva.",
              "El ajuste minimiza la suma de errores cuadrados de 1/S(P) = f + (1-f)/P, "
              "con igual peso por configuración y f restringida a [0,1]. No impone que el modelo con P=1 tenga speedup 1.",
              "No mide directamente código secuencial: puede absorber creación de hilos, esperas, contención, "
              "carga desigual y efectos de hardware. SMT puede compartir recursos y los modelos añaden sincronización; "
              "Amdahl es una referencia idealizada, no una validación de esos modelos ni una predicción garantizada.",
              "Las barras negras son IC95 del speedup medido; la curva ajustada no incluye un intervalo de confianza. "
              "El límite 1/f es teórico y no implica que el equipo disponga de suficientes núcleos.", "",
              "| Política | P medidos | f efectiva | Límite teórico 1/f | RMSE en tiempo relativo | Nota |",
              "| --- | --- | ---: | ---: | ---: | --- |"]
    if not policies:
        report += ["", "Solo hay referencia sequential: faltan modelos para comparar."]
        (output/"amdahl.md").write_text("\n".join(report)+"\n",encoding="utf-8")
        return
    columns = min(2, len(policies))
    fig, axes = plt.subplots(math.ceil(len(policies)/columns), columns,
                             figsize=(8*columns,5*math.ceil(len(policies)/columns)+1), squeeze=False)
    for ax, policy in zip(axes.flat, policies):
        keys = sorted((k for k in groups if k[0] == policy), key=lambda k:k[1])
        ps = [k[1] for k in keys]
        stats = [metric_estimate(groups[k],2) for k in keys]
        points = list(zip(ps,[s[0] for s in stats]))
        worker_word = "trabajador" if ps[0] == 1 else "trabajadores"
        caption = readable(keys[0]).replace(f" · {ps[0]} {worker_word}", "").replace(" · 1 hilo real", "")
        fit = fit_amdahl(points)
        max_p = max(2,max(ps))
        curve = [1+(max_p-1)*i/200 for i in range(201)]
        ax.plot(curve,curve,"--",color="#94a3b8",label="Ideal lineal: S = P")
        ax.axhline(1,color="#64748b",linestyle=":",label="Sequential = 1×")
        ax.errorbar(ps,[s[0] for s in stats],yerr=[s[2] for s in stats],fmt="o",
                    color=COLORS[policy[0]],capsize=4,label="Medido ± IC95")
        for key, p, stat in zip(keys, ps, stats):
            ax.annotate(f"n={len(groups[key])}", (p, stat[0]), xytext=(5, 8),
                        textcoords="offset points", fontsize=8)
        if fit is not None:
            f, raw, rmse = fit
            limit = f"{1/f:.2f}×" if f else "sin cota finita en este modelo"
            ax.plot(curve,[1/(f+(1-f)/p) for p in curve],color=COLORS[policy[0]],
                    label=f"Amdahl ajustado: f = {f:.1%}\nLímite teórico: {limit}")
            note = "Ajuste restringido; f sin restringir fuera de [0,1]" if not 0 <= raw <= 1 else "Ajuste descriptivo"
            report.append(f"| {caption.replace(chr(10),' · ')} | {ps} | {f:.4%} | {limit} | {rmse:.5f} | {note} |")
        else:
            ax.text(.03,.25,"Un solo P: no se estima f.\nFine usa un único ejecutor real." if policy[0]=="fine" else "Un solo P: faltan puntos para ajustar f.",
                    transform=ax.transAxes,fontsize=10)
            report.append(f"| {caption.replace(chr(10),' · ')} | {ps} | — | — | — | Falta barrido de P |")
        decorate(ax,caption,"Trabajadores P")
        ax.set_ylabel("Speedup frente a sequential (×)")
        ax.set_xticks(sorted(set([1]+ps)))
        ax.set_ylim(bottom=0)
        ax.legend(fontsize=9,loc="upper left")
    for ax in list(axes.flat)[len(policies):]:
        ax.set_visible(False)
    save(fig,output/"amdahl.png","Amdahl: S(P) = 1 / [f + (1-f)/P]. Misma carga por comparación.\n"
         "f es un ajuste efectivo, no una fracción de código medida. Curva sin IC; puntos con IC95 aproximado.\n"
         "No se extrapola la curva fuera del rango de P mostrado. Revisar limitaciones en amdahl.md.")
    plt.close(fig)
    report += ["", "![Comparación con Amdahl](amdahl.png)"]
    (output/"amdahl.md").write_text("\n".join(report)+"\n",encoding="utf-8")


def generate(groups, workload, output):
    """Genera las gráficas de tiempo, speedup y eficiencia y el reporte de la carpeta."""
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError as error:
        raise ValueError("Falta Matplotlib. En Ubuntu: sudo apt install python3-matplotlib") from error
    output.mkdir(parents=True, exist_ok=True)
    keys = sorted(groups, key=lambda k: (list(NAMES).index(k[0][0]), k[0], k[1]))
    captions = [sample_caption(k, groups[k]) for k in keys]
    colors = [COLORS[k[0][0]] for k in keys]
    width, height, x, y, hash_a, hash_b, nodata = workload
    context = f"Sentinel: ventana {width}×{height} · origen ({x}, {y}) · NoData={nodata} · tiempo del modelo"
    shared = any(rows.baseline is not None for rows in groups.values())
    comparison = len({k[0][0] for k in keys}) > 1
    confidence = ("IC95 aproximado; speedup compartido = cociente de medias (método delta)." if shared else
                  "Barras de error: IC95 aproximado de la media.") + " n indica las repeticiones leídas del CSV."
    metrics = [(0, "Tiempo (segundos)", "tiempo", .001),
               (2, "Aceleración frente a secuencial (×)", "speedup", 1),
               (3, "Eficiencia paralela (%)", "eficiencia", 100)]

    for index, title, name, scale in metrics:
        values = [[r[index] * scale for r in groups[k]] for k in keys]
        estimates = [metric_estimate(groups[k], index, scale) for k in keys]
        fig, ax = plt.subplots(figsize=(12, max(5, len(keys) * .8 + 2)))
        positions = list(range(len(keys)))
        if index == 0 and comparison:
            # Sequential ya aparece como una configuración: no repetirlo por modelo.
            bars = ax.barh(positions, [s[0] for s in estimates], height=.55,
                           xerr=[s[2] for s in estimates], color=colors, capsize=4)
            question = "¿Cuál termina antes? · Menos tiempo es mejor"
        elif index == 0:
            # Una referencia por comparación, sin mezclar baselines de campañas distintas.
            bases = [metric_estimate(groups[k], 1, scale) for k in keys]
            reference_bars = ax.barh([p-.18 for p in positions], [s[0] for s in bases], height=.32,
                    xerr=[s[2] for s in bases], color=COLORS["sequential"], capsize=3, label="Referencia secuencial compartida" if shared else "Referencia secuencial del par")
            ax.bar_label(reference_bars, labels=[f"{s[0]:.3f} s" for s in bases], label_type="center", color="white", fontsize=9)
            bars = ax.barh([p+.18 for p in positions], [s[0] for s in estimates], height=.32,
                           xerr=[s[2] for s in estimates], color=colors, capsize=3, label="Modelo evaluado")
            ax.legend(loc="upper right", bbox_to_anchor=(1,1.035), fontsize=9, ncol=2)
            question = "¿Cuál termina antes? · Menos tiempo es mejor"
        else:
            bars = ax.barh(positions, [s[0] for s in estimates], height=.55,
                           xerr=[s[2] for s in estimates], color=colors, capsize=4)
            reference = 1 if index == 2 else 100
            ax.axvline(reference, color="#64748b", linestyle="--", label="Secuencial = 1×" if index == 2 else "Eficiencia ideal = 100 %")
            ax.legend(loc="upper right", bbox_to_anchor=(1,1.035), fontsize=9, ncol=2)
            question = "¿Cuánto acelera? · Más de 1× supera a secuencial" if index == 2 else "¿Cuánta aceleración aporta cada trabajador?"
        ax.bar_label(bars, labels=[f"{s[0]:.3f} s" if index == 0 else f"{s[0]:.2f}×" if index == 2 else f"{s[0]:.1f} %" for s in estimates], padding=8, fontsize=10)
        ax.set_yticks(positions, captions, fontsize=9)
        ax.invert_yaxis()
        upper = max(s[0] + s[2] for s in estimates)
        if index == 0:
            if not comparison:
                upper = max(upper, max(s[0] + s[2] for s in bases))
        else:
            upper = max(upper, reference)
        ax.set_xlim(0, upper * 1.25)
        decorate(ax, question, title)
        save(fig, output / f"comparacion-{name}.png", context + "\n" + confidence)
        plt.close(fig)

        fig, ax = plt.subplots(figsize=(12, max(5, len(keys) * .65 + 2)))
        boxes = ax.boxplot(values, vert=False, patch_artist=True)
        for box, color in zip(boxes["boxes"], colors):
            box.set_facecolor(color)
            box.set_alpha(.65)
        ax.set_yticks(range(1, len(keys)+1), captions, fontsize=9)
        ax.invert_yaxis()
        decorate(ax, "¿Qué tan constantes son las ejecuciones?", title)
        distribution_note = "\nCon referencia compartida, los cocientes por ejecución son descriptivos; el speedup global es el cociente de medias." if shared and index in (2,3) else ""
        save(fig, output / f"boxplot-{name}.png", context + "\nCaja: mitad central · línea: mediana · puntos: valores atípicos (se conservan)." + distribution_note)
        plt.close(fig)

        fig, axes = plt.subplots(len(keys), 1, figsize=(10, 2.7 * len(keys)), squeeze=False)
        for key, axes_row, color, data in zip(keys, axes, colors, values):
            ax = axes_row[0]
            ax.hist(data, bins="auto", color=color, edgecolor="white")
            ax.set_title(sample_caption(key, groups[key]), fontsize=10)
            ax.set_xlabel(title)
            ax.set_ylabel("Número de ejecuciones")
        save(fig, output / f"histograma-{name}.png", context + distribution_note)
        plt.close(fig)

    fig, ax = plt.subplots(figsize=(12, max(5, len(keys) * .65 + 2)))
    ax.boxplot([[t/1000 for t in groups[k].baseline] if groups[k].baseline is not None else [r[1]/1000 for r in groups[k]] for k in keys], vert=False)
    ax.set_yticks(range(1, len(keys)+1), captions, fontsize=9)
    ax.invert_yaxis()
    ax.set_yticks(range(1, len(keys)+1), [sample_caption(k, groups[k], reference=True) for k in keys], fontsize=9)
    decorate(ax, "Referencia secuencial de cada comparación", "Tiempo (segundos)")
    save(fig, output / "boxplot-base-secuencial.png", context)
    plt.close(fig)

    policies = sorted({key[0] for key in keys})
    for index, title, name, scale in metrics:
        fig, ax = plt.subplots(figsize=(11, 6))
        for policy in policies:
            selected = sorted([k for k in keys if k[0] == policy], key=lambda k: k[1])
            ps = [k[1] for k in selected]
            stats = [metric_estimate(groups[k], index, scale) for k in selected]
            worker_word = "trabajador" if ps[0] == 1 else "trabajadores"
            caption = readable(selected[0]).replace(f" · {ps[0]} {worker_word}", "").replace(" · 1 hilo real", "")
            # Un único P se muestra como punto, nunca como una curva de escalabilidad.
            ax.errorbar(ps, [s[0] for s in stats], yerr=[s[2] for s in stats],
                        marker="o", linestyle="-" if len(ps)>1 else "none", capsize=4,
                        color=COLORS[policy[0]], label=caption)
            for key, p, stat in zip(selected, ps, stats):
                ax.annotate(f"n={len(groups[key])}", (p, stat[0]), xytext=(5, 8),
                            textcoords="offset points", fontsize=8, color=COLORS[policy[0]])
        ps = sorted({k[1] for k in keys})
        if index == 2:
            ax.plot(ps, ps, "--", color="#94a3b8", label="Ideal: aceleración = trabajadores")
            ax.axhline(1, color="#64748b", linestyle=":", label="Referencia secuencial = 1×")
        elif index == 3:
            ax.axhline(100, color="#94a3b8", linestyle="--", label="Ideal: 100 %")
        decorate(ax, "¿Cómo cambia el rendimiento al añadir trabajadores?", "Cantidad de trabajadores reales")
        ax.set_ylabel(title)
        ax.set_xticks(ps)
        ax.legend(fontsize=9)
        filename = "escalabilidad.png" if index == 2 else f"escalabilidad-{name}.png"
        save(fig, output / filename, context + "\n" + confidence + " Un solo P representa un punto.")
        plt.close(fig)

    report = ["# Análisis de repeticiones", "",
              "Carga fija: " + ", ".join(f"{k}={v}" for k, v in zip(WORKLOAD, workload)) + ".", "",
              "Gráficas principales: comparacion-tiempo.png (segundos, menos es mejor), "
              "comparacion-speedup.png (1× es la referencia) y escalabilidad.png (variar trabajadores). "
              "Las barras muestran estimaciones con IC95 aproximado. En la comparación de modelos, "
              "el tiempo secuencial aparece en su propia fila, sin repetirlo junto a cada modelo. "
              "Los boxplots e histogramas muestran variabilidad y todas las repeticiones.", "",
              "Se incluyen todas las iteraciones, sin eliminar valores atípicos.",
              ("Sequential se mide una sola vez y se reutiliza. Speedup = media sequential / media modelo. "
               "Su IC95 usa el método delta y considera la incertidumbre de ambas medias. "
               "No se interpreta cada cociente por ejecución como una observación independiente de la referencia. "
               "El boxplot de referencia repite los mismos datos originales, no nuevas mediciones. "
               "Una referencia única entre estados SMT no aísla el efecto del hardware: registrar su estado y discutir ese sesgo."
               if shared else "La referencia sequential se compara consigo misma: speedup y eficiencia son 1."), "",
              "| Configuración | n | n referencia | Tiempo medio ± IC95 (ms) | Desviación (ms) | Speedup medio ± IC95 | Desviación speedup |",
              "| --- | ---: | --- | ---: | ---: | ---: | ---: |"]
    for key, rows in groups.items():
        tm, sd, margin = metric_estimate(rows, 0)
        sm, ssd, smargin = metric_estimate(rows, 2)
        reference_n = len(rows.baseline) if rows.baseline is not None else len(rows)
        report.append(f"| {label(key).replace(chr(10), ' · ')} | {len(rows)} | {reference_n} | {tm:.4f} ± {margin:.4f} | {sd:.4f} | {sm:.4f} ± {smargin:.4f} | {ssd:.4f} |")
    report += ["", "## Justificación y alcance", "",
               "El tamaño n se obtiene de las filas del CSV para cada modelo, política, estado SMT "
               "y cantidad de trabajadores. n referencia cuenta las mediciones secuenciales utilizadas. "
               "No se supone una cantidad fija ni se completan muestras artificialmente. "
               "El margen normal aproximado es 1.96 × desviación muestral / sqrt(n); "
               "al aumentar n disminuye el error estándar si las muestras son independientes. "
               "Evaluar la precisión con el margen relativo (margen/media) de tiempo y speedup; "
               "un número fijo de repeticiones no garantiza precisión suficiente.", "",
               "Registrar CPU, núcleos/hilos lógicos, sistema operativo, compilador, flags y condiciones "
               "de carga del equipo en el informe. Mantener TIFF A/B, ventana, tiles, quantum y política NoData constantes durante cada barrido. El calentamiento no se cuenta. El tiempo principal incluye preparación, creación de trabajadores, cálculo, sincronización y cierre; excluye lectura de TIFF, exportación, resumen y huella. El CSV también registra el tiempo del proceso completo por separado, sin exportar TIFF durante las mediciones. "
               "Las ejecuciones consecutivas pueden presentar correlación o cambios térmicos: el IC "
               "aproximado no corrige esos sesgos. Las mediciones principales deben hacerse en hardware físico."]
    generate_amdahl(groups, workload, output)
    report += ["", "La comparación con la ley de Amdahl está en [amdahl.md](amdahl.md)."]
    (output / "reporte.md").write_text("\n".join(report) + "\n", encoding="utf-8")
    print(f"Gráficas y reporte: {output}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", nargs="+", help="CSV crudos o patrones; los resúmenes se omiten")
    parser.add_argument("--output-dir", type=Path, default=Path("resultsen/plots"))
    parser.add_argument("--amdahl-only", action="store_true", help="Generar solo amdahl.png y su explicación")
    args = parser.parse_args()
    try:
        groups, workload = load(args.csv)
        if args.amdahl_only:
            generate_amdahl(groups, workload, args.output_dir)
            print(f"Amdahl: {args.output_dir}")
        else:
            generate(groups, workload, args.output_dir)
    except (ValueError, KeyError, OSError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
