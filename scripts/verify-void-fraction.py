#!/usr/bin/env python3
#******************************************************************************
# verify-void-fraction.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the sealed-melt void indicator (task 018a):
# a cavity sealed at t ~ 0 and cooled to the mould temperature must
# develop a positive, near-uniform and bounded voidFraction, and the
# final written field must agree with the logged maximum. The case also
# guards the sealed cooling against the rigid (acoustic) blow-up of the
# near-incompressible melt: the void fraction and the temperature must
# stay stable over the whole run.
#
# Usage: verify-void-fraction.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys


def read_scalar_field(path):
    with open(path, errors="replace") as f:
        txt = f.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n?\s*(\d+)\s*\((.*?)\)\s*;",
        txt,
        re.S,
    )
    if m is not None:
        return [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]

    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
    if m is not None:
        return [float(m.group(1))]

    return None


def time_dirs(case_dir):
    return sorted(
        (d for d in os.listdir(case_dir)
         if re.match(r"^[0-9]+(\.[0-9]+)?$", d) and float(d) > 0),
        key=float,
    )


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    m = re.search(r"gate sealed at t = ([-+0-9.eE]+) s", log)
    if m is None:
        print("FAIL: the gate never sealed; the melt is not a fixed mass")
        sys.exit(1)

    seal_time = float(m.group(1))
    if seal_time > 1e-3:
        print("FAIL: the gate did not seal at the start of the run "
              "(t = {:.3e} s)".format(seal_time))
        sys.exit(1)

    V = [float(x) for x in re.findall(r"void fraction: max = ([-+0-9.eE]+)", log)]
    if len(V) < 2:
        print("FAIL: not enough void-fraction samples in the log")
        sys.exit(1)

    print("  sealed at t = {:.3e} s".format(seal_time))
    print("  max(voidFraction): first = {:.6f}, last = {:.6f}".format(
        V[0], V[-1]))

    fail = False
    if V[-1] < 0.01:
        print("FAIL: no voids formed after the gate seal")
        fail = True
    if min(V) < 0 or max(V) >= 1:
        print("FAIL: the void fraction left the physical range [0, 1)")
        fail = True
    if max(V) - min(V) > 0.05:
        print("FAIL: the void fraction drifted; the sealed cooling is unstable")
        fail = True

    times = time_dirs(case_dir)
    if not times:
        print("FAIL: no written time directories")
        sys.exit(1)

    field_path = os.path.join(case_dir, times[-1], "voidFraction")
    if not os.path.exists(field_path):
        print("FAIL: no written voidFraction field")
        sys.exit(1)

    vals = read_scalar_field(field_path)
    if not vals:
        print("FAIL: cannot read the voidFraction field")
        sys.exit(1)

    fmax = max(vals)
    fmin = min(vals)
    print("  written field: max = {:.6f}, min = {:.6f}".format(fmax, fmin))

    if fmin < 0 or fmax >= 1:
        print("FAIL: the written field left the physical range")
        fail = True
    if fmin < 0.5*fmax:
        print("FAIL: the void fraction is not near-uniform over the part")
        fail = True
    if abs(fmax - max(V)) > 0.02 + 0.1*abs(max(V)):
        print("FAIL: the written field disagrees with the log")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the sealed melt develops a stable, near-uniform void fraction")


if __name__ == "__main__":
    main()
