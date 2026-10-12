"""Verifica fórmulas y evita comparaciones entre entradas o entornos distintos."""
import csv
import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).parents[1] / 'scripts'))
import report
from analyze import CONFIG_FIELDS


def lot(root, name, model='sequential', workers=1, bodies=1000, values=(2., 2.), topology='physical_cores=4,smt_active=0'):
    folder = root / name
    folder.mkdir()
    with (folder / 'samples.csv').open('w', newline='') as target:
        writer = csv.writer(target)
        writer.writerow(['repetition', *CONFIG_FIELDS, 'total_seconds'])
        writer.writerows([i, model, workers, bodies, 1000, .002, 42, 1 if model == 'fine-grained' else 0, value]
                         for i, value in enumerate(values, 1))
    (folder / 'summary.csv').write_text('completed\n')
    (folder / 'environment.txt').write_text('Linux test\nModel name: Test CPU\n')
    (folder / 'topology.txt').write_text(topology)
    return folder


class ReportTests(unittest.TestCase):
    def test_constant_ratio_and_efficiency(self):
        baseline = dict(config={'model': 'sequential', 'workers': '1', 'quantum': '0'}, values=[4.] * 10, source='base')
        parallel = dict(config={'model': 'cmp', 'workers': '2', 'quantum': '0'}, values=[2.] * 10, source='cmp')
        result = report.make_record(parallel, baseline, 200)
        self.assertEqual(result['speedup'], 2)
        self.assertEqual(result['speedup_ci95_low'], 2)
        self.assertEqual(result['speedup_ci95_high'], 2)
        self.assertEqual(result['efficiency'], 1)
        self.assertEqual(report.make_record(baseline, baseline, 200)['speedup_ci95_high'], 1)
        missing = report.make_record(parallel, None, 200)
        self.assertIsNone(missing['speedup'])
        self.assertIsNone(missing['efficiency'])

    def test_bootstrap_is_reproducible_and_handles_variation(self):
        a = report.speedup_interval([2., 3., 4.], [1., 1.5, 2.], 500)
        self.assertEqual(a, report.speedup_interval([2., 3., 4.], [1., 1.5, 2.], 500))
        self.assertLess(a[0], 2)
        self.assertGreater(a[1], 2)

    def test_separates_inputs_topology_and_keeps_latest(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            lot(root, 'benchmark-01')
            latest = lot(root, 'benchmark-02', values=(3., 3.))
            lot(root, 'benchmark-03', bodies=200)
            lot(root, 'benchmark-04', topology='physical_cores=4,smt_active=1')
            incomplete = lot(root, 'benchmark-05')
            (incomplete / 'summary.csv').unlink()
            invalid = lot(root, 'benchmark-06', values=(0., 1.))
            groups, excluded = report.collect(root)
            self.assertEqual(len(groups), 3)
            lots = [v for group in groups.values() for v in group.values()]
            self.assertIn(str(latest), [v['source'] for v in lots])
            self.assertNotIn(str(invalid), [v['source'] for v in lots])
            self.assertEqual(len(excluded), 3)

    def test_report_scaling_exports_and_script_escaping(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            base = lot(root, 'benchmark-01')
            lot(root, 'benchmark-02', model='cmp', workers=1, values=(1., 1.))
            lot(root, 'benchmark-03', model='cmp', workers=2, values=(.5, .5))
            # Datos externos no pueden cerrar el bloque JSON e inyectar HTML.
            for folder in root.iterdir():
                (folder / 'environment.txt').write_text('Linux test\nModel name: </script><script>bad()</script>\n')
            destination = root / 'report.html'
            cohorts = report.build_report(root, destination, 200)
            self.assertEqual(len(cohorts), 1)
            self.assertIn('cmp', cohorts[0]['scaling'])
            self.assertEqual(len(cohorts[0]['records']), 3)
            text = destination.read_text()
            self.assertNotIn('__REPORT_DATA__', text)
            self.assertNotIn('<script>bad()</script>', text)
            payload = text.split('<script type="application/json" id="report-data">', 1)[1].split('</script>', 1)[0]
            self.assertEqual(json.loads(payload)['cohorts'][0]['records'][0]['source'], str(base))
            with destination.with_suffix('.csv').open() as source:
                self.assertEqual(len(list(csv.DictReader(source))), 3)
            with destination.with_name('report-samples.csv').open() as source:
                self.assertEqual(len(list(csv.DictReader(source))), 6)


if __name__ == '__main__':
    unittest.main()
