import argparse
import contextlib
import csv
import io
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
import benchmark
import plot_metrics
import rebase_reference
class BenchmarkTests(unittest.TestCase):
    def setUp(self):
        temp=tempfile.TemporaryDirectory(prefix='dl-benchmark-test-')
        self.addCleanup(temp.cleanup)
        self.args=argparse.Namespace(binary=str(ROOT/'main'),results_dir=Path(temp.name),
            repetitions=2,models=['sequential','fine','coarse','smt','cmp'],model=None,
            epochs=2,samples=3,seed=1,quantum_us=100,block_interval_ms=1,block_duration_ms=1,
            thread_counts='1,2',cmp_thread_counts='1,2',smt_thread_counts='1,2',threads=None,
            baseline_csv=None,smt_states='keep',make_command='make')
        p=patch.object(benchmark,'generate');p.start();self.addCleanup(p.stop)
        out=contextlib.redirect_stdout(io.StringIO());out.__enter__();self.addCleanup(out.__exit__,None,None,None)
    def rows(self,name):
        with (self.args.results_dir/name).open() as stream:return list(csv.DictReader(stream))
    def test_csv_metrics_and_immutable_reference(self):
        result=benchmark.campaign(self.args)
        for name,n in [('sequential',2),('fine',2),('coarse',4),('cmp',4)]:
            rows=self.rows(name+'.csv');self.assertEqual(len(rows),n)
            self.assertEqual(set(rows[0]),set(benchmark.FIELDS))
            for row in rows:
                speedup=float(row['tiempo_secuencial_ms'])/float(row['tiempo_total_ms'])
                self.assertAlmostEqual(float(row['speedup']),speedup)
                self.assertAlmostEqual(float(row['eficiencia_paralela']),speedup/int(row['hilos']))
        groups,workload=plot_metrics.load([str(self.args.results_dir/'*.csv')])
        self.assertEqual(len(groups),8)
        self.assertEqual(workload[:4],('2','3','0.1','1'))
        self.assertEqual(result['metadata']['status'],'complete')
        self.args.baseline_csv=self.args.results_dir/self.rows('cmp.csv')[0]['referencia_csv']
        before=(self.args.results_dir/'sequential.csv').read_bytes()
        self.args.models=['cmp'];benchmark.campaign(self.args)
        self.assertEqual((self.args.results_dir/'sequential.csv').read_bytes(),before)
        self.assertTrue(list((self.args.results_dir/'backups').glob('*/cmp.csv')))
        plot_metrics.load([str(self.args.results_dir/'cmp.csv')])
    def test_resume_preserves_rows_adds_workers_and_only_fills_missing_repetitions(self):
        benchmark.campaign(self.args)
        old=self.rows('coarse.csv')
        self.args.models=['coarse'];self.args.baseline_csv=self.args.results_dir/'sequential.csv'
        self.args.resume=True;self.args.repetitions=3;self.args.thread_counts='1,2,4'
        benchmark.campaign(self.args)
        rows=self.rows('coarse.csv')
        self.assertEqual(rows[:len(old)],old)
        self.assertEqual(len(rows),9)
        for workers in ['1','2','4']:
            self.assertEqual(sorted(int(r['repeticion']) for r in rows if r['hilos']==workers),[1,2,3])
        before=(self.args.results_dir/'coarse.csv').read_bytes()
        self.args.repetitions=2
        with patch.object(benchmark.subprocess,'run',side_effect=AssertionError('No debe volver a medir')):
            benchmark.campaign(self.args)
        self.assertEqual((self.args.results_dir/'coarse.csv').read_bytes(),before)
        plot_metrics.load([str(self.args.results_dir/'coarse.csv')])
        self.args.block_interval_ms=2
        with self.assertRaisesRegex(ValueError,'CSV previo incompatible'):
            benchmark.campaign(self.args)
        self.assertEqual((self.args.results_dir/'coarse.csv').read_bytes(),before)

    def test_rebase_recalculates_metrics_without_changing_measured_times(self):
        benchmark.campaign(self.args)
        old=self.rows('cmp.csv')
        old_reference=self.args.results_dir/old[0]['referencia_csv']
        reference_bytes=old_reference.read_bytes()
        baseline=self.args.results_dir/'sequential.csv'
        seq=self.rows('sequential.csv')
        for row in seq:
            row['tiempo_total_ms']=float(row['tiempo_total_ms'])*2
            row['tiempo_secuencial_ms']=row['tiempo_total_ms']
        rebase_reference.write_rows(baseline,seq)
        result=rebase_reference.rebase(self.args.results_dir,baseline,plots=False)
        new=self.rows('cmp.csv')
        self.assertEqual([r['tiempo_total_ms'] for r in new],[r['tiempo_total_ms'] for r in old])
        for before,after in zip(old,new):
            self.assertAlmostEqual(float(after['speedup']),2*float(before['speedup']))
            self.assertAlmostEqual(float(after['eficiencia_paralela']),2*float(before['eficiencia_paralela']))
        self.assertEqual(old_reference.read_bytes(),reference_bytes)
        self.assertNotEqual(new[0]['referencia_csv'],old[0]['referencia_csv'])
        self.assertEqual(result['status'],'complete')
        self.assertTrue(list((self.args.results_dir/'backups').glob('rebase-*/cmp.csv')))
        plot_metrics.load([str(self.args.results_dir/'*.csv')])

    def test_rebase_rejects_mismatched_workloads_without_touching_model_csv(self):
        benchmark.campaign(self.args)
        old=(self.args.results_dir/'cmp.csv').read_bytes()
        baseline=self.args.results_dir/'sequential.csv'
        seq=self.rows('sequential.csv')
        for row in seq:row['semilla']='999'
        rebase_reference.write_rows(baseline,seq)
        with self.assertRaisesRegex(ValueError,'cargas diferentes'):
            rebase_reference.rebase(self.args.results_dir,baseline,plots=False)
        self.assertEqual((self.args.results_dir/'cmp.csv').read_bytes(),old)
        self.assertFalse(list((self.args.results_dir/'backups').glob('rebase-*')))

    def test_incompatible_reference_fails_before_overwriting_cmp(self):
        benchmark.campaign(self.args)
        old=(self.args.results_dir/'cmp.csv').read_bytes()
        self.args.models=['cmp'];self.args.baseline_csv=self.args.results_dir/'sequential.csv'
        self.args.seed=2
        with self.assertRaisesRegex(ValueError,'Referencia incompatible'):
            benchmark.campaign(self.args)
        self.assertEqual((self.args.results_dir/'cmp.csv').read_bytes(),old)
    def test_numerical_mismatch_is_rejected(self):
        benchmark.campaign(self.args)
        self.args.models=['cmp'];self.args.baseline_csv=self.args.results_dir/'sequential.csv'
        row=self.rows('sequential.csv')[0]
        path=self.args.results_dir/row['parametros_archivo']
        values=benchmark.read_parameters(path);values[0]+=1;values.tofile(path)
        with self.assertRaisesRegex(ValueError,'Parámetros de referencia'):
            benchmark.campaign(self.args)
    def test_smt_phases_keep_same_workers_and_restore_on_error(self):
        self.args.models=['smt'];self.args.smt_states='on,off'
        measured=[]
        def phase(args):
            measured.append(args)
            if len(measured)==2:raise ValueError('simulated failure')
            return dict(generated=[],metadata={},logs=self.args.results_dir)
        with patch.object(benchmark,'read_smt_state',return_value='off'), \
             patch.object(benchmark,'authorize_smt_control'), \
             patch.object(benchmark,'set_smt_state') as control, \
             patch.object(benchmark,'campaign',side_effect=phase):
            with self.assertRaisesRegex(ValueError,'simulated failure'):benchmark.run_campaigns(self.args)
        self.assertEqual(control.call_args.args,('off','make'))
        self.assertEqual([p.baseline_label for p in measured],['sequential-on','sequential-off'])
        self.assertEqual(measured[0].smt_thread_counts,measured[1].smt_thread_counts)
    def test_single_off_preserves_sequential_and_restores_after_interrupt(self):
        self.args.models=['smt'];self.args.smt_states='off'
        with patch.object(benchmark,'read_smt_state',return_value='on'), \
             patch.object(benchmark,'authorize_smt_control'), \
             patch.object(benchmark,'set_smt_state') as control, \
             patch.object(benchmark,'campaign',side_effect=KeyboardInterrupt) as campaign:
            with self.assertRaises(KeyboardInterrupt):benchmark.run_campaigns(self.args)
        self.assertEqual(campaign.call_args.args[0].baseline_label,'sequential-off')
        self.assertEqual(control.call_args.args,('on','make'))
if __name__=='__main__':unittest.main()
