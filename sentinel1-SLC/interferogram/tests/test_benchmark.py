"""Pruebas de métricas con TIFF sintéticos; no cambian SMT ni datos reales."""
import argparse
import contextlib
import csv
import io
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import benchmark
import plot_metrics
import rebase_reference

class BenchmarkTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fixture_dir = tempfile.TemporaryDirectory(prefix='sentinel-fixture-')
        cls.addClassCleanup(cls.fixture_dir.cleanup)
        cls.inputs = Path(cls.fixture_dir.name)
        subprocess.run([str(ROOT/'build/benchmark_fixture'), str(cls.inputs)], check=True)

    def setUp(self):
        temp = tempfile.TemporaryDirectory(prefix='sentinel-benchmark-')
        self.addCleanup(temp.cleanup)
        _, topo = benchmark.topology()
        physical = int(topo['physical_cores'])
        self.args = argparse.Namespace(binary=str(ROOT/'build/interferogram'), results_dir=Path(temp.name),
            models=['sequential','fine','coarse','smt','cmp'], model=None, repetitions=2, warmups=1,
            a=self.inputs/'A.tif', b=self.inputs/'B.tif', x=2, y=3, width=128, height=64,
            tile_width=16, tile_height=16, quantum=2, allow_nodata=True,
            thread_counts='1,2', cmp_thread_counts='1', smt_thread_counts=str(physical),
            threads=None, baseline_csv=None, smt_states='keep', make_command='make', resume=False)
        self.plotter = patch.object(benchmark, 'generate')
        self.plotter.start(); self.addCleanup(self.plotter.stop)
        quiet = contextlib.redirect_stdout(io.StringIO())
        quiet.__enter__(); self.addCleanup(quiet.__exit__, None, None, None)

    def rows(self, name):
        with (self.args.results_dir/name).open() as stream:
            return list(csv.DictReader(stream))

    def test_smt_physical_core_control(self):
        # benchmark.py exige al menos un trabajador SMT por núcleo físico.
        self.args.models = ['smt']
        _, topo = benchmark.topology()
        physical = int(topo['physical_cores'])
        self.args.smt_thread_counts = str(physical)
        benchmark.run_campaigns(self.args)
        rows = self.rows('smt-' + {'1': 'on', '0': 'off'}.get(topo.get('smt_active'), 'unknown') + '.csv')
        self.assertTrue(all(row['hilos'] == str(physical) for row in rows))
        expected = ';'.join(map(str, benchmark.smt_cpus(topo, physical)))
        self.assertTrue(all(row['afinidad_cpus'] == expected for row in rows))

    def test_smt_cpu_order_and_oversubscription(self):
        self.assertEqual(benchmark.smt_cpus({'sibling_groups': '0;4|1;5'}, 6),
                         [0, 1, 4, 5, 0, 1])
        self.assertEqual(benchmark.smt_cpus({'sibling_groups': '0|1'}, 4), [0, 1, 0, 1])

    def test_native_models_kernel_time_correctness_and_layout(self):
        result = benchmark.run_campaigns(self.args)
        all_rows = []
        for path in result['generated']:
            rows = self.rows(path.name)
            all_rows += rows
            self.assertEqual(set(rows[0]), set(benchmark.FIELDS))
            self.assertTrue(path.with_name(path.stem+'-summary.csv').exists())
            for row in rows:
                self.assertGreaterEqual(float(row['tiempo_total_ms']), float(row['tiempo_kernel_ms']))
                self.assertGreater(float(row['tiempo_proceso_ms']), float(row['tiempo_kernel_ms']))
                speedup = float(row['tiempo_secuencial_ms'])/float(row['tiempo_total_ms'])
                self.assertAlmostEqual(float(row['speedup']), speedup)
                self.assertAlmostEqual(float(row['eficiencia_paralela']), speedup/int(row['hilos']))
        self.assertEqual(len({r['resultado_hash'] for r in all_rows}), 1)
        self.assertEqual({r['afinidad_cpus'] for r in all_rows if r['modelo']=='smt'}, {';'.join(map(str, benchmark.smt_cpus(benchmark.topology()[1], int(self.args.smt_thread_counts))))})
        commands = [json.loads(line) for line in (result['logs']/'commands.jsonl').read_text().splitlines()]
        self.assertEqual(len(commands), (1+2)*6)
        self.assertTrue(all('--output' not in command for command in commands))
        groups, _ = plot_metrics.load([str(self.args.results_dir/'*.csv')])
        self.assertEqual(len(groups), 6)
        self.assertEqual(result['metadata']['status'], 'complete')

    def test_resume_keeps_data_and_completes_missing_repetitions(self):
        self.args.models=['coarse']
        benchmark.campaign(self.args)
        original=self.rows('coarse.csv')
        self.args.baseline_csv=self.args.results_dir/'sequential.csv'
        self.args.repetitions=3; self.args.thread_counts='1,2,3'; self.args.resume=True
        benchmark.run_campaigns(self.args)
        rows=self.rows('coarse.csv')
        self.assertEqual(rows[:len(original)],original)
        self.assertEqual(len(rows),9)
        old=(self.args.results_dir/'coarse.csv').read_bytes()
        with patch.object(benchmark.subprocess,'run',side_effect=AssertionError('No repetir mediciones')):
            benchmark.run_campaigns(self.args)
        self.assertEqual((self.args.results_dir/'coarse.csv').read_bytes(),old)
        self.assertTrue(list((self.args.results_dir/'backups').glob('*/coarse.csv')))
        self.args.tile_width=8
        with self.assertRaisesRegex(ValueError,'CSV previo incompatible'):
            benchmark.run_campaigns(self.args)
        self.assertEqual((self.args.results_dir/'coarse.csv').read_bytes(),old)

    def test_resume_rejects_old_unpinned_smt(self):
        self.args.models = ['smt']
        benchmark.campaign(self.args)
        label = 'smt-' + {'1': 'on', '0': 'off'}.get(benchmark.topology()[1]['smt_active'], 'unknown') + '.csv'
        path = self.args.results_dir / label
        rows = self.rows(label)
        for row in rows:
            row['afinidad_cpus'] = 'sin_fijar_SO'
        rebase_reference.write_rows(path, rows)
        self.args.baseline_csv = self.args.results_dir / 'sequential.csv'
        self.args.resume = True
        with self.assertRaisesRegex(ValueError, 'CSV previo incompatible'):
            benchmark.run_campaigns(self.args)

    def test_external_reference_and_new_smt_reference_preserve_sequential(self):
        self.args.models=['sequential']
        benchmark.campaign(self.args)
        seq=self.args.results_dir/'sequential.csv'
        original=seq.read_bytes()
        self.args.models=['cmp']; self.args.baseline_csv=seq
        result=benchmark.campaign(self.args)
        self.assertEqual(seq.read_bytes(),original)
        commands=(result['logs']/'commands.jsonl').read_text()
        self.assertNotIn('"sequential"',commands)
        self.args.width=64
        old=(self.args.results_dir/'cmp.csv').read_bytes()
        with self.assertRaisesRegex(ValueError,'Referencia incompatible'):
            benchmark.campaign(self.args)
        self.assertEqual((self.args.results_dir/'cmp.csv').read_bytes(),old)

    def test_rebase_preserves_times_and_changes_denominator_with_backup(self):
        self.args.models=['coarse']
        benchmark.campaign(self.args)
        old=self.rows('coarse.csv')
        baseline=self.args.results_dir/'sequential.csv'
        rows=self.rows('sequential.csv')
        for row in rows:
            row['tiempo_total_ms']=float(row['tiempo_total_ms'])*2
            row['tiempo_kernel_ms']=row['tiempo_total_ms']
            row['tiempo_secuencial_ms']=row['tiempo_total_ms']
        rebase_reference.write_rows(baseline,rows)
        result=rebase_reference.rebase(self.args.results_dir,baseline,plots=False)
        new=self.rows('coarse.csv')
        self.assertEqual([r['tiempo_total_ms'] for r in new],[r['tiempo_total_ms'] for r in old])
        for before,after in zip(old,new):
            self.assertAlmostEqual(float(after['speedup']),2*float(before['speedup']))
            self.assertAlmostEqual(float(after['eficiencia_paralela']),2*float(before['eficiencia_paralela']))
        self.assertEqual(result['status'],'complete')
        self.assertTrue(list((self.args.results_dir/'backups').glob('rebase-*/coarse.csv')))
        plot_metrics.load([str(self.args.results_dir/'*.csv')])

    def test_result_fingerprint_mismatch_is_rejected(self):
        self.args.models=['sequential']
        benchmark.campaign(self.args)
        self.args.models=['coarse']; self.args.baseline_csv=self.args.results_dir/'sequential.csv'
        reference=self.rows('sequential.csv')
        for row in reference: row['resultado_hash']='deadbeef'
        with self.args.baseline_csv.open('w',newline='') as stream:
            writer=csv.DictWriter(stream,fieldnames=benchmark.FIELDS)
            writer.writeheader(); writer.writerows(reference)
        with self.assertRaisesRegex(ValueError,'resultado difiere'):
            benchmark.campaign(self.args)
        self.assertEqual(self.rows('coarse.csv'),[])

    def test_missing_inputs_fail_before_sudo_or_state_change(self):
        self.args.a=self.args.a.with_name('missing.tif')
        self.args.smt_states='on,off'
        with patch.object(benchmark,'authorize_smt_control') as authorize, \
             patch.object(benchmark,'set_smt_state') as change:
            with self.assertRaisesRegex(ValueError,'No existe el TIFF'):
                benchmark.run_campaigns(self.args)
        authorize.assert_not_called(); change.assert_not_called()

    def test_smt_states_fixed_counts_and_restoration_on_error(self):
        self.args.models=['smt']; self.args.smt_states='on,off'
        measured=[]
        def campaign(args):
            measured.append(args)
            if len(measured)==2: raise ValueError('simulated failure')
            return dict(generated=[],metadata={},logs=self.args.results_dir)
        with patch.object(benchmark,'read_smt_state',return_value='off'), \
             patch.object(benchmark,'authorize_smt_control'), \
             patch.object(benchmark,'set_smt_state') as control, \
             patch.object(benchmark,'campaign',side_effect=campaign):
            with self.assertRaisesRegex(ValueError,'simulated failure'):
                benchmark.run_campaigns(self.args)
        self.assertEqual(control.call_args.args,('off','make'))
        self.assertEqual([a.baseline_label for a in measured],['sequential-on','sequential-off'])
        self.assertEqual(measured[0].smt_thread_counts,measured[1].smt_thread_counts)

    def test_off_only_restores_after_interruption_and_uses_own_reference_name(self):
        self.args.models=['smt']; self.args.smt_states='off'
        with patch.object(benchmark,'read_smt_state',return_value='on'), \
             patch.object(benchmark,'authorize_smt_control'), \
             patch.object(benchmark,'set_smt_state') as control, \
             patch.object(benchmark,'campaign',side_effect=KeyboardInterrupt) as campaign:
            with self.assertRaises(KeyboardInterrupt): benchmark.run_campaigns(self.args)
        self.assertEqual(campaign.call_args.args[0].baseline_label,'sequential-off')
        self.assertEqual(control.call_args.args,('on','make'))

    def test_legacy_kernel_reference_is_rejected(self):
        self.args.models = ['sequential']
        benchmark.campaign(self.args)
        rows = self.rows('sequential.csv')
        for row in rows:
            row['alcance_tiempo'] = 'kernel_sin_lectura_creacion_cierre_exportacion_hash'
        baseline = self.args.results_dir / 'sequential.csv'
        rebase_reference.write_rows(baseline, rows)
        self.args.baseline_csv = baseline
        self.args.models = ['coarse']
        with self.assertRaisesRegex(ValueError, 'alcance'):
            benchmark.campaign(self.args)
        with self.assertRaisesRegex(ValueError, 'alcance'):
            plot_metrics.load([str(baseline)])

    def test_metrics_depend_on_model_not_kernel_or_wall_clock(self):
        self.args.models=['sequential']
        text='''Mode: sequential
Window: 128 x 64
Offset: 2, 3
Model time: 3.000000 ms
Kernel time: 2.000000 ms
Result hash (fnv1a64): abcdef
Valid result samples: 8192
Invalid result samples: 0
'''
        with patch.object(benchmark.subprocess,'run',return_value=subprocess.CompletedProcess([],0,text,'')), \
             patch.object(benchmark.time,'perf_counter',side_effect=[0.,1.]*3):
            benchmark.campaign(self.args)
        for row in self.rows('sequential.csv'):
            self.assertEqual(float(row['tiempo_total_ms']),3)
            self.assertEqual(float(row['tiempo_kernel_ms']),2)
            self.assertEqual(float(row['tiempo_proceso_ms']),1000)

if __name__=='__main__': unittest.main()
