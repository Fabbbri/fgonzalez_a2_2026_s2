#!/usr/bin/env python3
# Lectura y validación de una captura ya almacenada.
# No ejecuta perf ni el renderizador: reconstruye presentación y resumen
# a partir de las observaciones originales y sus metadatos.
"""Regenera gráficas de una captura perf existente, sin volver a ejecutar perf."""
import argparse
import csv
import json
import math
from pathlib import Path
from profile_perf import report


# Carga filas CSV, valida conteos numéricos y delega presentación al módulo común.
def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="Carpeta que contiene contadores.csv y metadata.json")
    args = parser.parse_args()
    try:
        if not args.capture:
            raise ValueError("Indica PROFILE_CAPTURE=results/perf/nombre-de-campaña")
        # Directorio que agrupa procedencia JSON y contadores por repetición.
        capture = Path(args.capture)
        # Metadatos de alcance y modo de conteo necesarios para interpretar la captura.
        metadata = json.loads((capture / "metadata.json").read_text())
        # DictReader conserva nombres de columna y with cierra el archivo al salir.
        with (capture / "contadores.csv").open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        if not rows:
            raise ValueError("La captura no contiene iteraciones")
        # Solo estados ok admiten conversión numérica; estados sin conteo se conservan.
        for row in rows:
            if row["estado"] == "ok":
                row["valor"] = float(row["valor"])
                if not math.isfinite(row["valor"]) or row["valor"] < 0:
                    raise ValueError("Contador numérico inválido")
        # Genera reporte y figuras sin modificar los valores fuente.
        report(rows, capture, metadata["alcance"], metadata)
        print(f"Gráficas regeneradas: {capture}")
    # Convierte errores de archivo, esquema o dependencias en salida CLI no exitosa.
    except (OSError, ValueError, KeyError, ImportError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
