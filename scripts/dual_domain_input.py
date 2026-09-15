#!/usr/bin/env python3
"""T102 JSON reader only: no case generation, material model or solver fallback."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
import hashlib
import json
import math
from pathlib import Path
import sys


class InputError(ValueError):
    """Invalid upstream JSON contract."""


def require(condition, location, message):
    if not condition:
        raise InputError(f"{location}: {message}")


def number(value, location):
    require(type(value) in (int, float), location, "expected finite number")
    try:
        finite = math.isfinite(value)
    except OverflowError:
        finite = False
    require(finite, location, "expected finite number")


def index(value, size, location):
    require(type(value) is int and 0 <= value < size,
            location, f"expected integer index in [0, {size})")


def fields(value, names, location):
    require(isinstance(value, dict), location, "expected object")
    require(set(value) == set(names), location,
            "required keys: " + ", ".join(names) + "; unknown keys are rejected")


def validate(data):
    fields(data, ("schemaVersion", "lengthUnit", "thicknessUnit", "nodes",
                  "triangles", "thickness", "beams", "couplings"), "$")
    for key, expected in (("schemaVersion", "dual-domain/v1"),
                          ("lengthUnit", "mm"), ("thicknessUnit", "mm")):
        require(data[key] == expected, key, f"expected {expected}")
    for key in ("nodes", "triangles", "thickness", "beams", "couplings"):
        require(isinstance(data[key], list), key, "expected array")
    nodes, triangles = data["nodes"], data["triangles"]
    require(bool(nodes), "nodes", "must not be empty")
    require(bool(triangles), "triangles", "must not be empty")
    for i, node in enumerate(nodes):
        require(isinstance(node, list) and len(node) == 3,
                f"nodes[{i}]", "expected [x,y,z]")
        for j, value in enumerate(node):
            number(value, f"nodes[{i}][{j}]")
    require(len(data["thickness"]) == len(triangles), "thickness",
            "one value per triangle required")
    for i, value in enumerate(data["thickness"]):
        number(value, f"thickness[{i}]")
        require(value > 0, f"thickness[{i}]", "must be positive; unmatched is invalid")
    seen, edges = set(), {}
    for i, triangle in enumerate(triangles):
        loc = f"triangles[{i}]"
        require(isinstance(triangle, list) and len(triangle) == 3,
                loc, "expected three node indices")
        for value in triangle:
            index(value, len(nodes), loc)
        require(len(set(triangle)) == 3, loc, "repeated node")
        key = tuple(sorted(triangle))
        require(key not in seen, loc, "duplicate triangle")
        seen.add(key)
        a, b, c = (nodes[k] for k in triangle)
        u = [float(b[j]) - float(a[j]) for j in range(3)]
        v = [float(c[j]) - float(a[j]) for j in range(3)]
        cross = [u[1]*v[2]-u[2]*v[1], u[2]*v[0]-u[0]*v[2],
                 u[0]*v[1]-u[1]*v[0]]
        area2 = math.hypot(*cross)
        require(math.isfinite(area2) and area2 > 0, loc, "degenerate/overflow area")
        for x, y in zip(triangle, triangle[1:] + triangle[:1]):
            edge = tuple(sorted((x, y)))
            owners = edges.setdefault(edge, [])
            require(len(owners) < 2, loc, f"non-manifold edge {edge}")
            require((x, y) not in owners, loc, f"inconsistent winding at edge {edge}")
            owners.append((x, y))
    for i, beam in enumerate(data["beams"]):
        loc = f"beams[{i}]"
        fields(beam, ("nodes", "diameter", "kind"), loc)
        require(isinstance(beam["nodes"], list) and len(beam["nodes"]) == 2,
                loc, "expected two endpoint indices")
        for value in beam["nodes"]:
            index(value, len(nodes), loc)
        a, b = beam["nodes"]
        require(nodes[a] != nodes[b], loc, "zero-length beam")
        number(beam["diameter"], loc + ".diameter")
        require(beam["diameter"] > 0, loc, "diameter must be positive")
        require(beam["kind"] in ("gate", "runner"), loc, "unknown kind")
    coupled = set()
    for i, coupling in enumerate(data["couplings"]):
        loc = f"couplings[{i}]"
        fields(coupling, ("beam", "endpoint", "node", "distance"), loc)
        index(coupling["beam"], len(data["beams"]), loc + ".beam")
        index(coupling["endpoint"], 2, loc + ".endpoint")
        index(coupling["node"], len(nodes), loc + ".node")
        number(coupling["distance"], loc + ".distance")
        require(coupling["distance"] >= 0, loc, "distance must be nonnegative")
        key = (coupling["beam"], coupling["endpoint"])
        require(key not in coupled, loc, "duplicate endpoint coupling")
        coupled.add(key)
    # No claim about face matching, midsurface semantics, or solver readiness.
    return data


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, key, "duplicate JSON key")
        result[key] = value
    return result


def read(path):
    with Path(path).open(encoding="utf-8") as stream:
        return validate(json.load(stream, object_pairs_hook=unique_object))


def read_experiment(path):
    """Validate manifest and relative mesh reference; do not resolve materials."""
    path = Path(path)
    with path.open(encoding="utf-8") as stream:
        data = json.load(stream, object_pairs_hook=unique_object)
    fields(data, ("schemaVersion", "mesh", "material", "stage", "process",
                  "expectedUnits"), "$")
    require(data["schemaVersion"] == "dual-domain-experiment/v1",
            "schemaVersion", "expected dual-domain-experiment/v1")
    mesh = data["mesh"]
    require(isinstance(mesh, dict), "mesh", "expected object")
    required_mesh = ("schemaVersion", "fixture", "lengthUnit", "thicknessUnit")
    require(all(key in mesh for key in required_mesh), "mesh",
            "missing required mesh key")
    require(set(mesh).issubset(set(required_mesh) | {"source", "nodeCount", "triangleCount"}),
            "mesh", "unknown key")
    for key, expected in (("schemaVersion", "dual-domain/v1"),
                          ("lengthUnit", "mm"), ("thicknessUnit", "mm")):
        require(mesh[key] == expected, "mesh." + key, f"expected {expected}")
    require(isinstance(mesh["fixture"], str) and bool(mesh["fixture"]),
            "mesh.fixture", "expected relative fixture path")
    relative = Path(mesh["fixture"])
    require(not relative.is_absolute() and ".." not in relative.parts,
            "mesh.fixture", "expected relative fixture path without parent traversal")
    target = (path.parent / relative).resolve()
    require(target.is_relative_to(path.parent.resolve()), "mesh.fixture",
            "fixture must remain inside manifest directory")
    mesh_data = read(target)
    if "source" in mesh:
        require(mesh["source"] == "mug-stl-generated", "mesh.source",
                "expected mug-stl-generated provenance")
    if "nodeCount" in mesh:
        require(mesh["nodeCount"] == len(mesh_data["nodes"]), "mesh.nodeCount",
                "does not match referenced fixture")
    if "triangleCount" in mesh:
        require(mesh["triangleCount"] == len(mesh_data["triangles"]), "mesh.triangleCount",
                "does not match referenced fixture")
    material = data["material"]
    fields(material, ("id", "source"), "material")
    require(material["id"] == "PP-REF-01" and material["source"] == "kairos-builtin",
            "material", "only PP-REF-01 / kairos-builtin fixture reference is supported")
    require(data["stage"] == "fill-pack-cool", "stage", "expected fill-pack-cool")
    require(data["expectedUnits"] == {"pressure": "MPa", "temperature": "C", "time": "s"},
            "expectedUnits", "expected MPa, C, s")
    process = data["process"]
    fields(process, ("meltTemperatureC", "moldTemperatureC", "injectionTimeS",
                     "packingPressureCurveMpa", "coolingTimeS"), "process")
    for key in ("meltTemperatureC", "moldTemperatureC", "injectionTimeS", "coolingTimeS"):
        number(process[key], "process." + key)
    for key in ("meltTemperatureC", "moldTemperatureC"):
        require(process[key] > -273.15, "process." + key, "must exceed absolute zero")
    require(process["injectionTimeS"] > 0, "process.injectionTimeS", "must be positive")
    require(process["coolingTimeS"] >= 0, "process.coolingTimeS", "must be nonnegative")
    curve = process["packingPressureCurveMpa"]
    require(isinstance(curve, list) and len(curve) >= 2,
            "process.packingPressureCurveMpa", "at least two knots required")
    previous = -1.0
    for i, knot in enumerate(curve):
        loc = f"process.packingPressureCurveMpa[{i}]"
        require(isinstance(knot, list) and len(knot) == 2, loc, "expected [time, pressure]")
        for value in knot:
            number(value, loc)
        time, pressure = knot
        require((i != 0 or time == 0) and time > previous,
                loc, "times must start at zero and strictly increase")
        require(pressure >= 0, loc, "pressure must be nonnegative")
        previous = time
    return data, mesh_data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--experiment", action="store_true",
                        help="validate experiment manifest and its mesh fixture")
    args = parser.parse_args()
    try:
        if args.experiment:
            manifest, data = read_experiment(args.input)
        else:
            manifest, data = None, read(args.input)
        canonical = json.dumps(data, sort_keys=True, separators=(",", ":"), allow_nan=False)
        print(json.dumps({
            "status": "input_valid", "solverReady": False,
            "schemaVersion": data["schemaVersion"], "lengthUnit": data["lengthUnit"],
            "thicknessUnit": data["thicknessUnit"],
            "nodes": len(data["nodes"]), "triangles": len(data["triangles"]),
            "beams": len(data["beams"]), "couplings": len(data["couplings"]),
            "thicknessMin": min(data["thickness"]),
            "thicknessMax": max(data["thickness"]),
            "sourceDataDigest": hashlib.sha256(canonical.encode()).hexdigest(),
            "digestAlgorithm": "sha256-python-json-sort-keys-v1",
            "experimentSchemaVersion": manifest["schemaVersion"] if manifest else None,
            "materialResolved": False,
        }, allow_nan=False))
        return 0
    except (InputError, OSError, ValueError, RecursionError) as error:
        print(f"{args.input}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
