#!/usr/bin/env python3
"""Regenera gráficas de una captura perf existente, sin volver a ejecutar perf."""
import argparse
import csv
import json
import math
from pathlib import Path
from profile_perf import report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", help="Carpeta que contiene contadores.csv y metadata.json")
    args = parser.parse_args()
    try:
        if not args.capture:
            raise ValueError("Indica PROFILE_CAPTURE=results/perf/nombre-de-campaña")
        capture = Path(args.capture)
        metadata = json.loads((capture / "metadata.json").read_text())
        with (capture / "contadores.csv").open(newline="") as stream:
            rows = list(csv.DictReader(stream))
        if not rows:
            raise ValueError("La captura no contiene iteraciones")
        for row in rows:
            if row["estado"] == "ok":
                row["valor"] = float(row["valor"])
                if not math.isfinite(row["valor"]) or row["valor"] < 0:
                    raise ValueError("Contador numérico inválido")
        report(rows, capture, metadata["alcance"], metadata)
        print(f"Gráficas regeneradas: {capture}")
    except (OSError, ValueError, KeyError, ImportError) as error:
        parser.exit(1, f"Error: {error}\n")


if __name__ == "__main__":
    main()
