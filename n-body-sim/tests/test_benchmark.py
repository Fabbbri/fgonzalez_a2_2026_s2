# Ensayos de coordinación con ejecutable y topología simulados.
# Los tiempos y bytes de trayectoria de fixture tienen valores conocidos.
# Se verifican estadísticas, referencias inmutables, fases SMT y restauración
# sin ejecutar cambios de hardware ni sudo real.
"""Valida campañas, referencias, métricas y rechazos sin mediciones de rendimiento."""
import argparse
import csv
import contextlib
import io
import json
import os
from pathlib import Path
import statistics
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).parents[1] / "scripts"))
import benchmark
import plot_metrics


# Propiedades de la campaña y sus efectos en archivos y configuración.
class BenchmarkTests(unittest.TestCase):
    # Crea ejecutable temporal que emite métricas controladas y simula on/off mediante un archivo.
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="nbody-benchmark-test-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.binary = self.root / "nbody"
        self.binary.write_text('''#!/usr/bin/env python3
import csv, json, os, pathlib, sys
if sys.argv[1:] == ["--topology"]:
    state_file = pathlib.Path(__file__).with_suffix(".state")
    state = state_file.read_text() if state_file.exists() else "on"
    if state == "off":
        print("physical_cores=2,logical_cpus=2,allowed_cpus=0;2,core_cpus=0;2,sibling_groups=0|2,smt_control=off,smt_active=0")
    else:
        print("physical_cores=2,logical_cpus=4,allowed_cpus=0;1;2;3,core_cpus=0;2,sibling_groups=0;1|2;3,smt_control=on,smt_active=1")
    sys.exit(0)
args = sys.argv[1:]
def get(key, default=None):
    return args[args.index(key)+1] if key in args else default
model = get("--model")
state_file = pathlib.Path(__file__).with_suffix(".state")
state = state_file.read_text() if state_file.exists() else "on"
p = int(get("--threads", "1"))
payload = b"trajectory" + (b"invalid" if os.environ.get("BAD_MODEL") == model else b"")
if model == "smt" and os.environ.get("BAD_STATE") == state:
    payload += b"invalid state"
pathlib.Path(get("--trajectory")).write_bytes(payload)
counter = pathlib.Path(__file__).with_suffix(".json")
counts = json.loads(counter.read_text()) if counter.exists() else {}
counts[model] = counts.get(model, 0) + 1
counter.write_text(json.dumps(counts))
if model in ("smt", "cmp"):
    cpus = [0,2,1,3] if model == "smt" and state == "on" else [0,2]
    text = "CPU fijadas y verificadas: " if model == "smt" else "CPU fijadas por núcleo físico: "
    print(text + ";".join(str(cpus[i % len(cpus)]) for i in range(p)) + "; ", file=sys.stderr)
out = csv.writer(sys.stdout)
out.writerow(["model","workers","bodies","steps","dt","seed","quantum","total_seconds"])
out.writerow([model,p,int(get("--bodies-per-galaxy"))*2,get("--steps"),get("--dt"),get("--seed"),get("--quantum","0"),(.01 if model == "sequential" else .004)+counts[model]*.001])
''')
        self.binary.chmod(0o755)
        self.args = argparse.Namespace(binary=str(self.binary), repetitions=2,
            results_dir=self.root / "resultnbody", models=["sequential", "fine", "coarse", "smt", "cmp"],
            model=None, thread_counts="1,2", cmp_thread_counts="1,2", smt_thread_counts="2,4", threads=None, baseline_csv=None,
            bodies_per_galaxy=5, steps=3, dt=0.002, seed=42, quantum=1,
            block_interval_ms=10, block_duration_ms=1)
        self.plots = patch.object(benchmark, "generate")
        self.plots.start()
        self.addCleanup(self.plots.stop)
        quiet = contextlib.redirect_stdout(io.StringIO())
        quiet.__enter__()
        self.addCleanup(quiet.__exit__, None, None, None)

    # Lee un CSV generado en la carpeta de ensayo como registros por nombre de columna.
    def rows(self, name):
        with (self.args.results_dir / name).open(newline="") as stream:
            return list(csv.DictReader(stream))

    # Valida cantidad de repeticiones, calentamientos, S/E y estructura de resultados.
    def test_campaign_metrics_warmups_and_layout(self):
        benchmark.campaign(self.args)
        expected = {"sequential": 2, "fine": 2, "coarse": 4, "smt-on": 4, "cmp": 4}
        for label, count in expected.items():
            rows = self.rows(label + ".csv")
            self.assertEqual(len(rows), count)
            self.assertEqual(set(rows[0]), set(benchmark.FIELDS))
            for row in rows:
                ratio = float(row["tiempo_secuencial_ms"]) / float(row["tiempo_total_ms"])
                self.assertAlmostEqual(float(row["speedup"]), ratio)
                self.assertAlmostEqual(float(row["eficiencia_paralela"]), ratio / int(row["hilos"]))
            self.assertTrue((self.args.results_dir / (label + "-summary.csv")).exists())
        base = statistics.mean(float(r["tiempo_total_ms"]) for r in self.rows("sequential.csv"))
        rows = self.rows("coarse.csv")[:2]
        summary = self.rows("coarse-summary.csv")[0]
        self.assertAlmostEqual(float(summary["speedup_media"]), base / statistics.mean(float(r["tiempo_total_ms"]) for r in rows))
        self.assertEqual(json.loads(self.binary.with_suffix(".json").read_text()),
                         {"sequential": 3, "fine-grained": 3, "coarse-grained": 6, "smt": 6, "cmp": 6})
        self.assertEqual(self.rows("smt-on.csv")[-1]["afinidad_cpus"], "0;2;1;3")
        groups, workload = plot_metrics.load([str(self.args.results_dir / "*.csv")])
        self.assertEqual(len(groups), 8)
        self.assertEqual(workload, ("10", "3", "0.002", "42"))
        self.assertEqual(next(self.args.results_dir.glob("logs/*/status.txt")).read_text(), "complete\n")

    # Una campaña posterior no puede alterar la referencia que importan CSV anteriores.
    def test_immutable_reference_survives_later_campaign(self):
        benchmark.campaign(self.args)
        row = self.rows("coarse.csv")[0]
        reference = self.args.results_dir / row["referencia_csv"]
        content = reference.read_bytes()
        self.args.models = ["smt"]
        self.args.baseline_csv = reference
        benchmark.campaign(self.args)
        self.assertEqual(reference.read_bytes(), content)
        self.assertEqual(json.loads(self.binary.with_suffix(".json").read_text())["sequential"], 3)
        plot_metrics.load([str(self.args.results_dir / "*.csv")])

    # Un hash de trayectoria diferente al secuencial impide exportar mediciones válidas.
    def test_mismatching_trajectory_is_rejected(self):
        with patch.dict(os.environ, BAD_MODEL="coarse-grained"):
            with self.assertRaisesRegex(ValueError, "trayectoria difiere"):
                benchmark.campaign(self.args)
        self.assertEqual(self.rows("coarse.csv"), [])
        self.assertEqual(next(self.args.results_dir.glob("logs/*/status.txt")).read_text(), "incomplete\n")

    # El analizador detecta cocientes inconsistentes y cargas incompatibles.
    def test_plot_rejects_bad_speedup_and_mixed_workloads(self):
        benchmark.campaign(self.args)
        path = self.args.results_dir / "coarse.csv"
        rows = self.rows("coarse.csv")
        rows[0]["speedup"] = "999"
        with path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=benchmark.FIELDS)
            writer.writeheader(); writer.writerows(rows)
        with self.assertRaisesRegex(ValueError, "Speedup"):
            plot_metrics.load([str(path)])
        rows[0]["speedup"] = str(float(rows[0]["tiempo_secuencial_ms"]) / float(rows[0]["tiempo_total_ms"]))
        rows[1]["dt"] = "0.5"
        with path.open("w", newline="") as stream:
            writer = csv.DictWriter(stream, fieldnames=benchmark.FIELDS)
            writer.writeheader(); writer.writerows(rows)
        with self.assertRaisesRegex(ValueError, "cargas diferentes"):
            plot_metrics.load([str(path)])

    # Verifica barridos automáticos por C,L y rechazo de cantidades no positivas o mal formadas.
    def test_counts(self):
        self.assertEqual(benchmark.counts("auto", 8, 12), [1, 2, 4, 8])
        self.assertEqual(benchmark.counts("auto", 8, 12, True), [8, 12])
        self.assertEqual(benchmark.counts("auto", 8, 8, True), [8, 16])
        for value in ("0", "1,", "-1", "x"):
            with self.assertRaises(ValueError):
                benchmark.counts(value, 8, 12)

    # Consulta --defaults sin sustituir un valor explícito del usuario.
    def test_defaults_come_from_binary_and_preserve_overrides(self):
        values = dict(bodies_per_galaxy=123, steps=77, dt=0.125, seed=987,
                      quantum=9, block_interval_ms=21, block_duration_ms=3)
        for key in values:
            setattr(self.args, key, None)
        self.args.steps = 4  # Un argumento explícito debe tener precedencia.
        with patch.object(benchmark.subprocess, "check_output", return_value=json.dumps(values)) as query:
            benchmark.apply_config_defaults(self.args)
        self.assertEqual(query.call_args.args[0], [str(self.binary), "--defaults"])
        for key, value in values.items():
            self.assertEqual(getattr(self.args, key), 4 if key == "steps" else value)

    # Fixture de fases SMT con estado inicial configurable y fallo opcional de trayectoria.
    def automatic_campaign(self, original="on", failure=None, states="on,off"):
        state_file = self.binary.with_suffix(".state")
        state_file.write_text(original)
        calls = []
        # Sustituye el control de hardware por escritura de un archivo temporal y registro de llamadas.
        def change(state, make_command):
            calls.append(state)
            state_file.write_text(state)
        self.args.smt_states = states
        self.args.make_command = "make"
        self.args.smt_thread_counts = "auto"
        # Ningún sudo ni cambio de hardware real: solo se modifica el archivo temporal.
        with patch.object(benchmark, "read_smt_state", return_value=original), \
             patch.object(benchmark, "set_smt_state", side_effect=change), \
             patch.object(benchmark, "authorize_smt_control"):
            if failure:
                with patch.dict(os.environ, BAD_STATE="off"):
                    with self.assertRaisesRegex(ValueError, "trayectoria difiere"):
                        benchmark.run_campaigns(self.args)
            else:
                benchmark.run_campaigns(self.args)
        self.assertEqual(state_file.read_text(), original)
        self.assertEqual(calls[-1], original)
        return calls

    # Medir solo off conserva los archivos secuenciales anteriores y usa base con etiqueta propia.
    def test_off_only_preserves_existing_sequential(self):
        benchmark.campaign(self.args)
        path = self.args.results_dir / "sequential.csv"
        summary = self.args.results_dir / "sequential-summary.csv"
        before = path.read_bytes(), summary.read_bytes()
        self.args.models = ["smt"]
        self.automatic_campaign(states="off")
        self.assertEqual((path.read_bytes(), summary.read_bytes()), before)
        self.assertEqual({r["smt_activo_linux"] for r in self.rows("sequential-off.csv")}, {"0"})

    # Ambos estados usan los mismos P, bases de su estado y grupos de presentación separados.
    def test_automatic_on_off_same_workers_and_references(self):
        self.automatic_campaign(original="off")
        on, off = self.rows("smt-on.csv"), self.rows("smt-off.csv")
        self.assertEqual({r["hilos"] for r in on}, {"2", "4"})
        self.assertEqual({r["hilos"] for r in off}, {"2", "4"})
        self.assertEqual({r["smt_activo_linux"] for r in on}, {"1"})
        self.assertEqual({r["smt_activo_linux"] for r in off}, {"0"})
        self.assertEqual({r["referencia_smt"] for r in off}, {"0"})
        self.assertEqual({r["referencia_smt"] for r in on}, {"1"})
        self.assertEqual(off[-1]["afinidad_cpus"], "0;2;0;2")
        groups, _ = plot_metrics.load([str(self.args.results_dir / "*.csv")])
        self.assertTrue(any(k[0][0] == "smt" and "SMT=0" in k[0][4] for k in groups))
        self.assertEqual(len([k for k in groups if k[0][0] == "sequential"]), 2)
        manifest = json.loads((self.args.results_dir / "benchmark.txt").read_text())
        self.assertEqual(manifest["restored_smt"], "off")
        self.assertEqual(manifest["status"], "complete")

    # El fallo de una fase restaura estado inicial y conserva el lote como incompleto.
    def test_automatic_restores_on_failure(self):
        self.automatic_campaign(failure=True)
        self.assertEqual(self.rows("smt-off.csv"), [])
        self.assertNotEqual(json.loads((self.args.results_dir / "benchmark.txt").read_text())["status"], "complete")

    # Campañas sin SMT o keep no invocan control global de hardware.
    def test_no_hardware_control_without_smt_or_when_keep(self):
        with patch.object(benchmark, "campaign") as campaign, \
             patch.object(benchmark, "set_smt_state") as change:
            self.args.smt_states = "keep"
            benchmark.run_campaigns(self.args)
            self.args.smt_states = "on,off"
            self.args.models = ["cmp"]
            benchmark.run_campaigns(self.args)
        self.assertEqual(campaign.call_count, 2)
        change.assert_not_called()

    # KeyboardInterrupt recorre finally y restaura SMT antes de propagarse.
    def test_restores_after_interruption(self):
        self.args.smt_states = "on,off"
        self.args.make_command = "make"
        with patch.object(benchmark, "read_smt_state", return_value="off"), \
             patch.object(benchmark, "set_smt_state") as change, \
             patch.object(benchmark, "authorize_smt_control"), \
             patch.object(benchmark, "campaign", side_effect=KeyboardInterrupt):
            with self.assertRaises(KeyboardInterrupt):
                benchmark.run_campaigns(self.args)
        self.assertEqual(change.call_args.args, ("off", "make"))

    # Un fallo de restauración conserva diagnóstico con el comando de recuperación.
    def test_failed_restoration_reports_recovery_command(self):
        self.args.smt_states = "on,off"
        self.args.make_command = "make"
        with patch.object(benchmark, "read_smt_state", return_value="on"), \
             patch.object(benchmark, "set_smt_state", side_effect=[None, None, None, OSError("denied")]), \
             patch.object(benchmark, "authorize_smt_control"), \
             patch.object(benchmark, "campaign", return_value={}):
            with self.assertRaisesRegex(RuntimeError, "make smt-control SMT=on"):
                benchmark.run_campaigns(self.args)


if __name__ == "__main__":
    unittest.main()
