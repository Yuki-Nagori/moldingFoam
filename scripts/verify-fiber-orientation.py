#!/usr/bin/env python3
#******************************************************************************
# verify-fiber-orientation.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the fibre-orientation solver case: the
# Folgar-Tucker equation evolves the orientation tensor in a shear flow;
# the off-diagonal component a12 must grow from the isotropic state and
# the trace must stay at 1.
#
# Usage: verify-fiber-orientation.py <caseDir>  (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    a12 = [float(x) for x in re.findall(r"max\|a12\| = ([-+0-9.eE]+)", log)]
    trerr = [
        float(x) for x in re.findall(r"max\|tr\(a\)-1\| = ([-+0-9.eE]+)", log)
    ]
    if len(a12) < 3:
        print("FAIL: not enough orientation samples in the log")
        sys.exit(1)

    print("  max|a12|: first = {:.6f}, last = {:.6f}".format(
        a12[0], a12[-1]))
    print("  max|tr(a)-1| over the run = {:.3e}".format(max(trerr)))

    fail = False
    if a12[-1] < 0.05:
        print("FAIL: the shear did not orient the fibres")
        fail = True
    if max(a12) > 0.5 + 1e-9:
        print("FAIL: |a12| exceeded its physical bound of 0.5")
        fail = True
    if max(trerr) > 1e-6:
        print("FAIL: the trace of the orientation tensor drifted")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the fibres orient under shear with tr(a) = 1 preserved")


if __name__ == "__main__":
    main()
