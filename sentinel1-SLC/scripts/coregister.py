#!/usr/bin/env python3
"""Corregistro SNAP y exportación de I/Q a dos TIFF complejos."""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import sys
import xml.etree.ElementTree as ET


def source_path(value):
    path = Path(value).resolve()
    if path.is_dir():
        path /= 'manifest.safe'
    if not path.is_file():
        raise ValueError(f'Producto inexistente: {path}')
    if path.name != 'manifest.safe' and path.suffix.lower() != '.zip':
        raise ValueError('Usa un directorio SAFE, manifest.safe o ZIP SLC con metadatos.')
    return path


def burst_range(value):
    try:
        first, last = map(int, value.split(':'))
        if first < 1 or last < first:
            raise ValueError()
        return first, last
    except ValueError:
        raise argparse.ArgumentTypeError('Usa PRIMERO:ULTIMO, índices desde 1.')


def graph(a, b, out, ranges, swath, pol, dem, already_split=False):
    root = ET.Element('graph', id='SLC-coregistration')
    ET.SubElement(root, 'version').text = '1.0'

    def node(name, operator, sources=(), **params):
        n = ET.SubElement(root, 'node', id=name)
        ET.SubElement(n, 'operator').text = operator
        s = ET.SubElement(n, 'sources')
        for i, ref in enumerate(sources):
            ET.SubElement(s, 'sourceProduct' if i == 0 else f'sourceProduct.{i}', refid=ref)
        p = ET.SubElement(n, 'parameters', {'class': 'com.bc.ceres.binding.dom.XppDomElement'})
        for key, value in params.items():
            ET.SubElement(p, key).text = str(value)

    for role, path, (first, last) in zip(('A', 'B'), (a, b), ranges):
        node('Read' + role, 'Read', file=path)
        source = 'Read' + role
        if not already_split:
            node('Split' + role, 'TOPSAR-Split', [source], subswath=swath,
                 selectedPolarisations=pol, firstBurstIndex=first, lastBurstIndex=last)
            source = 'Split' + role
        node('Orbit' + role, 'Apply-Orbit-File', [source],
             orbitType='Sentinel Precise (Auto Download)', polyDegree=3, continueOnFail='false')
    node('Coreg', 'Back-Geocoding', ['OrbitA', 'OrbitB'], demName=dem,
         demResamplingMethod='BILINEAR_INTERPOLATION', resamplingType='BISINC_21_POINT_INTERPOLATION',
         maskOutAreaWithoutElevation='true', outputDerampDemodPhase='false', disableReramp='false')
    last = 'Coreg'
    if ranges[0][1] > ranges[0][0]:
        node('ESD', 'Enhanced-Spectral-Diversity', [last])
        last = 'ESD'
    node('Write', 'Write', [last], file=out / 'stack.dim', formatName='BEAM-DIMAP')
    ET.indent(root)
    return ET.ElementTree(root)


