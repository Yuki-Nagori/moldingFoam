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
import sys

ALPHA = 1.1e-5
DT = 20.0
H = 1.0
L = 30.0
KAPPA = ALPHA*DT/H


from validation_metrics import mesh_dimensions, field, completed_time, measure, section_average


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."
    t_last = completed_time(case_dir)
    nx, ny = mesh_dimensions(case_dir)
    D = field(os.path.join(case_dir, t_last, "D"), "vector", 3, nx*ny)

    # Sample one fixed physical section at every mesh level.
    xSample = 29.5
    dyEnd = section_average(D, nx, ny, xSample)[1]

    # Compare at the sampled section centre.
    dAna = KAPPA*xSample*xSample/2

    err = abs(abs(dyEnd) - dAna)/dAna

    print("  free-end neutral-axis deflection = {:.6e} m "
          "(expected {:.6e} m, error {:.3%})".format(
              dyEnd, dAna, err))

    accepted = measure("free-end-deflection-m", dyEnd, dAna, err, 0.08, t_last)
    if not accepted:
        raise ValueError("thermoelastic deflection error exceeds 8%")
    if os.environ.get("UNCERTAINTY_MATRIX_MODE") != "1":
        print("PASS: thermoelastic deflection error <= 8%")


if __name__ == "__main__":
    try:
        main()
    except (ValueError, OSError) as exc:
        print("FAIL:", exc)
        sys.exit(1)
