#!/usr/bin/env python3
# Coordinación entre medición C++ y análisis posterior en Python.
# El proceso del benchmark termina antes de iniciar Matplotlib: las gráficas
# no forman parte del intervalo cronometrado de los modelos.
"""Ejecuta el benchmark y después grafica el CSV que realmente se escribió."""
import argparse
from pathlib import Path
import subprocess
import sys


# Interpreta rutas, ejecuta una campaña y propaga el código de salida de cada fase.
def main():
    # El analizador separa opciones del coordinador de argumentos del ejecutable.
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--csv", default="results/metrics.csv")
    parser.add_argument("--plot-dir", type=Path, default=Path("results/plots"))
    parser.add_argument("binary")
    # REMAINDER conserva literalmente las opciones restantes para el renderizador.
    parser.add_argument("args", nargs=argparse.REMAINDER)
    options = parser.parse_args()
    # Comprobar la dependencia antes de iniciar una campaña larga.
    try:
        import matplotlib  # noqa: F401
    except ImportError:
        parser.exit(1, "Falta Matplotlib. Instala: sudo apt install python3-matplotlib\n")
    # Ruta efectiva: puede ser sustituida por --metrics-output dentro de args.
    csv_path = options.csv
    # Busca la última ruta explícita con la misma precedencia que el programa C++.
    for index, argument in enumerate(options.args):
        if argument == "--metrics-output" and index + 1 < len(options.args):
            csv_path = options.args[index + 1]
    # Lista de argumentos sin shell intermedio: conserva espacios en las rutas.
    command = [options.binary, "--benchmark", "--metrics-output", options.csv, *options.args]
    # Ejecución bloqueante; no hay campañas paralelas compitiendo por CPU.
    measured = subprocess.run(command)
    # Un benchmark fallido impide graficar resultados potencialmente parciales.
    if measured.returncode:
        return measured.returncode
    # Una carpeta por archivo evita que coarse sobrescriba las gráficas de fine.
    # El nombre base del CSV identifica su carpeta independiente de gráficas.
    destination = options.plot_dir / Path(csv_path).stem
    # Usa el mismo intérprete Python para cargar únicamente el CSV recién medido.
    plotted = subprocess.run([sys.executable, str(Path(__file__).with_name("plot_metrics.py")),
                              csv_path, "--output-dir", str(destination)])
    return plotted.returncode


# Punto de entrada CLI; importar el módulo no ejecuta una campaña.
if __name__ == "__main__":
    sys.exit(main())
