#!/usr/bin/env python3
"""Ejecuta el benchmark y después grafica el CSV que realmente se escribió."""
import argparse
from pathlib import Path
import subprocess
import sys


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", default="results/metrics.csv")
    parser.add_argument("--plot-dir", type=Path, default=Path("results/plots"))
    parser.add_argument("binary")
    parser.add_argument("args", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    # Comprobar la dependencia antes de iniciar una campaña larga.
    try:
        import matplotlib  # noqa: F401
    except ImportError:
        parser.exit(1, "Falta Matplotlib. Instala: sudo apt install python3-matplotlib\n")
    csv_path = options.csv
    for index, argument in enumerate(options.args):
        if argument == "--metrics-output" and index + 1 < len(options.args):
            csv_path = options.args[index + 1]
    command = [options.binary, "--benchmark", "--metrics-output", options.csv, *options.args]
    measured = subprocess.run(command)
    if measured.returncode:
        return measured.returncode
    # Una carpeta por archivo evita que coarse sobrescriba las gráficas de fine.
    destination = options.plot_dir / Path(csv_path).stem
    plotted = subprocess.run([sys.executable, str(Path(__file__).with_name("plot_metrics.py")),
                              csv_path, "--output-dir", str(destination)])
    return plotted.returncode


if __name__ == "__main__":
    sys.exit(main())