def export_stack(out):
    import numpy as np
    from osgeo import gdal
    gdal.UseExceptions()
    files = list((out / 'stack.data').glob('*.img'))
    datasets = []
    selected = []
    for role, aliases in (('reference', ('mst', 'ref')), ('secondary', ('slv', 'sec'))):
        for component in ('i', 'q'):
            matches = [p for p in files if p.stem.startswith(component + '_')
                       and any(t.startswith(aliases) for t in p.stem.split('_'))]
            if len(matches) != 1:
                raise ValueError(f'Se esperaba una banda {component}/{role}; encontradas: {matches}')
            selected.append(str(matches[0]))
            datasets.append(gdal.Open(str(matches[0])))
    dims = {(d.RasterXSize, d.RasterYSize) for d in datasets}
    if len(dims) != 1:
        raise ValueError('Las cuatro bandas I/Q no comparten dimensiones.')
    width, height = dims.pop()
    outputs = []
    for name in ('A', 'B'):
        ds = gdal.GetDriverByName('GTiff').Create(str(out / (name + '.tif')), width, height,
              1, gdal.GDT_CFloat32, options=['TILED=YES', 'COMPRESS=DEFLATE', 'BIGTIFF=IF_SAFER'])
        ds.SetMetadataItem('PROCESSING', 'SNAP Back-Geocoding; see graph.xml and processing.json')
        ds.GetRasterBand(1).SetNoDataValue(float('nan'))
        outputs.append(ds)
    valid_count = 0
    for y in range(0, height, 128):
        rows = min(128, height - y)
        arrays = [d.ReadAsArray(0, y, width, rows).astype(np.float32) for d in datasets]
        valid = np.logical_and.reduce([np.isfinite(v) for v in arrays])
        for k in (0, 2):
            # SNAP's zero-filled margins are not observations. Use a common mask.
            valid &= (arrays[k] != 0) | (arrays[k + 1] != 0)
        for d, v in zip(datasets, arrays):
            nodata = d.GetRasterBand(1).GetNoDataValue()
            if nodata is not None and not np.isnan(nodata) and nodata != 0:
                valid &= v != nodata
        valid_count += int(valid.sum())
        for k, ds in zip((0, 2), outputs):
            values = arrays[k].astype(np.complex64)
            values.imag = arrays[k + 1]
            values[~valid] = complex(float('nan'), float('nan'))
            ds.GetRasterBand(1).WriteArray(values, 0, y)
    for ds in outputs:
        ds.FlushCache()
    outputs.clear()
    if valid_count == 0:
        raise ValueError('El stack no contiene muestras comunes válidas.')
    return {'width': width, 'height': height, 'valid_pixels': valid_count, 'iq_bands': selected}


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--a', required=True, help='SAFE o ZIP de referencia')
    p.add_argument('--b', required=True, help='SAFE o ZIP secundario')
    p.add_argument('--out', required=True, type=Path, help='Directorio NUEVO de salida')
    p.add_argument('--bursts-a', type=burst_range, default=(1, 1))
    p.add_argument('--bursts-b', type=burst_range, default=(1, 1))
    p.add_argument('--swath', choices=('IW1', 'IW2', 'IW3'), default='IW2')
    p.add_argument('--pol', choices=('VV', 'VH', 'HH', 'HV'), default='VV')
    p.add_argument('--dem', default='SRTM 1Sec HGT')
    p.add_argument('--gpt', default='gpt', help='Ruta al ejecutable de SNAP')
    p.add_argument('--cache', default='2G', help='Caché de GPT; no es el límite de heap Java')
    p.add_argument('--already-split', action='store_true', help='SAFE de un solo burst ya extraído; omitir TOPSAR-Split')
    p.add_argument('--dry-run', action='store_true', help='Solo validar entradas y escribir graph.xml')
    args = p.parse_args()
    a, b = source_path(args.a), source_path(args.b)
    if a == b:
        raise ValueError('Selecciona dos adquisiciones diferentes.')
    ranges = (args.bursts_a, args.bursts_b)
    if ranges[0][1] - ranges[0][0] != ranges[1][1] - ranges[1][0]:
        raise ValueError('Selecciona igual cantidad de bursts correspondientes en A y B.')
    if args.already_split and ranges != ((1, 1), (1, 1)):
        raise ValueError('--already-split admite únicamente un burst por producto (1:1).')
    if not args.dry_run:
        if not shutil.which(args.gpt):
            raise ValueError('No se encontró SNAP GPT. Usa --gpt /ruta/snap/bin/gpt.')
        try:
            import numpy
            from osgeo import gdal
        except ImportError as e:
            raise ValueError('Faltan numpy/GDAL para Python (python3-numpy y python3-gdal).') from e
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=False)
    graph(a, b, out, ranges, args.swath, args.pol, args.dem, args.already_split).write(
        out / 'graph.xml', encoding='utf-8', xml_declaration=True)
    print(f'Grafo: {out / "graph.xml"}', flush=True)
    if args.dry_run:
        return
    command = [args.gpt, str(out / 'graph.xml'), '-c', args.cache]
    with (out / 'gpt.log').open('w') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    report = export_stack(out)
    report.update(reference=str(a), secondary=str(b), bursts=ranges, command=command,
                  already_split=args.already_split,
                  esd=ranges[0][1] > ranges[0][0], quality='Corregistro ejecutado; calidad no validada científicamente')
    (out / 'processing.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Listo: {out / "A.tif"} y {out / "B.tif"}. Selecciona una ventana sin NoData.')


if __name__ == '__main__':
    try:
        main()
    except (ValueError, OSError, subprocess.CalledProcessError, RuntimeError) as error:
        sys.exit(f'Error: {error}')
