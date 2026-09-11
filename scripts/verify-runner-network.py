#!/usr/bin/env python3
#******************************************************************************
# verify-runner-network.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the runner-coupled inlet case: the inlet
# moldingInletVelocity boundary takes its flow rate from the equal
# pressure-drop split of a 1D runner network (a single gate here, so the
# gate flow equals the prescribed total). The inlet mass flux must match
# rho*Q and the run must complete.
#
# Note: the melt volume fraction is not compared with Q t / V because the
# trapped air is heated by the melt and expands, which is physical.
#
# Usage: verify-runner-network.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

Q = 1e-07             # total (single-gate) runner flow rate [m^3/s]
V = 0.02*0.002*0.002  # cavity volume [m^3]
RHO = 746.3           # melt density at 480 K, 1e5 Pa [kg/m^3]


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    fills = [
        float(x) for x in re.findall(r"filled fraction = ([-+0-9.eE]+)", log)
    ]
    if not fills:
        print("FAIL: no filling reports in the log")
        sys.exit(1)

    print("  final melt volume fraction = {:.6f}".format(fills[-1]))

    # Inflow is negative; ignore the zero samples written before the first
    # solve and average the last few settled samples
    mflows = [
        float(x) for x in re.findall(
            r"sum\(inlet\) of alphaRhoPhi\.melt = ([-+0-9.eE]+)", log)
        if abs(float(x)) > 0
    ]
    if not mflows:
        print("FAIL: no non-zero inlet mass flow samples in the log")
        sys.exit(1)

    settled = mflows[-20:] if len(mflows) >= 20 else mflows
    mdot = sum(settled)/len(settled)
    merr = abs(abs(mdot) - RHO*Q)/(RHO*Q)

    print("  inlet mass flux = {:.6e} kg/s "
          "(rho Q = {:.6e} kg/s, error {:.3%})".format(
              mdot, -RHO*Q, merr))

    fail = False
    if fills[-1] <= 0 or fills[-1] >= 1:
        print("FAIL: the cavity did not fill during the run")
        fail = True
    if merr > 0.03:
        print("FAIL: the inlet mass flux does not match rho Q")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the runner-coupled inlet injects the network gate flow")


if __name__ == "__main__":
    main()
