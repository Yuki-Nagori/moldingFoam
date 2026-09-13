#!/usr/bin/env python3
#******************************************************************************
# verify-nu-correlation.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the Nusselt-correlation coolant branch of
# moldingMoldTemperature (task 044, gap G9). The case configures
#
#   Nu { C 8.333333333; m 1; n 1; Re 1; Pr 1; k 0.6; D 1e-3; }
#
# so that Nu*k/D = 5000 W/m^2/K exactly - the same effective htc as the
# sibling coolantChannel case, which makes this a self-consistency check
# with no free parameters: the mould temperature history must match the
# constant-htc baseline. Calibration:
#
#   constant htc 5000   -> T_mould end = 306.87675 K
#   Nu equivalent       -> T_mould end = 306.87675 K (identical)
#   ablation (C x2)     -> T_mould end = 312.02658 K (+5.15 K)
#
# The tolerance is 0.05 K, i.e. ~100x smaller than the ablation shift.
#
# Usage: verify-nu-correlation.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

T_REF = 306.87675
T_TOL = 0.05


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    if not os.path.isfile(os.path.join(case_dir, "0", "T")):
        print("FAIL: missing 0/T")
        sys.exit(1)

    with open(os.path.join(case_dir, "0", "T"), errors="replace") as f:
        bc = f.read()
    if "Nu" not in bc:
        print("FAIL: the case must configure the Nu correlation")
        sys.exit(1)

    series = [float(x) for x in re.findall(
        r"areaAverage\(coldWall\) of T = ([-+0-9.eE]+)", log)]
    if len(series) < 2:
        print("FAIL: no coldWall temperature history in the log")
        sys.exit(1)

    print("  coldWall T: start = {:.5f} K, end = {:.5f} K (Nu-correlated; "
          "reference {:.5f} +- {:g} K)".format(
              series[0], series[-1], T_REF, T_TOL))

    if series[0] > 301.0:
        print("FAIL: the mould did not start at its initial temperature")
        sys.exit(1)
    if series[-1] < series[0]:
        print("FAIL: the coolant channel did not heat the mould")
        sys.exit(1)
    if abs(series[-1] - T_REF) > T_TOL:
        print("FAIL: the Nu correlation does not reproduce the equivalent "
              "constant htc (the branch is misconfigured or ineffective)")
        sys.exit(1)

    print("PASS: the Nu correlation reproduces the equivalent constant-htc "
          "mould response")


if __name__ == "__main__":
    main()
