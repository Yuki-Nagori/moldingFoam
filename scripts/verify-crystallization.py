#!/usr/bin/env python3
#******************************************************************************
# verify-crystallization.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the crystallisation kinetics solver case:
# the melt cools through the crystallisation window and the relative
# crystallinity field must grow monotonically from 0 towards 1, staying
# bounded, while the released latent heat is fed to the energy equation.
#
# Usage: verify-crystallization.py <caseDir>   (exits non-zero on failure)
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

    chi = [float(x) for x in re.findall(r"max\(chi\) = ([-+0-9.eE]+)", log)]
    if len(chi) < 3:
        print("FAIL: not enough crystallinity samples in the log")
        sys.exit(1)

    print("  chi history: first = {:.6f}, last = {:.6f}, samples = {}".format(
        chi[0], chi[-1], len(chi)))

    fail = False
    if chi[0] > 0.05:
        print("FAIL: the first crystallinity sample is too large "
              "(the log starts after the first 50 steps)")
        fail = True
    if min(chi) < -1e-12 or max(chi) > 1 + 1e-12:
        print("FAIL: chi left the physical range [0, 1]")
        fail = True
    if chi[-1] < 0.9:
        print("FAIL: the melt did not crystallise")
        fail = True

    # Monotonic growth (allowing tiny numerical noise)
    for a, b in zip(chi, chi[1:]):
        if b < a - 1e-9:
            print("FAIL: chi decreased from {:.6f} to {:.6f}".format(a, b))
            fail = True
            break

    # Crystallinity-coupled shrinkage (task 034): with the shrinkage
    # coupling enabled the volumetric shrinkage must gain at least half
    # of the chi increment times crystallinityShrinkage (the density
    # contribution adds more for a cooling melt)
    S = [float(x) for x in re.findall(r"max\(S\) = ([-+0-9.eE]+)", log)]
    if S:
        dS = S[-1] - S[0]
        dChi = chi[-1] - chi[0]
        expected = 0.02*dChi

        print("  shrinkage max(S): first = {:.6f}, last = {:.6f} "
              "(dS = {:.6f}, crystallinity part = {:.6f})".format(
                  S[0], S[-1], dS, expected))

        if dS < 0.5*expected:
            print("FAIL: the shrinkage did not gain the crystallinity "
                  "contribution")
            fail = True
    else:
        print("FAIL: no shrinkage samples in the log")
        fail = True

    if fail:
        sys.exit(1)


    # Latent heat must retard the sealed cooling (task 044, ablation
    # calibrated): with latentHeat 2e5 J/kg the volume-averaged melt
    # temperature ends near 429 K, while removing the latent heat lets the
    # case cool to 380 K. The threshold sits between the two with ~20 K
    # margin on each side (the signal is ~49 K wide).
    import glob as _glob
    fns = sorted(_glob.glob(os.path.join(
        case_dir, "postProcessing", "meltTemperature", "*", "*.dat")))
    if not fns:
        print("FAIL: no melt-temperature samples (the case must log the "
              "volAverage(T) function object)")
        sys.exit(1)
    series = []
    for fn in fns:
        for line in open(fn, errors="replace"):
            if line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                series.append(float(parts[1]))
    if not series:
        print("FAIL: the melt-temperature series is empty")
        sys.exit(1)
    Tfinal = series[-1]
    print("  final melt vol-average T = {:.2f} K (latent-heat threshold "
          "400 K)".format(Tfinal))
    if Tfinal < 400.0:
        print("FAIL: the melt cooled through the latent-heat plateau "
              "(the latent heat is not active)")
        sys.exit(1)

    print("PASS: crystallinity grows monotonically and bounded to "
          "near 1; the coupled shrinkage gains the crystallinity part")


if __name__ == "__main__":
    main()
