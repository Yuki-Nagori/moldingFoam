#!/usr/bin/env python3
#******************************************************************************
# verify-shrink-bar.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the free anisotropic-shrinkage bar (task
# 034): a bar with a slip (roller) plane at x = 0 carrying a uniform free
# shrinkage eps must displace as u_x = eps x (the normal is fixed by the
# roller, the tangential is free) and contract laterally as
# u_y = eps (y - h/2) about the centreline, stress-free away from the
# constraint.
#
# Usage: verify-shrink-bar.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys


def read_vector_field(path):
    with open(path, errors="replace") as f:
        txt = f.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<vector>\s*\n\s*\d+\s*\n\((.*?)\)\s*;",
        txt,
        re.S,
    )
    if m is None:
        return None

    return [
        [float(x) for x in g.split()]
        for g in re.findall(r"\(([^()]*)\)", m.group(1))
    ]


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

    times = time_dirs(case_dir)
    if not times:
        print("FAIL: no written time directories")
        sys.exit(1)

    D = read_vector_field(os.path.join(case_dir, times[-1], "D"))
    if D is None or len(D) != 48*16:
        print("FAIL: cannot read the displacement field")
        sys.exit(1)

    eps = 0.01
    L = 30.0
    h = 1.0

    # Free end neutral axis (mean over the layers)
    ux = sum(D[47 + 48*j][0] for j in range(16))/16
    uyCentre = 0.5*(D[47 + 48*7][1] + D[47 + 48*8][1])
    uyTop = D[47 + 48*15][1]

    print("  free end u_x = {:.6e} m (expected {:.6e})".format(ux, eps*L))
    print("  u_y(top) - u_y(centre) = {:.6e} m (expected {:.6e})".format(
        uyTop - uyCentre, eps*0.5))

    fail = False
    if abs(ux - eps*L)/(eps*L) > 0.10:
        print("FAIL: the axial free strain deviates by more than 10%")
        fail = True
    if abs((uyTop - uyCentre) - eps*0.5)/(eps*0.5) > 0.15:
        print("FAIL: the lateral free strain deviates by more than 15%")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the uniform free shrinkage strains the bar freely about "
          "the roller")


if __name__ == "__main__":
    main()
