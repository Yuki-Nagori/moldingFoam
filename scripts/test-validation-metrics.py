#!/usr/bin/env python3
"""Fault injection for structural validation readers and measurement mode."""
import json
import os
from pathlib import Path
import shutil
import csv
import subprocess
import sys
import tempfile
import unittest
from validation_metrics import field, mesh_dimensions

SCRIPTS = Path(__file__).resolve().parent


class ValidationTests(unittest.TestCase):
    def test_runner_preserves_prepared_inputs_inside_git(self):
        with tempfile.TemporaryDirectory() as tmp:
            root = Path(tmp)
            subprocess.run(['git', 'init', '-q', str(root)], check=True)
            foam = root/'foam/bin/tools'
            foam.mkdir(parents=True)
            (foam/'RunFunctions').write_text('')
            commands = root/'bin'
            commands.mkdir()
            # Stop at mesh entry, after real runner cleanup, without a solver.
            mesh = commands/'blockMesh'
            mesh.write_text('#!/bin/sh\ntest -f 0/T || exit 42\nexit 23\n')
            mesh.chmod(0o755)
            env = dict(os.environ, WM_PROJECT_DIR=str(root/'foam'),
                       PATH=str(commands)+os.pathsep+os.environ['PATH'])
            for location in ('validation/tracked', '.uncertainty-matrix/copy'):
                case = root/location
                (case/'0').mkdir(parents=True)
                (case/'system').mkdir()
                (case/'system/verifier').write_text('verify-thermoelastic.py\n')
                initial = case/'0/T'
                initial.write_text('prepared profile\n')
                tracked = location.startswith('validation/')
                if tracked:
                    subprocess.run(['git', '-C', str(root), 'add', location], check=True)
                generated = case/'0/generated'
                generated.write_text('generated field\n')
                result = subprocess.run(['bash', str(SCRIPTS/'run-validation.sh'), str(case)],
                                        env=env, capture_output=True, text=True)
                self.assertEqual(result.returncode, 23, result.stderr)
                self.assertEqual(initial.read_text(), 'prepared profile\n')
                self.assertEqual(generated.exists(), not tracked)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.case = Path(self.tmp.name)
        (self.case/'system').mkdir()
        (self.case/'1').mkdir()
        (self.case/'system/blockMeshDict').write_text('vertices ((30 0 0) (0 1 1));\nblocks (hex (0 1 2 3 4 5 6 7) (2 2 1) simpleGrading (1 1 1));')
        (self.case/'system/controlDict').write_text('endTime 1;')
        (self.case/'log.foamRun').write_text('Time = 1\nEnd\n')
        self.path = self.case/'1/D'

    def test_mesh_ignores_vertices_grading_comments(self):
        self.assertEqual(mesh_dimensions(self.case), (2, 2))

    def test_strict_fields(self):
        for value in ['nonuniform List<vector> 4 ((1 2 3))',
                      'nonuniform List<vector> 1 ((1 2 3))',
                      'nonuniform List<vector> 4 ((1 2 3) (1 2 3) (1 2 3) (1 2 3) (1 2 3))',
                      'uniform (0 nan 0)', 'uniform (0 0 inf)',
                      'uniform (0 0 1e999)', 'uniform (1 2)',
                      'nonuniform List<vector> 4 ((0 nan 0) (0 0 0) (0 0 0) (0 0 0))']:
            with self.subTest(value=value):
                self.path.write_text('internalField '+value+';')
                with self.assertRaises(ValueError):
                    field(self.path, 'vector', 3, 4)
        self.path.write_text('internalField uniform (0 1 0);')
        self.assertEqual(field(self.path, 'vector', 3, 4), [[0, 1, 0]]*4)
        self.path.write_text('internalField nonuniform List<vector> 4 ((0 1 0) (0 2 0) (0 3 0) (0 4 0));')
        self.assertEqual(len(field(self.path, 'vector', 3, 4)), 4)

    def run_verifier(self, matrix):
        env = dict(os.environ)
        env.pop('UNCERTAINTY_MATRIX_MODE', None)
        env['UNCERTAINTY_METRICS_FILE'] = str(self.case/'metrics.json')
        if matrix:
            env['UNCERTAINTY_MATRIX_MODE'] = '1'
        return subprocess.run([sys.executable, str(SCRIPTS/'verify-thermoelastic.py'), str(self.case)],
                              env=env, capture_output=True, text=True)

    def test_threshold_and_invalid_data_are_distinct(self):
        expected = 1.1e-5*20*29.5**2/2
        self.path.write_text(f'internalField uniform (0 {-0.9*expected} 0);')
        self.assertNotEqual(self.run_verifier(False).returncode, 0)
        result = self.run_verifier(True)
        self.assertEqual(result.returncode, 0, result.stdout+result.stderr)
        self.assertNotIn('PASS', result.stdout)
        self.assertNotIn('FAIL', result.stdout)
        metrics = json.loads((self.case/'metrics.json').read_text())
        self.assertAlmostEqual(metrics['relative_error'], 0.1)
        self.assertFalse(metrics['within_threshold'])
        self.path.write_text('internalField uniform (0 nan 0);')
        self.assertNotEqual(self.run_verifier(True).returncode, 0)
        self.path.write_text(f'internalField uniform (0 {-expected} 0);')
        self.assertEqual(self.run_verifier(False).returncode, 0)
        (self.case/'log.foamRun').write_text('Time = 1\n')
        self.assertNotEqual(self.run_verifier(True).returncode, 0)
        (self.case/'log.foamRun').write_text('       End\n')
        (self.case/'system/controlDict').write_text('endTime 2;')
        self.assertNotEqual(self.run_verifier(True).returncode, 0)

    def test_matrix_records_missing_metrics_and_continues(self):
        repo = self.case/'repo'
        (repo/'scripts').mkdir(parents=True)
        shutil.copy(SCRIPTS/'uncertainty-matrix.sh', repo/'scripts')
        (repo/'scripts/prepare-uncertainty-case.py').write_text('')
        # A solver that exits successfully without metrics must never make CI green.
        for runner in ('run-validation.sh', 'run-moldcht.sh'):
            (repo/'scripts'/runner).write_text('exit 0\n')
        for name in ('thermoelastic', 'moldCHT', 'warpagePlate'):
            (repo/'validation'/name).mkdir(parents=True)
        env = dict(os.environ, UNCERTAINTY_MATRIX_DIR=str(self.case/'results'))
        env.pop('UNCERTAINTY_MATRIX_LIMIT', None)
        result = subprocess.run(['bash', str(repo/'scripts/uncertainty-matrix.sh'), '--run'],
                                env=env, capture_output=True, text=True)
        self.assertNotEqual(result.returncode, 0)
        table = next((self.case/'results').glob('run-*/results.tsv'))
        with table.open() as handle:
            rows = list(csv.DictReader(handle, delimiter='\t'))
        self.assertEqual(len(rows), 9)
        self.assertIn('structural_convergence', rows[0])
        self.assertEqual(rows[0]['structural_convergence'], 'unavailable')
        self.assertTrue(all(row['value'] == '' for row in rows))
        self.assertEqual(len(list(table.parent.glob('*/runner.log'))), 9)

    def test_cht_scalar_faults(self):
        import importlib.util
        spec = importlib.util.spec_from_file_location('cht', SCRIPTS/'verify-moldcht.py')
        cht = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cht)
        for value in ('uniform nan', 'uniform 1nan', 'uniform inf',
                      'nonuniform List<scalar> 2 (1)',
                      'nonuniform List<scalar> 2 (1 nan)'):
            self.path.write_text('internalField '+value+';')
            with self.subTest(value=value), self.assertRaises(ValueError):
                cht.cell_average(self.path)
        self.path.write_text('internalField uniform 420;')
        self.assertEqual(cht.cell_average(self.path), 420)

    def test_warpage_preparation_all_scales(self):
        for scale in (0.5, 1, 2):
            case = self.case/('grid-'+str(scale))
            shutil.copytree(SCRIPTS.parent/'validation/warpagePlate', case)
            result = subprocess.run([sys.executable, str(SCRIPTS/'prepare-uncertainty-case.py'),
                                     str(case), 'warpagePlate', str(scale), str(scale)],
                                    capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            nx, ny = mesh_dimensions(case)
            for name in ('T', 'e'):
                import re
                text = (case/'0'/name).read_text()
                match = re.search(r'internalField\s+nonuniform\s+List<scalar>\s+(\d+)\s*\((.*?)\)', text, re.S)
                values = list(map(float, match[2].split()))
                self.assertEqual(int(match[1]), nx*ny)
                self.assertEqual(len(values), nx*ny)
                self.assertEqual(len(set(values[:nx*ny//2])), 1)
                self.assertEqual(values[-1], 2*values[0])


if __name__ == '__main__':
    unittest.main()
