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


class AnalysisTests(unittest.TestCase):
    def test_student_quantiles(self):
        # df=1 es Cauchy; df=2 tiene CDF 1/2+x/(2*sqrt(x²+2)).
        self.assertAlmostEqual(analysis.t_critical(1), math.tan(0.475 * math.pi), places=7)
        self.assertAlmostEqual(analysis.t_critical(2), math.sqrt(2 * 0.95**2 / (1 - 0.95**2)), places=7)
        # Tabla NIST, columna 0.975: eda/section3/eda3672.htm.
        for df, expected in [(9, 2.262), (30, 2.042), (100, 1.984)]:
            self.assertAlmostEqual(analysis.t_critical(df), expected, delta=0.0005)

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

    def test_invalid_measurements(self):
        for values in ([1], [1, float("nan")], [1, float("inf")], [1, -1], [0, 1]):
            with self.assertRaises(ValueError):
                analysis.summarize(values)

    def test_mixed_configurations_and_svg(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            sample = root / "samples.csv"
            header = ["repetition", *analysis.CONFIG_FIELDS, "total_seconds"]
            rows = [[1, "sequential", 1, 4, 10, 0.002, 42, 1],
                    [2, "sequential", 1, 8, 10, 0.002, 42, 2]]
            with sample.open("w", newline="") as target:
                writer = csv.writer(target); writer.writerow(header); writer.writerows(rows)
            with self.assertRaises(ValueError):
                analysis.load_samples(sample)
            config = {"model": "sequential", "workers": "1"}
            analysis.histogram([2] * 200, config, analysis.summarize([2] * 200), root / "histogram.svg")
            chart = ET.parse(root / "histogram.svg").getroot()
            bars = chart.findall(".//{http://www.w3.org/2000/svg}rect/{http://www.w3.org/2000/svg}title")
            self.assertEqual(sum(int(bar.text.split()[0]) for bar in bars), 200)


if __name__ == "__main__":
    unittest.main()
