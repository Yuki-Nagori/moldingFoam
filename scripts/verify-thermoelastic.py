#!/usr/bin/env python3
#******************************************************************************
# verify-thermoelastic.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the thermoelastic cantilever benchmark
# (solidDisplacement): a linear through-thickness temperature profile
# bends a free cantilever with the classical curvature
# kappa = alpha dT/h (alpha the linear coefficient), so the free-end
# neutral-axis deflection is kappa x^2/2 at the sampled section.
#
# Usage: verify-thermoelastic.py <caseDir>
#******************************************************************************

import os
import re
import sys

ALPHA = 1.1e-5
DT = 20.0
H = 1.0
L = 30.0
KAPPA = ALPHA*DT/H


def read_D(path):
    with open(path) as handle:
        text = handle.read()
    m = re.search(r"internalField\s+nonuniform\s+List<vector>\s*\n?\s*(\d+)"
                  r"\s*\((.*?)\)\s*;", text, re.S)
    if not m:
        return None
    return [[float(v) for v in row] for row in re.findall(
        r"\(([-+0-9.eE]+) ([-+0-9.eE]+) ([-+0-9.eE]+)\)", m.group(2))]


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    times = [d for d in os.listdir(case_dir)
             if re.match(r"^[0-9.]+$", d) and float(d) > 0
             and os.path.isfile(os.path.join(case_dir, d, "D"))]
    if not times:
        print("FAIL: no written displacement field")
        sys.exit(1)

    t_last = max(times, key=float)
    D = read_D(os.path.join(case_dir, t_last, "D"))
    if D is None or len(D) != 48*80:
        print("FAIL: cannot read the displacement field")
        sys.exit(1)

    # The free end is the i=47 column of the 48x80x1 mesh; the neutral
    # axis deflection is the mean over the 80 through-thickness cells
    dyEnd = sum(D[47 + 48*j][1] for j in range(80))/80

    # Section centre x = (47 + 0.5)*L/48
    xEnd = 47.5*L/48
    dAna = KAPPA*xEnd*xEnd/2

    err = abs(abs(dyEnd) - dAna)/dAna

    print("  free-end neutral-axis deflection = {:.6e} m "
          "(expected {:.6e} m, error {:.3%})".format(
              dyEnd, dAna, err))

    if err > 0.08:
        print("FAIL: the thermoelastic cantilever does not bend with "
              "kappa = alpha dT/h")
        sys.exit(1)

    print("PASS: the thermoelastic cantilever bends with kappa = "
          "alpha dT/h")


if __name__ == "__main__":
    main()
