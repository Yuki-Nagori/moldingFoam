#!/usr/bin/env python3
#******************************************************************************
# verify-warpage-aniso.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the anisotropic-shrinkage warpage plate
# (task 034, stage 3b): the two layers carry different degrees of fibre
# alignment (p = 1 top, p = 1/3 bottom), hence different anisotropic
# free-shrinkage means. The mapped in-plane-mean eigenstrain difference
# drives the Timoshenko bimetal curvature
#
#     kappa = 3 dEps / (2 h),  dEps = e0 beta (p_top - p_bot) / 4
#     e0 = S/3,  S = 0.036, beta = 0.5
#
# so the free cantilever end must deflect by kappa L^2 / 2.
#
# Usage: verify-warpage-aniso.py <caseDir>   (exits non-zero on failure)
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

    dy = sum(D[47 + 48*j][1] for j in range(16))/16

    # Anisotropic in-plane-mean eigenstrain difference
    S = 0.036
    beta = 0.5
    e0 = S/3
    dEps = e0*beta*(1.0 - 1.0/3)/4.0
    kappa = 3*dEps/2.0
    expected = kappa*30.0*30.0/2.0

    print("  anisotropic dEps = {:.6e}, kappa = {:.6e} 1/m".format(
        dEps, kappa))
    print("  free-end deflection = {:.6e} m (expected {:.6e} m)".format(
        dy, expected))

    fail = False
    if abs(abs(dy) - expected)/expected > 0.10:
        print("FAIL: the deflection deviates from the anisotropic "
              "Timoshenko curvature by more than 10%")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the anisotropic shrinkage difference bends the plate by "
          "the Timoshenko curvature")


if __name__ == "__main__":
    main()
