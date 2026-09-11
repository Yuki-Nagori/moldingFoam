#!/usr/bin/env python3
#******************************************************************************
# verify-viscoelastic-flow.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the viscoelastic momentum coupling: in a
# shear channel the extra stress tau must develop a finite shear component
# and the run must stay stable.
#
# Usage: verify-viscoelastic-flow.py <caseDir>
#******************************************************************************

import os
import re
import sys


def read_internal(path):
    with open(path) as handle:
        text = handle.read()
    m = re.search(r"internalField\s+nonuniform\s+List<symmTensor>\s*\n?\s*"
                  r"(\d+)\s*\((.*?)\)\s*;", text, re.S)
    if not m:
        return None
    vals = re.findall(
        r"\(([-+0-9.eE]+) ([-+0-9.eE]+) ([-+0-9.eE]+) "
        r"([-+0-9.eE]+) ([-+0-9.eE]+) ([-+0-9.eE]+)\)", m.group(2))
    return [[float(v) for v in row] for row in vals]


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    times = [d for d in os.listdir(case_dir)
             if re.match(r"^[0-9.]+$", d)
             and os.path.isfile(os.path.join(case_dir, d, "tau"))]
    if not times:
        print("FAIL: no written extra stress field")
        sys.exit(1)

    t_last = max(times, key=float)
    tau = read_internal(os.path.join(case_dir, t_last, "tau"))
    if tau is None:
        print("FAIL: cannot read the extra stress field")
        sys.exit(1)

    maxtxy = max(abs(row[1]) for row in tau)
    maxtr = max(abs(row[0] + row[3] + row[5]) for row in tau)

    print("  max|tau_xy| = {:.4e} Pa at t = {} (max|tr(tau)| = {:.3e} Pa)"
          .format(maxtxy, t_last, maxtr))

    # UCM steady shear stress with eta0*gammadot = 100*1000 = 1e5 Pa; the
    # solvent also carries stress, so only a lower bound is checked
    if maxtxy < 1e3:
        print("FAIL: the extra stress did not develop")
        sys.exit(1)

    if not all(v == v and abs(v) < 1e12 for row in tau for v in row):
        print("FAIL: the extra stress is not finite")
        sys.exit(1)

    print("PASS: the viscoelastic extra stress develops in the shear flow "
          "and the run is stable")


if __name__ == "__main__":
    main()
