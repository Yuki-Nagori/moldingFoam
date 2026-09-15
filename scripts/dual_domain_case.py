#!/usr/bin/env python3
"""Emit explicit OpenFOAM Dual Domain dictionaries from a validated JSON input."""
# SPDX-License-Identifier: GPL-3.0-or-later
import argparse
from pathlib import Path
import sys

from dual_domain_input import InputError, read


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


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    try:
        output = args.output
        if output.name != "dualDomainMesh":
            raise InputError("output: filename must be dualDomainMesh")
        data = read(args.input)
        output.parent.mkdir(parents=True, exist_ok=True)
        output.write_text(emit(data), encoding="utf-8")
        print(f"wrote {output} (nodes={len(data['nodes'])}, triangles={len(data['triangles'])})")
        return 0
    except (InputError, OSError, ValueError) as error:
        print(f"{args.input}: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
