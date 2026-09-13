#!/usr/bin/env python3
#******************************************************************************
# verify-eigenstrain-graded.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the graded-eigenstrain domain source (task 040).
#
# The bar (30 x 1 x 1, free transverse slip, traction-free ends) carries a
# linearly graded in-plane eigenstrain
#
#     eps_xx(x) = 0.02 (1 + 0.5 x/30),   x in [0, 30]
#
# so the one-dimensional free-expansion solution is
#
#     u_x(x) = 0.02 (x + x^2/120)   ->   u_x(30) = 0.750
#
# Uniform eps*: the domain source vanishes identically (div = 0) and the
# face tractions alone reproduce the analytic 0.600 (task 040 §9b measured
# 0.59375, -1.04% - the case's own discretisation bias).
#
# Usage: verify-eigenstrain-graded.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

U_TIP = 0.750
U_TOL = 0.015


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    log = ""
    for name in ("log.run", "log.foamRun"):
        p = os.path.join(case_dir, name)
        if os.path.isfile(p):
            with open(p, errors="replace") as f:
                log = f.read()
            break

    if not log:
        print("FAIL: no solver log found")
        sys.exit(1)
    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    if not os.path.isfile(os.path.join(case_dir, "constant", "fvModels")):
        print("FAIL: missing constant/fvModels (the case must enable moldingEigenstrain)")
        sys.exit(1)

    with open(os.path.join(case_dir, "constant", "fvModels"),
              errors="replace") as f:
        fvm = f.read()
    if "moldingEigenstrain" not in fvm:
        print("FAIL: the case must enable the moldingEigenstrain model")
        sys.exit(1)

    with open(os.path.join(case_dir, "0", "eigenstrain"),
              errors="replace") as f:
        eps = f.read()
    if "nonuniform" not in eps:
        print("FAIL: the eigenstrain must be graded (non-uniform)")
        sys.exit(1)

    times = sorted((d for d in os.listdir(case_dir)
                    if re.match(r"^[0-9]+(\.[0-9]+)?$", d)
                    and float(d) > 0), key=float)
    if not times:
        print("FAIL: no written time directories")
        sys.exit(1)

    with open(os.path.join(case_dir, times[-1], "D"), errors="replace") as f:
        txt = f.read()
    m = re.search(
        r"internalField\s+nonuniform\s+List<vector>\s*\n\s*\d+\s*\n"
        r"\((.*?)\)\s*;", txt, re.S)
    if m is None:
        print("FAIL: cannot read the displacement field")
        sys.exit(1)

    ux = max(float(a) for a, b, c in re.findall(
        r"\(([-+0-9.eE]+) ([-+0-9.eE]+) ([-+0-9.eE]+)\)", m.group(1)))

    dev = abs(ux - U_TIP)/U_TIP
    print("  tip u_x = {:.6f} (1D analytic {:.6f}, deviation {:.3%})"
          .format(ux, U_TIP, dev))

    if dev > U_TOL:
        print("FAIL: the graded eigenstrain response does not match the "
              "one-dimensional free-expansion solution")
        sys.exit(1)

    print("PASS: the domain eigenstrain source reproduces the graded "
          "free-expansion solution")


if __name__ == "__main__":
    main()
