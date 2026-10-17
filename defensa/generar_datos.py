#!/usr/bin/env python3
"""Genera las tablas de la vista y, opcionalmente, copia los resultados del repositorio."""
import argparse
import json
from pathlib import Path
import shutil

ROOT = Path(__file__).resolve().parent
SOURCES = {
    "dl": "DL/src/resultdl/plots/comparison",
    "nbody": "n-body-sim/resultnbody/plots/comparison",
    "ray": "ray tracing/resultsrt/plots/comparison",
    "sentinel": "sentinel1-SLC/resultsen/plots/comparison",
}
CHARTS = ("comparacion-eficiencia.png", "comparacion-speedup.png",
          "comparacion-tiempo.png", "escalabilidad.png")
FILES = (*CHARTS, "reporte.md", "amdahl.md", "amdahl.png")


def read_report(folder, relative):
    report = folder / "reporte.md"
    lines = report.read_text(encoding="utf-8").splitlines()
    table = [line.strip() for line in lines if line.lstrip().startswith("|")]
    if len(table) < 3:
        raise ValueError(f"No se encontró la tabla de resultados: {report}")
    cells = lambda line: [cell.strip() for cell in line.strip("|").split("|")]
    headers = cells(table[0])
    rows = [cells(line) for line in table[2:]]
    if len(headers) != 7 or any(len(row) != 7 for row in rows):
        raise ValueError(f"Se esperaban siete columnas: {report}")
    for chart in CHARTS:
        if not (folder / chart).is_file():
            raise FileNotFoundError(folder / chart)
    return {"folder": relative, "headers": headers, "rows": rows,
            "workload": next((line.removeprefix("Carga fija: ").rstrip(".")
                              for line in lines if line.startswith("Carga fija: ")), "")}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--actualizar", action="store_true",
                        help="Copiar los reportes y las gráficas actuales de los cuatro proyectos.")
    args = parser.parse_args()
    if args.actualizar:
        # Validar todas las fuentes antes de reemplazar las copias de la vista.
        for key, path in SOURCES.items():
            folder = ROOT.parent / path
            read_report(folder, f"resultados/{key}")
            for filename in FILES:
                if not (folder / filename).is_file():
                    parser.error(f"No se encontró {folder / filename}")
        for key, path in SOURCES.items():
            destination = ROOT / "resultados" / key
            destination.mkdir(parents=True, exist_ok=True)
            for filename in FILES:
                shutil.copy2(ROOT.parent / path / filename, destination / filename)
    data = {key: read_report(ROOT / "resultados" / key, f"resultados/{key}")
            for key in SOURCES}
    data["sentinel_equipo_b"] = read_report(
        ROOT / "resultados/sentinel_equipo_b", "resultados/sentinel_equipo_b")
    # Es un archivo JS local para que index.html también funcione por file://.
    payload = json.dumps(data, ensure_ascii=False, indent=2).replace("</", "<\\/")
    (ROOT / "data.js").write_text(
        "// Generado por generar_datos.py a partir de los reportes incluidos en defensa.\n"
        f"window.METRICAS = {payload};\n", encoding="utf-8")
    print("Actualizadas las tablas de los cuatro problemas; 20 PNG verificados en defensa.")


if __name__ == "__main__":
    main()
