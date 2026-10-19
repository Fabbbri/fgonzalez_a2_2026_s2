import csv
import importlib.util
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("profile", Path(__file__).parents[1] / "perf_profile.py")
profile = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(profile)


class ProfileTests(unittest.TestCase):
    def test_hybrid_pmus_cpu_and_missing_counters(self):
        rows = profile.parse_stat(
            "# comment\nCPU3;120;;cpu_core/cycles/;1000000;75.00;;\n"
            "CPU3;<not counted>;;cpu_atom/instructions/;0;0.00;;\n"
            "12.5;msec;task-clock;12500000;100.00;;\n"
            "<not supported>;;stalled-cycles-backend;0;0;;\n")
        self.assertEqual(len(rows), 4)
        self.assertEqual(rows[0]["cpu"], "CPU3")
        self.assertEqual(rows[0]["evento"], "cpu_core/cycles/")
        self.assertEqual(rows[0]["porcentaje_activo"], "75.00")
        self.assertEqual(rows[1]["valor"], "")
        self.assertEqual(rows[1]["estado"], "<not counted>")
        self.assertEqual(rows[2]["valor"], 12.5)

    def test_permission_failure_stops_preflight(self):
        with tempfile.TemporaryDirectory() as tmp, patch.object(profile.shutil, "which", return_value="perf"), \
                patch.object(profile.subprocess, "run", return_value=subprocess.CompletedProcess([], 255, "", "No permission to enable task-clock event")), \
                patch.dict(os.environ, {"EVENTS": "task-clock"}):
            with self.assertRaisesRegex(ValueError, "no tiene permiso"):
                profile.preflight("perf", "process", {}, Path(tmp))

    def test_unsupported_events_are_explicit(self):
        def fake_run(cmd, **kwargs):
            event = cmd[cmd.index("-e") + 1]
            Path(cmd[cmd.index("-o") + 1]).write_text(
                f"{'<not supported>' if event == 'cycles' else '1'};;{event};100;100;;\n")
            return subprocess.CompletedProcess(cmd, 0, "", "")
        with tempfile.TemporaryDirectory() as tmp, patch.object(profile.shutil, "which", return_value="perf"), \
                patch.object(profile.subprocess, "run", side_effect=fake_run), \
                patch.dict(os.environ, {"EVENTS": "cycles,context-switches"}):
            self.assertEqual(profile.preflight("perf", "process", {}, Path(tmp)),
                             (["context-switches"], ["cycles"]))

    def test_campaign_captures_five_models_and_generates_report(self):
        topo = {"cpus": [0, 1], "cores": [[0], [1]], "smt_active": "0"}
        calls = []

        def fake_run(cmd, **kwargs):
            calls.append(cmd)
            Path(cmd[cmd.index("-o") + 1]).write_text("10;msec;task-clock;10000000;100.00;;\n")
            return subprocess.CompletedProcess(cmd, 0)

        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            output = root / "perf"
            folder = output / "raw" / "nbody-test"
            folder.mkdir(parents=True)
            with patch.object(profile, "ROOT", root), patch.object(profile, "OUTPUT", output), \
                    patch.object(profile, "topology", return_value=topo), \
                    patch.object(profile, "command", return_value=(["/bin/true"], root, {})), \
                    patch.object(profile.subprocess, "run", side_effect=fake_run), \
                    patch.dict(os.environ, {"MODELS": "sequential fine coarse smt cmp", "REPETITIONS": "2",
                                            "THREADS": "2", "SMT_THREADS": "2"}):
                profile.capture("nbody", "perf", "process", topo, folder, ["task-clock"], ["cycles"])
                (folder / "status.txt").write_text("complete\n")
                profile.report()
            with (output / "nbody.csv").open() as stream:
                rows = list(csv.DictReader(stream))
            self.assertEqual(len(calls), 10)
            self.assertEqual(len(rows), 20)
            self.assertEqual({r["modelo"] for r in rows}, set(profile.MODELS))
            self.assertEqual(sum(r["estado"] == "no_soportado" for r in rows), 10)
            self.assertTrue(all(r["valor"] == "" for r in rows if r["estado"] != "ok"))
            self.assertIn("**complete**", (output / "reporte.md").read_text())
            self.assertEqual(len(list((output / "plots").glob("*.svg"))), 2)

    def test_cpu_scope_is_explicit_system_wide(self):
        self.assertEqual(profile.flags("process", {"cpus": [2, 7]}), [])
        self.assertEqual(profile.flags("cpu", {"cpus": [2, 7]}), ["-a", "-A", "-C", "2,7"])

    def test_commands_match_each_project_cli(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            for name in ("n-body-sim/build/nbody", "ray tracing/build/headless/raytracer",
                         "sentinel1-SLC/build/interferogram", "DL/src/main", "DL/src/fino",
                         "DL/src/grueso", "DL/src/smt.bin", "DL/src/cmp",
                         "DL/data/mnist/train-images-idx3-ubyte", "DL/data/mnist/train-labels-idx1-ubyte",
                         "A.tif", "B.tif"):
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.touch()
            with patch.object(profile, "ROOT", root), \
                    patch.object(profile, "topology", return_value={"cores": [[2, 6], [3, 7]]}), \
                    patch.dict(os.environ, {"SLC_A": "A.tif", "SLC_B": "B.tif", "DL_EPOCHS": "9"}):
                for problem in ("nbody", "ray", "dl", "sentinel"):
                    for model in profile.MODELS:
                        cmd, cwd, env = profile.command(problem, model, 2, root)
                        self.assertTrue(Path(cmd[0]).is_file())
                        self.assertTrue(cwd.is_dir())
                        if problem == "dl":
                            expected = {"sequential": ["9"], "fine": ["9"],
                                        "coarse": ["9", "2", os.environ.get("BLOCK_INTERVAL_MS", "10"), os.environ.get("BLOCK_DURATION_MS", "1")],
                                        "smt": ["2", "2,3", "9"], "cmp": ["2", "9"]}
                            self.assertEqual(cmd[1:], expected[model])
                            self.assertEqual(env["DL_QUIET"], "1")
                        else:
                            flag = "--mode" if problem == "sentinel" else "--model"
                            expected = model if problem == "ray" else {"fine": "fine-grained", "coarse": "coarse-grained"}.get(model, model)
                            self.assertEqual(cmd[cmd.index(flag) + 1], expected)
                            self.assertEqual("--threads" in cmd, model in ("coarse", "smt", "cmp"))


if __name__ == "__main__":
    unittest.main()
