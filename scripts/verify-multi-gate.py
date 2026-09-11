#!/usr/bin/env python3
#******************************************************************************
# verify-multi-gate.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the multi-gate runner split: two gates fed
# from one runner network (the same total flow rate, gate diameters 4 and
# 2 mm) must split the flow by the analytic resistance ratio. For a power
# law with a common index n, Qi/Qj = (Dj/Di)^(3 + 1/n).
#
# Usage: verify-multi-gate.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

RHO = 746.3            # melt density at 480 K, 1e5 Pa [kg/m^3]
QTOT = 2e-07           # total runner flow rate [m^3/s]
N = 0.5                # power-law index
RATIO = (0.004/0.002)**(3 + 1/N)   # analytic Q1/Q2


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    m1 = [float(x) for x in re.findall(
        r"sum\(inlet1\) of alphaRhoPhi\.melt = ([-+0-9.eE]+)", log)
        if abs(float(x)) > 0]
    m2 = [float(x) for x in re.findall(
        r"sum\(inlet2\) of alphaRhoPhi\.melt = ([-+0-9.eE]+)", log)
        if abs(float(x)) > 0]

    if not m1 or not m2:
        print("FAIL: no mass flux samples for one of the gates")
        sys.exit(1)

    # Settled samples (the flow profile is constant)
    f1 = sum(m1[-20:])/len(m1[-20:])
    f2 = sum(m2[-20:])/len(m2[-20:])
    q1 = abs(f1)/RHO
    q2 = abs(f2)/RHO

    ratio = q1/q2
    rerr = abs(ratio - RATIO)/RATIO
    serr = abs(q1 + q2 - QTOT)/QTOT

    print("  gate flows = {:.4e} / {:.4e} m^3/s, ratio = {:.3f} "
          "(analytic {:.1f}, error {:.3%})".format(
              q1, q2, ratio, RATIO, rerr))
    print("  total flow error = {:.3%}".format(serr))

    fail = False
    if rerr > 0.05:
        print("FAIL: the multi-gate flow split does not match the "
              "resistance ratio")
        fail = True
    if serr > 0.05:
        print("FAIL: the total flow does not match totalFlowRate")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the runner network splits the flow by the analytic "
          "resistance ratio")


if __name__ == "__main__":
    main()
