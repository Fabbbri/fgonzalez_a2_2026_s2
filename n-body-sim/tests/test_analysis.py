# Ensayos de Student t, estadística descriptiva y compatibilidad de muestras.
# Se utilizan valores analíticos o tabulados y archivos temporales; no se
# ejecutan simulaciones de rendimiento.
"""Referencias independientes para la estadística y validación de muestras."""
import csv
import importlib.util
import math
from pathlib import Path
import tempfile
import unittest
import xml.etree.ElementTree as ET

spec = importlib.util.spec_from_file_location("analysis", Path(__file__).parents[1] / "scripts/analyze.py")
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


# Propiedades de cuantiles, intervalos, validación de carga y generación SVG.
class AnalysisTests(unittest.TestCase):
    # Contrasta cuantiles con Cauchy, fórmula de df=2 y valores tabulados NIST.
    def test_student_quantiles(self):
        # df=1 es Cauchy; df=2 tiene CDF 1/2+x/(2*sqrt(x²+2)).
        self.assertAlmostEqual(analysis.t_critical(1), math.tan(0.475 * math.pi), places=7)
        self.assertAlmostEqual(analysis.t_critical(2), math.sqrt(2 * 0.95**2 / (1 - 0.95**2)), places=7)
        # Tabla NIST, columna 0.975: eda/section3/eda3672.htm.
        for df, expected in [(9, 2.262), (30, 2.042), (100, 1.984)]:
            self.assertAlmostEqual(analysis.t_critical(df), expected, delta=0.0005)

    # La serie 1,2,3 valida media, desviación e IC; una serie constante da margen cero.
    def test_sample_deviation_and_interval(self):
        result = analysis.summarize([1, 2, 3])
        self.assertEqual(result["mean_seconds"], 2)
        self.assertEqual(result["sample_stddev_seconds"], 1)
        margin = math.sqrt(2 * 0.95**2 / (1 - 0.95**2)) / math.sqrt(3)
        self.assertAlmostEqual(result["ci95_low_seconds"], 2 - margin, places=7)
        self.assertAlmostEqual(result["ci95_high_seconds"], 2 + margin, places=7)
        constant = analysis.summarize([2] * 200)
        self.assertEqual(constant["ci95_low_seconds"], 2)
        self.assertEqual(constant["sample_stddev_seconds"], 0)

    # Rechaza muestras insuficientes, tiempos no positivos, NaN e infinito.
    def test_invalid_measurements(self):
        for values in ([1], [1, float("nan")], [1, float("inf")], [1, -1], [0, 1]):
            with self.assertRaises(ValueError):
                analysis.summarize(values)

    # Comprueba salida de resumen/histograma y rechazo de un lote con configuraciones mezcladas.
    def test_mixed_configurations_and_svg(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sample = root / "samples.csv"
            header = ["repetition", *analysis.CONFIG_FIELDS, "total_seconds"]
            rows = [[1, "sequential", 1, 4, 10, 0.002, 42, 0, 1],
                    [2, "sequential", 1, 8, 10, 0.002, 42, 0, 2]]
            with sample.open("w", newline="") as target:
                writer = csv.writer(target); writer.writerow(header); writer.writerows(rows)
            with self.assertRaises(ValueError):
                analysis.load_samples(sample)
            config = {"model": "sequential", "workers": "1"}
            analysis.histogram([2] * 200, config, analysis.summarize([2] * 200), root / "histogram.svg")
            chart = ET.parse(root / "histogram.svg").getroot()
            bars = chart.findall(".//{http://www.w3.org/2000/svg}rect/{http://www.w3.org/2000/svg}title")
            self.assertEqual(sum(int(bar.text.split()[0]) for bar in bars), 200)

    # K distingue configuraciones; solo sequential admite un esquema anterior sin quantum.
    def test_quantum_separates_samples_and_legacy_sequential(self):
        with tempfile.TemporaryDirectory() as temporary:
            sample = Path(temporary) / "samples.csv"
            header = ["repetition", *analysis.CONFIG_FIELDS, "total_seconds"]
            rows = [[1, "fine-grained", 1, 4, 10, 0.002, 42, 1, 1],
                    [2, "fine-grained", 1, 4, 10, 0.002, 42, 3, 2]]
            with sample.open("w", newline="") as target:
                writer = csv.writer(target); writer.writerow(header); writer.writerows(rows)
            with self.assertRaises(ValueError):
                analysis.load_samples(sample)
            with sample.open("w", newline="") as target:
                writer = csv.writer(target)
                writer.writerow([field for field in header if field != "quantum"])
                writer.writerows([[1, "sequential", 1, 4, 10, 0.002, 42, 1],
                                  [2, "sequential", 1, 4, 10, 0.002, 42, 2]])
            config, values = analysis.load_samples(sample)
            self.assertEqual(config["quantum"], "0")
            self.assertEqual(values, [1, 2])


if __name__ == "__main__":
    unittest.main()
