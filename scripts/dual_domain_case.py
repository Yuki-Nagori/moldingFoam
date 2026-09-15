#!/usr/bin/env python3
"""Emit explicit OpenFOAM Dual Domain dictionaries from a validated JSON input."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
from pathlib import Path
import sys

from dual_domain_input import InputError, read, read_experiment


def vec(values):
    return "(" + " ".join(f"{float(v) * 1e-3:.17g}" for v in values) + ")"


def emit(data):
    nodes = "\n".join(f"    {vec(node)}" for node in data["nodes"])
    triangles = "\n".join("    (" + " ".join(map(str, tri)) + ")"
                          for tri in data["triangles"])
    thickness = "\n".join(f"    {float(h) * 1e-3:.17g}" for h in data["thickness"])
    beams = "\n".join(
        f"    ({b['nodes'][0]} {b['nodes'][1]} {float(b['diameter']) * 1e-3:.17g} {b['kind']})"
        for b in data["beams"])
    couplings = "\n".join(
        f"    ({c['beam']} {c['endpoint']} {c['node']} {float(c['distance']) * 1e-3:.17g})"
        for c in data["couplings"])
    return f'''FoamFile
{{
    format ascii;
    class dictionary;
    location "constant";
    object dualDomainMesh;
}}

schemaVersion dual-domain/v1;
sourceUnits mm;
targetUnits m;
nodes
(
{nodes}
);
triangles
(
{triangles}
);
thickness
(
{thickness}
);
beams
(
{beams}
);
couplings
(
{couplings}
);
'''


def emit_properties(manifest):
    process = manifest["process"]
    knots = "\n".join(
        f"        ({float(t):.17g} {float(p) * 1e6:.17g})"
        for t, p in process["packingPressureCurveMpa"])
    return f'''FoamFile
{{
    format ascii;
    class dictionary;
    location "constant";
    object dualDomainProperties;
}}

schemaVersion dual-domain-experiment/v1;
stage {manifest["stage"]};
materialId {manifest["material"]["id"]};
sourceUnits "C MPa s";
targetUnits "K Pa s";
meltTemperature {float(process["meltTemperatureC"]) + 273.15:.15g};
cavityTemperature {float(process["moldTemperatureC"]) + 273.15:.15g};
coreTemperature {float(process["moldTemperatureC"]) + 273.15:.15g};
injectionTime {float(process["injectionTimeS"]):.17g};
packingPressure
(
{knots}
);
coolingTime {float(process["coolingTimeS"]):.17g};
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--experiment", action="store_true",
                        help="also emit dualDomainProperties from experiment manifest")
    parser.add_argument("--properties-output", type=Path,
                        help="output path for dualDomainProperties")
    args = parser.parse_args()
    try:
        output = args.output
        if output.name != "dualDomainMesh":
            raise InputError("output: filename must be dualDomainMesh")
        manifest = None
        if args.experiment:
            manifest, data = read_experiment(args.input)
        else:
            data = read(args.input)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(emit(data), encoding="utf-8")
        if args.experiment:
            properties = args.properties_output or output.with_name("dualDomainProperties")
            if properties.name != "dualDomainProperties":
                raise InputError("properties-output: filename must be dualDomainProperties")
            properties.parent.mkdir(parents=True, exist_ok=True)
            properties.write_text(emit_properties(manifest), encoding="utf-8")
        print(f"wrote {output} (nodes={len(data['nodes'])}, triangles={len(data['triangles'])})")
        return 0
    except (InputError, OSError, ValueError) as error:
        print(f"{args.input}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
