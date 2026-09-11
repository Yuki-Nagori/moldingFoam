#!/usr/bin/env python3
#******************************************************************************
# verify-warpage-plate.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the shrinkage-driven warpage plate (task
# 013b): the plate carries a two-layer free-shrinkage profile
# (eps_bottom = 1e-3, eps_top = 2e-3) imposed as a frozen equivalent
# temperature field (alphav = 1). For two bonded layers of equal
# thickness the Timoshenko bimetal curvature reduces to
#
#     kappa = 3 (eps_top - eps_bottom) / (2 h)
#
# so the free end of the cantilever must deflect by kappa L^2 / 2.
#
# Usage: verify-warpage-plate.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import math
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


def read_sigma_xx(path):
    with open(path, errors="replace") as f:
        txt = f.read()

    m = re.search(
        r"internalField\s+nonuniform\s+List<symmTensor>\s*\n\s*\d+\s*\n\((.*?)\)\s*;",
        txt,
        re.S,
    )
    if m is None:
        return None

    return [
        float(g.split()[0]) for g in re.findall(r"\(([^()]*)\)", m.group(1))
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

    # Free end: the i = 47 column, neutral axis = mean over 16 layers
    dy = sum(D[47 + 48*j][1] for j in range(16))/16

    # Timoshenko bimetal, equal layers: kappa = 3 (eps_top - eps_bottom)
    # / (2 h); L = 30, h = 1
    L = 30.0
    h = 1.0
    dEps = 1e-3
    kappa = 3*dEps/(2*h)
    expected = kappa*L*L/2

    print("  bimetal curvature = {:.6e} 1/m".format(kappa))
    print("  free-end neutral-axis deflection = {:.6e} m "
          "(expected {:.6e} m)".format(dy, expected))

    # The mesh places the larger-shrinkage layers on the topSurface side,
    # which bends the cantilever towards -y (the same convention as the
    # thermoelastic case); only the magnitude is asserted
    fail = False
    if abs(abs(dy) - expected)/expected > 0.10:
        print("FAIL: the free-end deflection deviates from kappa L^2/2 by "
              "more than 10%")
        fail = True

    # Residual stress self-equilibrium: the two layers carry opposite
    # through-thickness stresses whose integral vanishes (no external
    # axial load on the free plate)
    sigma = read_sigma_xx(os.path.join(case_dir, times[-1], "sigma"))
    if sigma is None:
        print("FAIL: cannot read the residual stress field")
        sys.exit(1)

    # Mean over the free-end column for each layer
    def layer_mean(j):
        return sum(sigma[47 + 48*j] for _ in [0])/1.0

    sBot = sum(sigma[47 + 48*j] for j in range(0, 8))/8
    sTop = sum(sigma[47 + 48*j] for j in range(8, 16))/8
    sInt = sum(sigma[47 + 48*j] for j in range(16))/16

    print("  residual stress: bottom layer = {:.6e} Pa, top layer = "
          "{:.6e} Pa, neutral-axis mean = {:.3e} Pa".format(
              sBot, sTop, sInt))

    if sBot*sTop >= 0:
        print("FAIL: the layer residual stresses do not oppose")
        fail = True
    if abs(sInt) > 0.05*max(abs(sBot), abs(sTop)):
        print("FAIL: the residual stress is not self-equilibrated across "
              "the thickness")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the mapped free shrinkage bends the plate by the "
          "Timoshenko bimetal curvature with self-equilibrated residual "
          "stress")


if __name__ == "__main__":
    main()
