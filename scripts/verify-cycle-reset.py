#!/usr/bin/env python3
#******************************************************************************
# verify-cycle-reset.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the cavity cycle reset (tasks 006/012/044):
# the reset restores the cavity state while the mould temperature is kept,
# so the cavity density must return to the EOS density of the initial
# (p, T) state and the melt fraction to one; the ejected cycles must be
# counted as configured. Numeric companion of the expectedPatterns of
# tests/cases/cycleReset (task 044, gap G8).
#
# Usage: verify-cycle-reset.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import math
import os
import re
import sys

RHO_TOL = 1e-3


def read_scalar(case_dir, time, name):
    with open(os.path.join(case_dir, time, name), errors="replace") as f:
        txt = f.read()
    m = re.search(
        r"internalField\s+nonuniform\s+List<scalar>\s*\n\s*\d+\s*\n"
        r"\((.*?)\)\s*;", txt, re.S)
    if m is not None:
        return [float(x) for x in re.findall(r"[-+0-9.eE]+", m.group(1))]
    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
    return [float(m.group(1))] if m is not None else None


def uniform(case_dir, time, name):
    with open(os.path.join(case_dir, time, name), errors="replace") as f:
        txt = f.read()
    m = re.search(r"internalField\s+uniform\s+([-+0-9.eE]+)", txt)
    return float(m.group(1)) if m else None


def rho_tait(p, T, c):
    v0 = c["b1m"] + c["b2m"]*(T - c["b5"])
    B = c["b3"]*math.exp(-c["b4"]*T)
    return 1.0/(v0*(1 - c["C"]*math.log(1 + p/B)))


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    nCycles = None
    with open(os.path.join(case_dir, "constant", "moldingDict"),
              errors="replace") as f:
        md = f.read()
    m = re.search(r"^\s*nCycles\s+(\d+)\s*;", md, re.M)
    if m:
        nCycles = int(m.group(1))

    ejections = len(re.findall(r"ejection criterion met:", log))
    resets = re.findall(r"cycle reset: min\(rho\) = ([-+0-9.eE]+)", log)

    print("  ejections = {}, resets = {}".format(ejections, len(resets)))
    if nCycles is not None and ejections != nCycles:
        print("FAIL: expected {} cycles, counted {}".format(nCycles, ejections))
        sys.exit(1)
    if not resets:
        print("FAIL: no cycle reset happened")
        sys.exit(1)

    # Tait coefficients and the initial state
    with open(os.path.join(case_dir, "constant", "physicalProperties.melt"),
              errors="replace") as f:
        melt = f.read()

    def coef(name):
        return float(re.search(r"\b" + name + r"\s+([-+0-9.eE]+)\s*;",
                               melt).group(1))

    c = {k: coef(k) for k in ("b1m", "b2m", "b3", "b4", "b5", "C")}
    T0 = uniform(case_dir, "0", "T")
    p0 = uniform(case_dir, "0", "p")
    rhoRef = rho_tait(p0, T0, c)

    # the reset must restore the cavity density to the EOS value
    worst = 0.0
    for r in resets:
        worst = max(worst, abs(float(r) - rhoRef)/rhoRef)
    print("  reset min(rho): {} vs EOS rho(p0={:g}, T0={:g}) = {:.5f} "
          "(max relative {:.2e})".format(
              ", ".join("{:.3f}".format(float(x)) for x in resets),
              p0, T0, rhoRef, worst))
    if worst > RHO_TOL:
        print("FAIL: the cycle reset did not restore the cavity density")
        sys.exit(1)

    # the written state must show the restored cavity (melt fraction one,
    # density at the initial EOS value)
    times = sorted((d for d in os.listdir(case_dir)
                    if re.match(r"^[0-9]+(\.[0-9]+)?$", d)
                    and float(d) > 0), key=float)
    if times:
        a = read_scalar(case_dir, times[-1], "alpha.melt")
        rho = read_scalar(case_dir, times[-1], "rho")
        if a:
            aMean = sum(a)/len(a)
            print("  final mean alpha.melt = {:.6f}".format(aMean))
            if aMean < 1 - 1e-3:
                print("FAIL: the cavity was not refilled to a full melt "
                      "fraction")
                sys.exit(1)
        if rho:
            rMean = sum(rho)/len(rho)
            dev = abs(rMean - rhoRef)/rhoRef
            print("  final mean rho = {:.5f} (deviation {:.2e})"
                  .format(rMean, dev))
            if dev > RHO_TOL:
                print("FAIL: the final cavity density is not the restored "
                      "EOS density")
                sys.exit(1)

    print("PASS: the cavity resets to the initial EOS state while the "
          "cycles are counted as configured")


if __name__ == "__main__":
    main()
