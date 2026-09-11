#!/usr/bin/env python3
#******************************************************************************
# verify-fiber-orientation-advection.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the orientation-tensor advection: the cavity
# starts with a fully aligned orientation (a = xx) and fresh isotropic
# melt (a = I/3) enters through the gate, so the final mean a_xx must
# approach 1/3 while the trace stays 1.
#
# Usage: verify-fiber-orientation-advection.py <caseDir>
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
             and os.path.isfile(os.path.join(case_dir, d, "a"))]
    if not times:
        print("FAIL: no written orientation field")
        sys.exit(1)

    t_last = max(times, key=float)
    a = read_internal(os.path.join(case_dir, t_last, "a"))
    if a is None:
        print("FAIL: cannot read the orientation field")
        sys.exit(1)

    n = len(a)
    meanxx = sum(row[0] for row in a)/n
    maxtr = max(abs(row[0] + row[3] + row[5] - 1) for row in a)

    print("  final mean(a_xx) = {:.4f}, max|tr(a)-1| = {:.3e}".format(
        meanxx, maxtr))

    fail = False
    if meanxx > 0.5:
        print("FAIL: the fresh isotropic melt did not flush the aligned "
              "orientation")
        fail = True
    if maxtr > 1e-6:
        print("FAIL: the orientation trace drifted")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the orientation tensor is advected with the melt and "
          "keeps tr(a) = 1")


if __name__ == "__main__":
    main()
