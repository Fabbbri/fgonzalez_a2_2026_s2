"""Pruebas reales con MNIST; no alteran SMT ni los resultados del usuario."""
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest
import numpy as np
ROOT = Path(__file__).resolve().parents[1]
class ExecutionTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="dl-execution-")
        self.addCleanup(self.temp.cleanup)
        self.output = Path(self.temp.name) / "params"
    def run_model(self, name, *args, **env):
        result = subprocess.run([str(ROOT/name), *map(str,args)], cwd=ROOT,
            env={**os.environ,"DL_SAMPLES":"5","DL_SEED":"1","DL_QUIET":"1",
                 "DL_PARAMS":str(self.output),**env}, text=True,capture_output=True,timeout=60)
        self.assertEqual(result.returncode,0,result.stderr)
        values=np.fromfile(self.output,dtype=np.float64)
        self.assertEqual(len(values),109386)
        self.assertTrue(np.isfinite(values).all())
        self.assertRegex(result.stdout,r"Tiempo total: [0-9]+ us")
        return values,result.stdout
    def test_coarse_partition_and_periodic_blocks_preserve_training(self):
        base,_=self.run_model("main",4)
        for workers in [1,2,4,8]:
            with self.subTest(workers=workers):
                values,out=self.run_model("grueso",4,workers,1,1)
                np.testing.assert_array_equal(values,base)
                self.assertIn(f"Hilos: {workers}\n",out)
        values,out=self.run_model("grueso",4,4,0,1)
        self.assertIn("Bloqueos periódicos: 0",out)
        np.testing.assert_array_equal(values,base)
        _,out=self.run_model("grueso",4,4,1,1)
        self.assertGreater(int(re.search(r"Bloqueos periódicos: (\d+)",out)[1]),0)
    def test_fine_smt_cmp_match_sequential(self):
        base,_=self.run_model("main",3)
        values,_=self.run_model("fino",3)
        np.testing.assert_allclose(values,base,rtol=1e-9,atol=1e-11)
        for model in ['smt.bin','cmp']:
            values,out=self.run_model(model,1,*(['','3'] if model=='smt.bin' else [3]))
            np.testing.assert_allclose(values,base,rtol=1e-9,atol=1e-11)
            self.assertIn("CPU fijadas y verificadas:",out)
            # El secuencial y CMP/SMT con P=1 comparten el mismo orden aritmético.
            np.testing.assert_array_equal(values,base)
    def test_affinity_respects_taskset(self):
        cpu=min(os.sched_getaffinity(0))
        for model,args in [('cmp',['1','1']),('smt.bin',['2','','1'])]:
            result=subprocess.run(['taskset','-c',str(cpu),str(ROOT/model),*args],cwd=ROOT,
                env={**os.environ,'DL_SAMPLES':'2','DL_QUIET':'1'},capture_output=True,text=True,timeout=60)
            self.assertEqual(result.returncode,0,result.stderr)
            expected=str(cpu) if model=='cmp' else f'{cpu};{cpu}'
            self.assertIn('CPU fijadas y verificadas: '+expected+';',result.stdout)
        result=subprocess.run(['taskset','-c',str(cpu),str(ROOT/'cmp'),'2','1'],cwd=ROOT,capture_output=True,text=True)
        self.assertNotEqual(result.returncode,0)
    def test_complete_mnist_batch_with_eight_and_twelve_workers(self):
        base,_=self.run_model('main',10,DL_SAMPLES='100')
        for name,args in [('fino',[10]),('grueso',[10,8,10,1]),
                          ('smt.bin',[12,'',10]),('cmp',[8,10])]:
            values,_=self.run_model(name,*args,DL_SAMPLES='100')
            np.testing.assert_allclose(values,base,rtol=1e-9,atol=1e-11)

    def test_seed_is_repeatable_and_affects_parameters(self):
        first,_=self.run_model('main',1)
        second,_=self.run_model('main',1)
        np.testing.assert_array_equal(first,second)
        changed,_=self.run_model('main',1,DL_SEED='2')
        self.assertFalse(np.array_equal(first,changed))
if __name__=='__main__': unittest.main()
