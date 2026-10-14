#!/usr/bin/env python3
"""Visualiza amplitudes SLC y la fase del resultado complejo exportado por C++."""
import argparse
import html
import json
from pathlib import Path

import numpy as np
from osgeo import gdal
from PIL import Image


INVALID_COLOR = (255, 0, 255)  # Color para los píxeles sin datos válidos.


def read_window(path, args):
    """Lee la ventana solicitada del TIFF y conserva sus muestras complejas."""
    dataset = gdal.Open(str(path))
    if dataset.RasterCount != 1 or not gdal.DataTypeIsComplex(dataset.GetRasterBand(1).DataType):
        raise ValueError(f'Se esperaba una banda compleja: {path}')
    if args.x + args.width > dataset.RasterXSize or args.y + args.height > dataset.RasterYSize:
        raise ValueError(f'Ventana fuera de límites: {path}')
    return dataset.ReadAsArray(args.x, args.y, args.width, args.height)


def phase_image(phase, valid):
    """Representa la fase en radianes con colores y marca los datos inválidos."""
    # Llevar la fase de [-π, π] al intervalo [0, 1] del tono de color.
    hue = np.where(valid, (phase + np.pi) / (2 * np.pi), 0)
    hsv = np.full((*phase.shape, 3), 255, dtype=np.uint8)
    hsv[:, :, 0] = (hue * 255).astype(np.uint8)
    height, width = phase.shape
    image = Image.frombytes('HSV', (width, height), hsv.tobytes())
    rgb = np.array(image.convert('RGB'))
    rgb[~valid] = INVALID_COLOR
    return Image.fromarray(rgb)


def amplitude_image(amplitude, valid, low, high):
    """Convierte la amplitud logarítmica a grises usando los límites de contraste recibidos."""
    scaled = np.clip((amplitude - low) / (high - low), 0, 1)
    gray = (np.where(valid, scaled, 0) * 255).astype(np.uint8)
    rgb = np.repeat(gray[:, :, None], 3, axis=2)
    rgb[~valid] = INVALID_COLOR
    return Image.fromarray(rgb)


