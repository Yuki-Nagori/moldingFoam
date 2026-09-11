#!/usr/bin/env python3
#******************************************************************************
# verify-crystallization-advection.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the crystallinity advection: the cavity is
# initially fully crystallised (chi = 1) and fresh melt with chi = 0
# enters through the gate, so the final mean crystallinity must approach
# zero as the cavity is flushed. The kinetics are far from their window,
# so the change is pure advection.
#
# Usage: verify-crystallization-advection.py <caseDir>
#******************************************************************************

import os
import re
import sys


def read_internal(path):
    with open(path) as handle:
        text = handle.read()
    m = re.search(r"internalField\s+nonuniform\s+List<scalar>\s*\n?\s*"
                  r"(\d+)\s*\((.*?)\)", text, re.S)
    if not m:
        return None
    return [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(2))]


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    times = [d for d in os.listdir(case_dir)
             if re.match(r"^[0-9.]+$", d)
             and os.path.isfile(os.path.join(case_dir, d, "chi"))]
    if not times:
        print("FAIL: no written chi field")
        sys.exit(1)

    t_last = max(times, key=float)
    chi = read_internal(os.path.join(case_dir, t_last, "chi"))
    alpha = read_internal(os.path.join(case_dir, t_last, "alpha.melt"))
    if chi is None or alpha is None:
        print("FAIL: cannot read the chi or alpha field")
        sys.exit(1)

    # Crystallinity of the melt only: the remaining air keeps its initial
    # chi = 1, so the plain mean would be biased by the air fraction
    meltMass = sum(alpha)
    mean = sum(a*c for a, c in zip(alpha, chi))/max(meltMass, 1e-30)

    print("  final melt-weighted mean(chi) = {:.6f} at t = {} "
          "(air fraction {:.3f})".format(
              mean, t_last, 1 - meltMass/len(alpha)))

    fail = False
    if mean > 0.05:
        print("FAIL: the fresh melt did not flush the crystallised melt")
        fail = True
    if min(chi) < -1e-12 or max(chi) > 1 + 1e-12:
        print("FAIL: chi left the physical range")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the crystallinity is advected with the melt and stays "
          "bounded")


if __name__ == "__main__":
    main()
