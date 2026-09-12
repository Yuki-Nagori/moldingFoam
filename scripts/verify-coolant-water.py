#!/usr/bin/env python3
#******************************************************************************
# verify-coolant-water.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the 3D coolant-flow module (task 036,
# route A): a laminar water channel heated through its walls must
# conserve energy exactly at steady state - the net boundary heat equals
# the enthalpy rise of the flow, mdot cp (T_bulk - Tin) - and leave the
# outlet warmer than the inlet but colder than the wall.
#
# Usage: verify-coolant-water.py <caseDir>   (exits non-zero on failure)
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

    samples = [
        (float(a), float(b), float(c))
        for a, b, c in re.findall(
            r"moldingCoolantFluid: net boundary heat = ([-+0-9.eE]+) W, "
            r"outlet mdot = ([-+0-9.eE]+) kg/s, outlet bulk T = "
            r"([-+0-9.eE]+) K",
            log,
        )
    ]
    if not samples:
        print("FAIL: no coolant channel samples in the log")
        sys.exit(1)

    q, mdot, Tbulk = samples[-1]
    cp = 4182.0
    Tin = 300.0
    Twall = 350.0

    enthalpy = mdot*cp*(Tbulk - Tin)
    err = abs(q - enthalpy)/max(abs(q), 1e-12)

    print("  net boundary heat = {:.4f} W".format(q))
    print("  outlet: mdot = {:.6e} kg/s, bulk T = {:.4f} K, enthalpy rise "
          "= {:.4f} W".format(mdot, Tbulk, enthalpy))
    print("  energy balance relative error = {:.3e}".format(err))

    fail = False
    if err > 1e-2:
        print("FAIL: the steady-state energy balance is not closed")
        fail = True
    if not (Tin + 1.0 < Tbulk < Twall - 1.0):
        print("FAIL: the outlet bulk temperature left the physical range")
        fail = True
    if mdot < 1e-5:
        print("FAIL: the outlet mass flow is too small")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the water channel conserves energy and heats the coolant")


if __name__ == "__main__":
    main()
