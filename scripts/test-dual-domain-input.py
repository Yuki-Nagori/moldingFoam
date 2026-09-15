#!/usr/bin/env python3
"""T102 synthetic fixture tests; no Mug assets or OpenFOAM needed."""
# SPDX-License-Identifier: GPL-3.0-or-later
import copy
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from dual_domain_input import InputError, read, read_experiment, validate

ROOT = Path(__file__).resolve().parent.parent
FIXTURE = ROOT / "tests/fixtures/dual-domain-v1.sample.json"
READER = ROOT / "scripts/dual_domain_input.py"


class InputTests(unittest.TestCase):
    def setUp(self):
        self.data = json.loads(FIXTURE.read_text())

    def test_sample_preserves_units_values_and_topology(self):
        original = copy.deepcopy(self.data)
        self.assertEqual(read(FIXTURE), original)
        self.assertEqual(len(original["nodes"]), 8)
        self.assertEqual(len(original["triangles"]), 12)
        self.assertEqual(original["thickness"], [10.0] * 12)
        self.assertEqual(original["lengthUnit"], "mm")
        self.assertEqual(original["thicknessUnit"], "mm")
        validate(self.data)
        self.assertEqual(self.data, original)

    def test_required_keys_and_version(self):
        for key in self.data:
            with self.subTest(missing=key):
                bad = copy.deepcopy(self.data)
                del bad[key]
                with self.assertRaises(InputError):
                    validate(bad)
        for key in ("schemaVersion", "lengthUnit", "thicknessUnit"):
            bad = copy.deepcopy(self.data)
            bad[key] = "unsupported"
            with self.assertRaisesRegex(InputError, key):
                validate(bad)
        self.data["unexpected"] = 1
        with self.assertRaises(InputError):
            validate(self.data)

    def test_indices_are_integers_in_range(self):
        for value in (-1, 8, True, 1.0, "1"):
            with self.subTest(value=value):
                bad = copy.deepcopy(self.data)
                bad["triangles"][0][0] = value
                with self.assertRaisesRegex(InputError, "index"):
                    validate(bad)

    def test_thickness(self):
        for value in (0, -1, float("nan"), float("inf"), True, "10", None):
            with self.subTest(value=value):
                bad = copy.deepcopy(self.data)
                bad["thickness"][0] = value
                with self.assertRaisesRegex(InputError, "thickness"):
                    validate(bad)
        self.data["thickness"].pop()
        with self.assertRaisesRegex(InputError, "one value per triangle"):
            validate(self.data)

    def test_coordinates(self):
        for node in ([0, 1], [True, 0, 1], [float("inf"), 0, 1],
                     [10**400, 0, 1], ["0", 0, 1]):
            bad = copy.deepcopy(self.data)
            bad["nodes"][0] = node
            with self.assertRaisesRegex(InputError, "nodes"):
                validate(bad)

    def test_degenerate_duplicate_and_flipped_faces(self):
        variants = []
        bad = copy.deepcopy(self.data)
        bad["triangles"][0] = [0, 0, 1]
        variants.append(bad)
        bad = copy.deepcopy(self.data)
        bad["triangles"][1] = bad["triangles"][0][:]
        variants.append(bad)
        for bad in variants:
            with self.subTest(triangles=bad["triangles"][:2]):
                with self.assertRaises(InputError):
                    validate(bad)

    def test_nonmanifold_edge(self):
        self.data["nodes"].append([5, -5, 5])
        self.data["triangles"].append([0, 1, 8])
        self.data["thickness"].append(10)
        with self.assertRaisesRegex(InputError, "non-manifold"):
            validate(self.data)

    def test_beams_and_couplings(self):
        self.data["beams"] = [{"nodes": [0, 1], "diameter": 2, "kind": "gate"}]
        self.data["couplings"] = [{"beam": 0, "endpoint": 1, "node": 1, "distance": 0}]
        self.assertEqual(validate(self.data), self.data)
        for key, value in (("beam", 1), ("endpoint", 2), ("node", 8),
                           ("distance", -1)):
            bad = copy.deepcopy(self.data)
            bad["couplings"][0][key] = value
            with self.assertRaises(InputError):
                validate(bad)
        for key, value in (("nodes", [0, 8]), ("nodes", [0, 0]),
                           ("diameter", 0), ("kind", "unknown")):
            bad = copy.deepcopy(self.data)
            bad["beams"][0][key] = value
            with self.assertRaises(InputError):
                validate(bad)
        self.data["couplings"] *= 2
        with self.assertRaisesRegex(InputError, "duplicate endpoint"):
            validate(self.data)

    def test_cli_success_is_not_solver_success(self):
        run = subprocess.run([sys.executable, str(READER), str(FIXTURE)],
                             capture_output=True, text=True, timeout=5)
        self.assertEqual(run.returncode, 0, run.stderr)
        summary = json.loads(run.stdout)
        self.assertEqual(summary["status"], "input_valid")
        self.assertFalse(summary["solverReady"])
        self.assertEqual((summary["nodes"], summary["triangles"]), (8, 12))
        self.assertEqual(summary["thicknessMin"], 10)
        self.assertEqual(len(summary["sourceDataDigest"]), 64)

    def test_cli_failure_retains_diagnostic_log(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "invalid.json"
            log = Path(tmp) / "reader.log"
            for content in ('{"nodes": [], "nodes": []}', '{broken', 'null'):
                path.write_text(content)
                with log.open("w") as stream:
                    run = subprocess.run([sys.executable, str(READER), str(path)],
                                         stdout=stream, stderr=stream, timeout=5)
                self.assertNotEqual(run.returncode, 0)
                self.assertIn(str(path), log.read_text())
                self.assertNotIn("Traceback", log.read_text())


class ExperimentTests(unittest.TestCase):
    def setUp(self):
        self.manifest = FIXTURE.with_name("dual-domain-v1-experiment-manifest.json")
        self.data = json.loads(self.manifest.read_text())
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.path = Path(self.tmp.name) / "experiment.json"
        (self.path.parent / FIXTURE.name).write_bytes(FIXTURE.read_bytes())

    def write(self, data):
        self.path.write_text(json.dumps(data))
        return self.path

    def test_frozen_fixture_values(self):
        manifest, mesh = read_experiment(self.manifest)
        self.assertEqual(manifest, self.data)
        self.assertEqual(mesh, read(FIXTURE))
        self.assertEqual(manifest["material"], {"id": "PP-REF-01", "source": "kairos-builtin"})
        self.assertEqual(manifest["process"], {
            "meltTemperatureC": 220.0, "moldTemperatureC": 50.0,
            "injectionTimeS": 5.5, "packingPressureCurveMpa":
                [[0.0, 0.9229], [0.2, 27.6282], [315.0797, 27.6282]],
            "coolingTimeS": 20.0})

    def test_bad_curve(self):
        for curve in ([], [[0, 1]], [[1, 1], [2, 2]], [[0, 1], [0, 2]],
                      [[0, 1], [-1, 2]], [[0, -1], [1, 2]],
                      [[0, 1], [float("nan"), 2]], [[0, 1], [1]]):
            with self.subTest(curve=curve):
                bad = copy.deepcopy(self.data)
                bad["process"]["packingPressureCurveMpa"] = curve
                with self.assertRaisesRegex(InputError, "packingPressureCurveMpa"):
                    read_experiment(self.write(bad))

    def test_bad_process(self):
        for key, value in (("meltTemperatureC", -273.15), ("moldTemperatureC", True),
                           ("injectionTimeS", 0), ("coolingTimeS", -1)):
            bad = copy.deepcopy(self.data)
            bad["process"][key] = value
            with self.assertRaisesRegex(InputError, key):
                read_experiment(self.write(bad))

    def test_contract_mismatch(self):
        for key, value in (("schemaVersion", "v2"), ("stage", "fill"),
                           ("material", {"id": "unknown", "source": "kairos-builtin"}),
                           ("expectedUnits", {"pressure": "Pa", "temperature": "K", "time": "s"})):
            bad = copy.deepcopy(self.data)
            bad[key] = value
            with self.assertRaises(InputError):
                read_experiment(self.write(bad))
        self.data["mesh"]["lengthUnit"] = "m"
        with self.assertRaisesRegex(InputError, "mesh.lengthUnit"):
            read_experiment(self.write(self.data))

    def test_mesh_reference_and_nested_validation(self):
        for reference in ("../outside.json", str(FIXTURE), "", 42):
            self.data["mesh"]["fixture"] = reference
            with self.assertRaisesRegex(InputError, "mesh.fixture"):
                read_experiment(self.write(self.data))
        self.data["mesh"]["fixture"] = FIXTURE.name
        bad_mesh = json.loads(FIXTURE.read_text())
        bad_mesh["thickness"][0] = 0
        (self.path.parent / FIXTURE.name).write_text(json.dumps(bad_mesh))
        with self.assertRaisesRegex(InputError, "thickness"):
            read_experiment(self.write(self.data))

    def test_manifest_cli(self):
        run = subprocess.run([sys.executable, str(READER), "--experiment", str(self.manifest)],
                             capture_output=True, text=True, timeout=5)
        self.assertEqual(run.returncode, 0, run.stderr)
        summary = json.loads(run.stdout)
        self.assertEqual(summary["experimentSchemaVersion"], "dual-domain-experiment/v1")
        self.assertFalse(summary["solverReady"])
        self.assertFalse(summary["materialResolved"])


if __name__ == "__main__":
    unittest.main()