def parse_arguments():
    """Recoge las rutas y la ventana de píxeles indicadas en la terminal."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--a', type=Path, default=Path('data/coregistered/P09-R2-split/A.tif'))
    parser.add_argument('--b', type=Path, default=Path('data/coregistered/P09-R2-split/B.tif'))
    parser.add_argument('--result', type=Path, required=True, help='TIFF complejo exportado por C++')
    parser.add_argument('--out', type=Path, default=Path('results'))
    parser.add_argument('--x', type=int, default=12000)
    parser.add_argument('--y', type=int, default=750)
    parser.add_argument('--width', type=int, default=512)
    parser.add_argument('--height', type=int, default=512)
    args = parser.parse_args()
    if args.x < 0 or args.y < 0 or args.width <= 0 or args.height <= 0:
        parser.error('Origen no negativo y dimensiones positivas requeridos.')
    return args


def write_report(args, valid, low, high):
    """Guarda las entradas, la ventana y la escala usadas para reproducir la visualización."""
    report = dict(a=str(args.a.resolve()), b=str(args.b.resolve()), x=args.x, y=args.y,
                  width=args.width, height=args.height, valid_pixels=int(valid.sum()),
                  total_pixels=int(valid.size), amplitude_limits=[float(low), float(high)],
                  amplitude_scale='log(1 + abs(SLC)), percentiles conjuntos 2–98',
                  phase='arg(resultado C++), radianes, sin correcciones topográfica/tierra plana',
                  calculation='Resultado exportado del kernel C++; NumPy solo convierte a fase y amplitud', result=str(args.result.resolve()))
    (args.out / 'visualization.json').write_text(json.dumps(report, indent=2) + '\n')


def write_page(args, valid):
    """Crea el visor HTML con las imágenes generadas y sus leyendas."""
    cards = [('Amplitud A · referencia', 'amplitude_a.png'),
             ('Amplitud B · secundaria', 'amplitude_b.png'),
             ('Fase del producto interferométrico', 'interferogram_phase.png'),
             ('Máscara común · blanco = válido', 'valid_mask.png')]
    figures = ''.join(f'<figure><img src="{file}" alt="{title}"><figcaption>{title}</figcaption></figure>'
                      for title, file in cards)
    page = f'''<!doctype html><html lang="es"><meta charset="utf-8">
<meta name="viewport" content="width=device-width"><title>Sentinel-1 · resultados</title>
<style>body{{font:16px system-ui;background:#101820;color:#edf2f7;max-width:1150px;margin:40px auto;padding:0 24px}}
p{{line-height:1.6}}.grid{{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:24px}}
figure{{margin:0;background:#1b2935;padding:16px;border-radius:10px}}figure img{{width:100%;image-rendering:pixelated}}
figcaption{{padding-top:12px}}.legend{{max-width:512px;width:100%;height:24px}}code{{overflow-wrap:anywhere}}</style>
<h1>Sentinel-1 · imágenes corregistradas</h1>
<p>Ventana {args.width} × {args.height}, origen ({args.x}, {args.y}).
{valid.sum():,} de {valid.size:,} píxeles comunes válidos.</p>
<p>Amplitudes con la misma escala logarítmica y contraste. Magenta = datos inválidos.
Los ejes representan columnas y filas del radar, no distancias en el terreno.</p>
<div class="grid">{figures}</div>
<p>Escala de fase: −π → 0 → +π radianes (colores cíclicos).</p>
<img class="legend" src="phase_legend.png" alt="Escala de fase">
<p>Fase del resultado exportado por el modelo C++, sin corrección de tierra plana ni topografía.
No es un mapa de deformación ni una validación de la precisión del corregistro.</p>
<p>A: <code>{html.escape(str(args.a))}</code><br>B: <code>{html.escape(str(args.b))}</code></p></html>'''
    (args.out / 'index.html').write_text(page, encoding='utf-8')


def main():
    """Lee ambas adquisiciones y genera las imágenes, el reporte y el visor."""
    args = parse_arguments()
    gdal.UseExceptions()
    a = read_window(args.a, args)
    b = read_window(args.b, args)
    # Una posición se muestra solo si ambas fechas tienen datos finitos y no nulos.
    valid = np.isfinite(a) & np.isfinite(b) & (np.abs(a) > 0) & (np.abs(b) > 0)
    if not valid.any():
        raise ValueError('La ventana no contiene muestras comunes válidas.')
    # El logaritmo reduce el contraste entre señales débiles y muy intensas.
    amplitudes = [np.log1p(np.abs(a)), np.log1p(np.abs(b))]
    # Usar el mismo contraste permite comparar las dos fechas.
    samples = np.concatenate([amplitude[valid] for amplitude in amplitudes])
    # Recortar los extremos evita que unos pocos valores dominen la imagen.
    low, high = np.percentile(samples, [2, 98])
    if high <= low:
        high = low + 1
    args.out.mkdir(parents=True, exist_ok=True)
    for name, amplitude in zip(('a', 'b'), amplitudes):
        image = amplitude_image(amplitude, valid, low, high)
        image.save(args.out / f'amplitude_{name}.png')

    # Leer el producto calculado por C++; no recalcular la multiplicación.
    dataset = gdal.Open(str(args.result))
    if dataset.RasterCount != 1 or not gdal.DataTypeIsComplex(dataset.GetRasterBand(1).DataType):
        raise ValueError('El resultado C++ debe ser una banda compleja.')
    if (dataset.RasterXSize, dataset.RasterYSize) != (args.width, args.height):
        raise ValueError('Las dimensiones del resultado deben coincidir con la ventana.')
    metadata = dataset.GetMetadata()
    for key, source in [('SOURCE_A', args.a), ('SOURCE_B', args.b)]:
        if Path(metadata.get(key, '')).resolve() != source.resolve():
            raise ValueError('El resultado no corresponde a las entradas seleccionadas.')
    if metadata.get('OFFSET_X') != str(args.x) or metadata.get('OFFSET_Y') != str(args.y):
        raise ValueError('El resultado no corresponde al origen seleccionado.')
    product = dataset.ReadAsArray()
    valid &= np.isfinite(product)
    phase = np.angle(product)
    phase_image(phase, valid).save(args.out / 'interferogram_phase.png')
    Image.fromarray((valid * 255).astype(np.uint8)).save(args.out / 'valid_mask.png')
    legend = np.linspace(-np.pi, np.pi, 512)[None, :].repeat(24, axis=0)
    phase_image(legend, np.ones(legend.shape, dtype=bool)).save(args.out / 'phase_legend.png')

    write_report(args, valid, low, high)
    write_page(args, valid)
    print(f'Visor: {args.out / "index.html"}; píxeles válidos: {valid.sum()} / {valid.size}')


if __name__ == '__main__':
    main()
