#!/usr/bin/env python3
#******************************************************************************
# verify-mass-fix.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the global mass fixer (tasks 043/018/031):
# with massFixGlobal enabled the discrete continuity residual of the sealed
# cavity is folded back into the phase-1 density, so the logged mass budget
# closes at machine level. Ablation calibration (voidFraction geometry):
#
#   massFixGlobal true  -> residual -8.2e-15 kg
#   no fixer            -> residual -2.5e-08 kg
#
# The threshold sits between the two with ~3 orders of magnitude of margin
# on each side.
#
# Usage: verify-mass-fix.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

RESIDUAL_TOL = 1e-12


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    with open(os.path.join(case_dir, "constant", "moldingDict"),
              errors="replace") as f:
        dict_txt = f.read()

    if not re.search(r"^\s*massFixGlobal\s+true\s*;", dict_txt, re.M):
        print("FAIL: the case must enable massFixGlobal")
        sys.exit(1)

    m = [float(x) for x in re.findall(
        r"mass budget: m = ([-+0-9.eE]+) kg", log)]
    res = [float(x) for x in re.findall(
        r"residual = ([-+0-9.eE]+) kg", log)]

    if not m or not res:
        print("FAIL: no mass-budget samples in the log "
              "(massBudget must be enabled)")
        sys.exit(1)

    rel = abs(res[-1])/m[-1]
    print("  sealed mass = {:.6e} kg, final budget residual = {:.3e} kg "
          "(relative {:.2e})".format(m[-1], res[-1], rel))

    if abs(res[-1]) > RESIDUAL_TOL:
        print("FAIL: the global mass fixer did not close the budget "
              "(residual above {:.0e} kg)".format(RESIDUAL_TOL))
        sys.exit(1)

    print("PASS: the global mass fixer closes the sealed mass budget at "
          "machine level")


if __name__ == "__main__":
    main()
