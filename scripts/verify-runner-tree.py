#!/usr/bin/env python3
#******************************************************************************
# verify-runner-tree.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the tree runner topology (task 058): two gates
# hang off *different* manifolds, so each path is its own manifold plus gate
# (a manifold shared by both gates would be common to the two paths and would
# cancel from the split ratio, making the check unable to tell the tree from
# the flat network). For a power law with a common index n a path adds
# a = L/D^(3n+1) and parallel branches split as Qi/Qj = (a_j/a_i)^(1/n), so
#
#     Q1/Q2 = ((a_m2 + a_gate2)/(a_m1 + a_gate1))^(1/n)
#
# which is ~15.3 here - clearly different from the flat two-gate case (32),
# so the check distinguishes the tree path from the legacy one.
#
# Usage: verify-runner-tree.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import os
import re
import sys

RHO = 746.3            # melt density at 480 K, 1e5 Pa [kg/m^3]
QTOT = 2e-07           # total runner flow rate [m^3/s]
N = 0.5                # power-law index


def aPath(L, D, n):
    """Power-law path coefficient L/D^(3n+1)"""
    return L/D**(3*n + 1)


A_P1 = aPath(0.03, 0.004, N) + aPath(0.02, 0.004, N)   # m1 + gate1
A_P2 = aPath(0.04, 0.003, N) + aPath(0.02, 0.002, N)   # m2 + gate2
RATIO = (A_P2/A_P1)**(1/N)


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

    print("  tree gate flows = {:.4e} / {:.4e} m^3/s, ratio = {:.3f} "
          "(analytic {:.2f}, error {:.3%})".format(
              q1, q2, ratio, RATIO, rerr))
    print("  total flow error = {:.3%}".format(serr))

    fail = False
    if rerr > 0.05:
        print("FAIL: the tree flow split does not match the manifold + gate "
              "resistance ratio")
        fail = True
    if serr > 0.05:
        print("FAIL: the total flow does not match totalFlowRate")
        fail = True

    if fail:
        sys.exit(1)

    print("PASS: the tree runner splits the flow by the manifold + gate "
          "resistance ratio")


if __name__ == "__main__":
    main()
