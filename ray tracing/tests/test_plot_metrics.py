# Ensayos de carga CSV y estadística mediante datos sintéticos con solución conocida.
# Los archivos temporales se eliminan al salir de cada contexto. No ejecuta el
# renderizador ni modifica las mediciones almacenadas de las campañas.
"""Comprueba que las distribuciones usan datos crudos compatibles, sin filtrarlos."""
import csv
import importlib.util
from pathlib import Path
import tempfile
import unittest

# Carga el módulo desde su ruta, sin exigir instalación como paquete.
spec = importlib.util.spec_from_file_location(
    "plot_metrics", Path(__file__).parents[1] / "scripts/plot_metrics.py")
# Crea y ejecuta el módulo de análisis en un espacio propio para probar sus funciones.
plots = importlib.util.module_from_spec(spec)
spec.loader.exec_module(plots)


# Agrupa propiedades de ajuste, referencia y compatibilidad de observaciones.
class PlotDataTests(unittest.TestCase):
    # Genera S(P) con f=0.2 y comprueba recuperación exacta sin error residual.
    def test_amdahl_recovers_known_fraction(self):
        f = .2
        points = [(p, 1/(f+(1-f)/p)) for p in (1,2,4,8)]
        fitted, raw, error = plots.fit_amdahl(points)
        self.assertAlmostEqual(fitted,f)
        self.assertAlmostEqual(raw,f)
        self.assertAlmostEqual(error,0)
        self.assertIsNone(plots.fit_amdahl([(1,1)]))

    # Los datos superlineales o más lentos fuerzan el límite [0,1] del ajuste.
    def test_amdahl_does_not_invent_negative_serial_fraction(self):
        fitted, raw, error = plots.fit_amdahl([(2,4),(4,8)])
        self.assertEqual(fitted,0)
        self.assertLess(raw,0)
        self.assertGreater(error,0)
        fitted,raw,_ = plots.fit_amdahl([(2,.5),(4,.5)])
        self.assertEqual(fitted,1)
        self.assertGreater(raw,1)

    # Contrasta la razón de medias y margen delta con un cálculo independiente.
    def test_shared_ratio_of_means_and_uncertainty(self):
        rows = plots.Measurements([(50,100,2,1), (25,100,4,2)])
        rows.baseline = [80,120]
        rows.workers = 2
        mean, _, margin = plots.metric_estimate(rows, 2)
        self.assertAlmostEqual(mean, 100/37.5)
        expected = 1.96 * (100/37.5) * ((800/100**2)/2 + (312.5/37.5**2)/2)**.5
        self.assertAlmostEqual(margin, expected)
        self.assertAlmostEqual(plots.metric_estimate(rows, 3, 100)[0], mean/2*100)
        self.assertAlmostEqual(plots.metric_estimate(rows, 1)[1], 800**.5)

    # Comprueba que la referencia conserva tiempos originales y no repite una media artificial.
    def test_load_shared_reference_keeps_original_distribution(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.write(root/'base.csv', times=(80,120), model='sequential')
            self.write(root/'fine.csv', model='fine')
            for name, shared in (('base.csv',False), ('fine.csv',True)):
                path = root/name
                with path.open() as stream:
                    rows = list(csv.DictReader(stream))
                for row in rows:
                    row.update(imagen_hash='abc', referencia_smt='1',
                               referencia_tipo='compartida' if shared else 'propia',
                               referencia_csv='base.csv' if shared else '')
                    if not shared: row['hilos']='1'
                with path.open('w', newline='') as stream:
                    writer = csv.DictWriter(stream,fieldnames=list(rows[0]))
                    writer.writeheader();writer.writerows(rows)
            groups,_ = plots.load([str(root/'fine.csv')])
            data = next(iter(groups.values()))
            self.assertEqual(data.baseline,[80,120])
            self.assertAlmostEqual(plots.metric_estimate(data,2)[0],100/37.5)
            self.assertGreater(plots.metric_estimate(data,1)[2],0)

    # Fixture CSV: escribe observaciones pareadas y permite alterar carga o cocientes para casos inválidos.
    def write(self, path, times=(50, 25), width=640, speedup_override=None, model="coarse", smt="1"):
        fields = list(plots.WORKLOAD) + list(plots.POLICY) + [
            "hilos", "repeticion", "tiempo_total_ms", "tiempo_secuencial_ms",
            "speedup", "eficiencia_paralela", "nucleos_fisicos_disponibles",
            "cpus_logicas_disponibles", "smt_activo_linux"]
        with path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=fields)
            writer.writeheader()
            for repetition, time in enumerate(times, 1):
                writer.writerow(dict(ancho=width, alto=480, tile_ancho=32, tile_alto=32,
                                     muestras=8, rebotes=12, semilla=2026, modelo=model,
                                     K=64, intervalo_ms=10, espera_ms=1, hilos=2,
                                     repeticion=repetition, tiempo_total_ms=time,
                                     tiempo_secuencial_ms=100,
                                     speedup=100/time if speedup_override is None else speedup_override,
                                     eficiencia_paralela=100/time/2, nucleos_fisicos_disponibles=2,
                                     cpus_logicas_disponibles=4 if smt == "1" else 2,
                                     smt_activo_linux=smt))

    # Verifica inclusión de todas las muestras y media de speedups pareados.
    def test_all_iterations_and_paired_statistics(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "raw.csv"
            self.write(path, times=(50, 25, 2000))
            (Path(directory) / "raw-summary.csv").write_text("modelo,hilos\ncoarse,2\n")
            groups, _ = plots.load([str(Path(directory) / "*.csv")])
            rows = next(iter(groups.values()))
            self.assertEqual(len(rows), 3)  # Incluye el valor atípico.
            self.assertAlmostEqual(plots.estimate([row[2] for row in rows])[0], 2.0166666666667)
            self.assertAlmostEqual(plots.estimate([50, 25])[2], 24.5)

    # Cambiar resolución entre archivos debe impedir su comparación conjunta.
    def test_different_workloads_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(Path(directory) / "one.csv")
            self.write(Path(directory) / "two.csv", width=641)
            with self.assertRaisesRegex(ValueError, "cargas diferentes"):
                plots.load([str(Path(directory) / "*.csv")])

    # Speedup incompatible con los tiempos provoca un error de carga.
    def test_invalid_ratios_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "raw.csv"
            self.write(path, speedup_override=999)
            with self.assertRaisesRegex(ValueError, "Speedup incompatible"):
                plots.load([str(path)])

    # Estados SMT distintos producen grupos separados aun con el mismo modelo y P.
    def test_smt_hardware_states_not_merged(self):
        with tempfile.TemporaryDirectory() as directory:
            self.write(Path(directory) / "on.csv", model="smt", smt="1")
            self.write(Path(directory) / "off.csv", model="smt", smt="0")
            groups, _ = plots.load([str(Path(directory) / "*.csv")])
            self.assertEqual(len(groups), 2)
            self.assertTrue(all(len(rows) == 2 for rows in groups.values()))
            self.assertTrue(any("SMT=0" in plots.label(key) for key in groups))


# unittest descubre métodos test_* y devuelve fallo si una propiedad no se cumple.
if __name__ == "__main__":
    unittest.main()
