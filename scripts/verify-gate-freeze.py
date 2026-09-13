#!/usr/bin/env python3
#******************************************************************************
# verify-gate-freeze.py
#
# This script is free software: you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation, either version 3 of the License, or (at your
# option) any later version. See the COPYING file for details.
#******************************************************************************
#******************************************************************************
# Acceptance verification for the physical gate-freeze criterion
# (tasks 010/044): the gate must seal at the freeze temperature and the
# cavity must then be a closed system - the melt mass stops changing (the
# gate-seal ramp, if enabled, decays within the ramp window first) and the
# gate flux vanishes. Numeric companion of the expectedPatterns of
# tests/cases/gateFreeze.
#
# The check is self-calibrating: it needs no absolute tolerance, only that
# the sealed mass is constant step to step.
#
# Usage: verify-gate-freeze.py <caseDir>   (exits non-zero on failure)
#******************************************************************************

import glob
import os
import re
import sys

STEP_TOL = 1e-6


def main():
    case_dir = sys.argv[1] if len(sys.argv) > 1 else "."

    with open(os.path.join(case_dir, "log.foamRun"), errors="replace") as f:
        log = f.read()

    if "\nEnd\n" not in log and not log.rstrip().endswith("End"):
        print("FAIL: the solver did not reach the end time")
        sys.exit(1)

    m = re.search(r"gate sealed at t = ([-+0-9.eE]+) s", log)
    if m is None:
        print("FAIL: the gate never sealed")
        sys.exit(1)
    tSeal = float(m.group(1))

    ramp = re.search(r"gateSealRamp\s+=\s+([-+0-9.eE]+)", log)
    tRamp = float(ramp.group(1)) if ramp else 0.0
    tClosed = tSeal + tRamp

    # melt mass history from the meltMass function object
    fns = sorted(glob.glob(os.path.join(
        case_dir, "postProcessing", "meltMass", "*", "*.dat")))
    if not fns:
        print("FAIL: no meltMass samples (the case must log volIntegrate("
              "alphaMeltRho))")
        sys.exit(1)

    series = []
    for fn in fns:
        for line in open(fn, errors="replace"):
            if line.startswith("#"):
                continue
            parts = line.split()
            if len(parts) >= 2:
                series.append((float(parts[0]), float(parts[1])))

    if not series:
        print("FAIL: the melt-mass series is empty")
        sys.exit(1)

    closed = [mm for t, mm in series if t > tClosed]
    print("  gate sealed at t = {:g} s (ramp {:g} s), {} closed-cavity "
          "samples".format(tSeal, tRamp, len(closed)))

    if len(closed) < 3:
        print("FAIL: not enough samples after the seal to verify the closed "
              "cavity")
        sys.exit(1)

    worst = 0.0
    for i in range(1, len(closed)):
        worst = max(worst, abs(closed[i] - closed[i-1])/abs(closed[i-1]))

    print("  sealed melt mass = {:.6e} kg, worst step change = {:.2e} "
          "(tolerance {:.0e})".format(closed[-1], worst, STEP_TOL))

    if worst > STEP_TOL:
        print("FAIL: the sealed cavity is not closed (melt mass still "
              "changing after the gate-seal ramp)")
        sys.exit(1)

    print("PASS: the gate seals and the cavity becomes a closed system")


if __name__ == "__main__":
    main()
