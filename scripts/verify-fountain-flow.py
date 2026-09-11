#!/usr/bin/env python3
#******************************************************************************
# verify-fountain-flow.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the fountain-flow channel: the melt front
# position must follow the injected volume (Q t/(h w) within 5%) and the
# developed velocity profile well behind the front must match the
# analytic Newtonian slit profile u = 1.5 u_mean (1 - (2y/h - 1)^2)
# within 10% (L2).
#
# Usage: verify-fountain-flow.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import math
import os
import re
import sys

H = 0.004              # channel height [m]
W = 0.002              # channel thickness [m]
Q = 1e-07              # inlet flow rate [m^3/s]
UMEAN = Q/(H*W)        # mean velocity [m/s]
NCX = 80
NCY = 8


def read_field(path, kind):
    with open(path) as handle:
        text = handle.read()
    m = re.search(
        r"internalField\s+nonuniform\s+List<" + kind + r">\s*\n?\s*"
        r"(\d+)\s*\((.*?)\)\s*;", text, re.S)
    if not m:
        return None
    return m.group(2)


def scalars(body):
    return [float(x) for x in re.findall(r"[-+0-9.eE]+", body)]


def vectors(body):
    return [
        [float(x) for x in row]
        for row in re.findall(
            r"\(([-+0-9.eE]+) ([-+0-9.eE]+) ([-+0-9.eE]+)\)", body)
    ]


def cell_index(i, j):
    """Block-aware cell index for the 38+4+38 x 8 mesh."""
    if i < 38:
        return i + 38*j
    if i < 42:
        return 304 + (i - 38) + 4*j
    return 336 + (i - 42) + 38*j


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    times = sorted(
        d for d in os.listdir(case_dir)
        if re.match(r"^[0-9.]+$", d) and float(d) > 0
        and os.path.isfile(os.path.join(case_dir, d, "alpha.melt")))
    if not times:
        print("FAIL: no written fields")
        sys.exit(1)

    # Front position from the melt volume over the whole run
    maxFrontErr = 0.0
    for tname in times:
        alpha = scalars(read_field(
            os.path.join(case_dir, tname, "alpha.melt"), "scalar"))
        if alpha is None or len(alpha) != NCX*NCY:
            continue
        # volume-equivalent front position (melt volume / cross-section)
        vMelt = sum(alpha)*(0.0005*0.0005*0.002)
        xFront = vMelt/(H*W)
        xAna = Q*float(tname)/(H*W)
        if xAna > 0:
            maxFrontErr = max(maxFrontErr, abs(xFront - xAna)/xAna)

    print("  front position vs Q t/(h w): max relative error = "
          "{:.3%}".format(maxFrontErr))

    # Developed velocity profile at x = 5.25 mm (cell i = 10), one second
    # in (the front is at about 12.5 mm then)
    tProf = min(times, key=lambda d: abs(float(d) - 1.0))
    body = read_field(os.path.join(case_dir, tProf, "U"), "vector")
    if body is None:
        print("FAIL: cannot read the velocity field")
        sys.exit(1)
    U = vectors(body)
    if len(U) != NCX*NCY:
        print("FAIL: unexpected velocity field size")
        sys.exit(1)

    # L2 (RMS) error of the developed profile against the analytic
    # Poiseuille parabola: the cell values represent cell averages, so the
    # RMS norm avoids over-weighting the steep near-wall cells
    sumSq = 0.0
    for j in range(NCY):
        idx = cell_index(10, j)
        y = (j + 0.5)*0.0005
        uAna = 1.5*UMEAN*(1 - (2*y/H - 1)**2)
        sumSq += (U[idx][0] - uAna)**2

    profErr = math.sqrt(sumSq/NCY)/UMEAN

    print("  developed profile (x = 5.25 mm, t = {} s) vs analytic "
          "Poiseuille: L2 error = {:.3%} u_mean".format(tProf, profErr))

    fail = False
    if maxFrontErr > 0.05:
        print("FAIL: the front position does not follow the injected volume")
        fail = True
    if profErr > 0.10:
        print("FAIL: the developed velocity profile deviates from the "
              "analytic Poiseuille profile")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the front position and the developed velocity profile "
          "match the analytic reference")


if __name__ == "__main__":
    main()
