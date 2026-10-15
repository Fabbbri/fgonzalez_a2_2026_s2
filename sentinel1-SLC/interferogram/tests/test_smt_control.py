"""Prueba el control Make de SMT con sysfs y sudo simulados, sin cambiar el PC."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest


class SmtControlTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="sentinel-smt-control-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.control = self.root / "control"
        self.active = self.root / "active"
        self.control.write_text("on\n")
        self.active.write_text("1\n")
        source = (Path(__file__).parents[2] / "Makefile").read_text()
        source = source.replace("/sys/devices/system/cpu/smt/control", str(self.control))
        source = source.replace("/sys/devices/system/cpu/smt/active", str(self.active))
        (self.root / "Makefile").write_text(source)
        # Solo acepta escribir el archivo temporal esperado; nunca invoca sudo real.
        sudo = self.root / "sudo"
        sudo.write_text('''#!/usr/bin/env python3
import os, pathlib, sys
root = pathlib.Path(os.environ["SMT_TEST_ROOT"])
assert sys.argv[1:] == ["tee", str(root / "control")]
state = sys.stdin.read().strip()
(root / "called").touch()
(root / "control").write_text(state + "\\n")
if not os.environ.get("SMT_TEST_MISMATCH"):
    (root / "active").write_text(("1" if state == "on" else "0") + "\\n")
''')
        sudo.chmod(0o755)
        self.env = {**os.environ, "PATH": str(self.root) + os.pathsep + os.environ["PATH"],
                    "SMT_TEST_ROOT": str(self.root)}

    def run_make(self, state, **env):
        return subprocess.run(["make", "--no-print-directory", "smt", "SMT=" + state],
                              cwd=self.root, env={**self.env, **env},
                              capture_output=True, text=True, timeout=10)

    def test_keep_and_already_on_do_not_write(self):
        for state in ("keep", "on"):
            self.assertEqual(self.run_make(state).returncode, 0)
        self.assertFalse((self.root / "called").exists())

    def test_off_and_on_verify_state(self):
        for state, active in (("off", "0"), ("on", "1")):
            result = self.run_make(state)
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(self.control.read_text().strip(), state)
            self.assertEqual(self.active.read_text().strip(), active)

    def test_invalid_unsupported_and_missing_control(self):
        self.assertNotEqual(self.run_make("invalid").returncode, 0)
        self.control.write_text("forceoff\n")
        self.assertNotEqual(self.run_make("on").returncode, 0)
        self.assertFalse((self.root / "called").exists())
        self.control.unlink()
        self.assertNotEqual(self.run_make("on").returncode, 0)

    def test_state_mismatch_fails(self):
        result = self.run_make("off", SMT_TEST_MISMATCH="1")
        self.assertNotEqual(result.returncode, 0)
        self.assertIn("No se alcanzó", result.stderr)


if __name__ == "__main__":
    unittest.main()
