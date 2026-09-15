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

import os
import sys


from validation_metrics import mesh_dimensions, field, completed_time, measure, section_average, section_rows


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    t_last = completed_time(case_dir)
    nx, ny = mesh_dimensions(case_dir)
    if ny % 2:
        raise ValueError("equal-layer benchmark requires an even ny")
    D = field(os.path.join(case_dir, t_last, "D"), "vector", 3, nx*ny)

    # Sample one fixed physical section at every mesh level.
    xSample = 29.5
    dy = section_average(D, nx, ny, xSample)[1]

    # Timoshenko bimetal, equal layers: kappa = 3 (eps_top - eps_bottom)
    # / (2 h); L = 30, h = 1
    L = 30.0
    h = 1.0
    dEps = 1e-3
    kappa = 3*dEps/(2*h)
    expected = kappa*xSample*xSample/2

    print("  bimetal curvature = {:.6e} 1/m".format(kappa))
    print("  free-end neutral-axis deflection = {:.6e} m "
          "(expected {:.6e} m)".format(dy, expected))

    # The mesh places the larger-shrinkage layers on the topSurface side,
    # which bends the cantilever towards -y (the same convention as the
    # thermoelastic case); only the magnitude is asserted
    err = abs(abs(dy) - expected)/expected
    fail = not measure("free-end-deflection-m", dy, expected, err, 0.08, t_last)
    if fail:
        print("FAIL: deflection error exceeds 8%")

    sigma = [row[0] for row in field(os.path.join(case_dir, t_last, "sigma"),
                                    "symmTensor", 6, nx*ny)]
    column = section_rows([[value] for value in sigma], nx, ny, xSample)
    column = [row[0] for row in column]
    sBot = sum(column[:ny//2])/(ny//2)
    sTop = sum(column[ny//2:])/(ny//2)
    sInt = sum(column)/ny

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

    if os.environ.get("UNCERTAINTY_MATRIX_MODE") == "1":
        print("VALID: residual stress opposes and is self-equilibrated")
    else:
        print("PASS: warpage deflection and residual stress")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as exc:
        print("FAIL:", exc)
        sys.exit(1)
