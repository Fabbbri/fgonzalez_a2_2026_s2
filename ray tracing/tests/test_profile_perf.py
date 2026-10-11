import importlib.util
from pathlib import Path
import unittest
import tempfile
import sys
import csv
import json

spec = importlib.util.spec_from_file_location("profile_perf", Path(__file__).parents[1] / "scripts/profile_perf.py")
profile = importlib.util.module_from_spec(spec)
spec.loader.exec_module(profile)
sys.path.insert(0, str(Path(__file__).parents[1] / 'scripts'))
import plot_profile_comparison as comparison


class PerfTests(unittest.TestCase):
    def test_comparison_preserves_configurations_and_deduplicates_shared_reference(self):
        with tempfile.TemporaryDirectory() as directory:
            captures=[]
            for model,state,p in [('sequential','1',1),('smt','1',12),('smt','0',12)]:
                capture=Path(directory)/f'{model}-{state}'
                capture.mkdir()
                captures.append(capture)
                metadata=dict(carga_argumentos=['--samples','8'], alcance='process', eventos='cycles',
                              modo_conteo='observed', topologia=f'smt_activo_linux={state}',
                              comandos={model:['--threads',str(p)]})
                if model!='sequential': metadata['referencia_compartida']='../sequential-1'
                (capture/'metadata.json').write_text(json.dumps(metadata))
                rows=[]
                for name in set(['sequential',model]):
                    for rep,value,status in [(1,1,'ok'),(2,1000,'ok'),(3,'','<not counted>')]:
                        rows.append(dict(modelo=name,repeticion=rep,cpu='programa',evento='cpu_atom/cycles/',
                                         unidad='',valor=value,estado=status,porcentaje_activo='0.00'))
                with (capture/'contadores.csv').open('w',newline='') as stream:
                    writer=csv.DictWriter(stream,fieldnames=list(rows[0]))
                    writer.writeheader(); writer.writerows(rows)
            entries,_=comparison.load(captures)
            self.assertEqual(len(entries),3)
            self.assertEqual(sum(e['model']=='sequential' for e in entries),1)
            self.assertEqual({e['state'] for e in entries if e['model']=='smt'},{'on','off'})
            self.assertEqual([r['valor'] for r in entries[0]['rows'] if r['estado']=='ok'],[1,1000])
            metadata['modo_conteo']='scaled'
            (captures[-1]/'metadata.json').write_text(json.dumps(metadata))
            with self.assertRaises(ValueError): comparison.load(captures)

    def test_compact_logs_keep_only_diagnostics(self):
        with tempfile.TemporaryDirectory() as directory:
            stem = Path(directory) / 'smt-001'
            result = profile.capture_process([sys.executable, '-c', 'print("ok")'], stem, None)
            self.assertEqual(result.returncode, 0)
            self.assertEqual(list(Path(directory).iterdir()), [])
            result = profile.capture_process([sys.executable, '-c', 'import sys; sys.stderr.write("fallo"); sys.exit(1)'], stem, None)
            self.assertEqual(result.returncode, 1)
            self.assertEqual(stem.with_suffix('.stderr.txt').read_text(), 'fallo')

    def test_detailed_logs_preserve_both_streams(self):
        with tempfile.TemporaryDirectory() as directory:
            stem = Path(directory) / 'smt-001'
            profile.capture_process([sys.executable, '-c', 'import sys; print("salida"); sys.stderr.write("aviso")'], stem, None, True)
            self.assertEqual(stem.with_suffix('.stdout.txt').read_text(), 'salida\n')
            self.assertEqual(stem.with_suffix('.stderr.txt').read_text(), 'aviso')

    def test_observed_capture_separates_hybrid_groups_and_disables_scaling(self):
        flags=profile.capture_flags(profile.DEFAULT_EVENTS,'process',[], 'observed', ['cpu_core','cpu_atom'])
        self.assertIn('--no-scale',flags)
        events=flags[flags.index('-e')+1]
        self.assertIn('{cpu_core/cycles/,cpu_core/instructions/,cpu_core/cache-misses/}',events)
        self.assertIn('{cpu_atom/cycles/,cpu_atom/instructions/,cpu_atom/cache-misses/}',events)
        self.assertNotIn('--no-scale',profile.capture_flags('cycles','process',[],'scaled'))

    def test_observed_short_pmu_residence_is_preserved(self):
        row=profile.parse_stat('123;;cpu_atom/cache-misses/;100;0.00;;\n','process')[0]
        self.assertEqual(profile.measurement_issue(row,scaled=False),'')
        row['estado']='<not counted>'
        self.assertEqual(profile.measurement_issue(row,scaled=False),'<not counted>')

    def test_numeric_counter_with_low_coverage_is_kept_with_warning(self):
        row = profile.parse_stat('999999;;cpu_atom/cache-misses/;123;0.00;;\n','process')[0]
        self.assertEqual(row['estado'],'ok')  # Captura cruda permanece intacta.
        self.assertEqual(profile.measurement_issue(row),'')
        self.assertEqual(profile.measurement_warning(row),'porcentaje medido bajo')
        row['porcentaje_activo']='49.99'
        self.assertEqual(profile.measurement_issue(row),'')
        self.assertTrue(profile.measurement_warning(row))
        row['porcentaje_activo']='50.00'
        self.assertEqual(profile.measurement_issue(row),'')
        self.assertEqual(profile.measurement_warning(row),'')
        row['porcentaje_activo']=''
        self.assertEqual(profile.measurement_issue(row),'')
        self.assertEqual(profile.measurement_warning(row),'cobertura no disponible')

    def test_old_ipc_recomputed_from_each_repetition(self):
        rows=[]
        for repetition, active in ((1,'0.00'),(2,'99.00')):
            raw=profile.parse_stat(f'100;;cpu_atom/cycles/;123;{active};;\n200;;cpu_atom/instructions/;123;{active};;\n','process')
            rows.extend(dict(modelo='fine',repeticion=repetition,**r) for r in raw)
        rows.append(dict(modelo='fine',repeticion=1,cpu='programa',evento='IPC derivado: cpu_atom/cycles/',
                         unidad='instrucciones/ciclo',valor=2,estado='ok',porcentaje_activo=''))
        ipc=[r for r in profile.assessed_rows(rows) if r['evento'].startswith('IPC derivado:')]
        self.assertEqual(len(ipc),2)
        self.assertEqual(profile.measurement_issue(ipc[0]),'')
        self.assertEqual(profile.measurement_warning(ipc[0]),'porcentaje medido bajo')
        self.assertEqual(profile.measurement_issue(ipc[1]),'')
        self.assertEqual(ipc[1]['valor'],2)

    def test_hybrid_domains_and_missing_counters(self):
        text = "# perf stat\n100;;cpu_core/cycles/;123;80.00;;\n200;;cpu_atom/cycles/;123;95.00;;\n<not supported>;;cache-misses;0;0;;\n"
        rows = profile.parse_stat(text, "process")
        self.assertEqual(len(rows), 3)
        self.assertEqual(rows[0]["valor"], 100)
        self.assertEqual(rows[0]["porcentaje_activo"], "80.00")
        self.assertNotEqual(rows[0]["evento"], rows[1]["evento"])
        self.assertEqual(rows[2]["valor"], "")
        self.assertEqual(rows[2]["estado"], "<not supported>")

    def test_cpu_rows_do_not_merge(self):
        rows = profile.parse_stat("CPU0;100;;cycles;123;100.00;;\nCPU1;200;;cycles;123;100.00;;\n", "cpu")
        self.assertEqual([row["cpu"] for row in rows], ["CPU0", "CPU1"])
        self.assertEqual([row["valor"] for row in rows], [100, 200])

    def test_ipc_matches_pmu_and_cpu(self):
        rows = profile.parse_stat("CPU0;100;;cpu_core/cycles/;123;100.00;;\nCPU0;200;;cpu_core/instructions/;123;100.00;;\nCPU1;300;;cpu_atom/instructions/;123;100.00;;\n", "cpu")
        ipc = profile.ipc_rows(rows)
        self.assertEqual(len(ipc), 1)
        self.assertEqual(ipc[0]["cpu"], "CPU0")
        self.assertEqual(ipc[0]["valor"], 2)

    def test_reference_preserves_workload(self):
        model, common = profile.configuration(["--width", "641", "--model", "smt", "--threads", "12", "--samples", "8"])
        self.assertEqual(model, "smt")
        self.assertEqual(common, ["--width", "641", "--samples", "8"])
        for args in (["--benchmark"], ["--output", "image.ppm"], ["--model"], ["--thread-counts", "1,2"],
                     ["--baseline-csv", "sequential.csv"]):
            with self.assertRaises(ValueError):
                profile.configuration(args)


if __name__ == "__main__":
    unittest.main()
